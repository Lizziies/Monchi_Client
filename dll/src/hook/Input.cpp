#include "Input.hpp"
#include "WindowChain.hpp"
#include "Hook.hpp"
#include "GameInput.hpp"
#include "Dx.hpp"
#include "core/Client.hpp"
#include "core/Guard.hpp"
#include "system/Mice.hpp"
#include "system/Tweaks.hpp"
#include "core/Log.hpp"
#include "modules/Manager.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "render/Ui.hpp"
#include "sdk/Inject.hpp"

#include <imgui.h>

#include <windowsx.h>

#include <array>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>

namespace input {

static std::atomic<bool> focusLost{false};
static std::atomic<int> sampled{-1};
static std::atomic<int> gameScreen{0};
static HWND target = nullptr;
static WNDPROC original = nullptr;
static std::array<std::atomic<bool>, 256> keys{};
static std::array<std::atomic<bool>, 2> realCtrl{};
static std::mutex clickLock;
static std::deque<int64_t> clicks[2];
static std::atomic<int64_t> lastClick{0};
static std::atomic<int64_t> lastMove{0};
static std::atomic<int> motionX{0}, motionY{0};
static bool rawButtons = false;
static LARGE_INTEGER qpf{};

using ClipCursorFn = BOOL(WINAPI*)(const RECT*);
using SetCursorPosFn = BOOL(WINAPI*)(int, int);
using GetCursorPosFn = BOOL(WINAPI*)(LPPOINT);
using KeyStateFn = SHORT(WINAPI*)(int);
using RawDataFn = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
using RawBufferFn = UINT(WINAPI*)(PRAWINPUT, PUINT, UINT);
using PeekFn = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
static ClipCursorFn oClipCursor = nullptr;
static SetCursorPosFn oSetCursorPos = nullptr;
static GetCursorPosFn oGetCursorPos = nullptr;
static KeyStateFn oGetAsyncKeyState = nullptr;
static KeyStateFn oGetKeyState = nullptr;
static RawDataFn oGetRawInputData = nullptr;
static RawBufferFn oGetRawInputBuffer = nullptr;
static PeekFn oPeekMessage = nullptr;

static thread_local int oursDepth = 0;
static POINT frozen{};
static bool wasBlocking = false;
static std::atomic<int64_t> hiddenSince{0};
static std::atomic<int64_t> lastPlaying{0};
static std::atomic<bool> playing{false};
static std::atomic<int> spy[8];
static int64_t spyAt = 0;

Ours::Ours() { oursDepth++; }
Ours::~Ours() { oursDepth--; }

static bool blocking() { return oursDepth == 0 && ui::capturing(); }

static int64_t qpc() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static void trimClicks(std::deque<int64_t>& d, int64_t t) {
    while (!d.empty() && t - d.front() > qpf.QuadPart) d.pop_front();
}

static void recordClick(int idx, int64_t t) {
    std::scoped_lock g(clickLock);
    clicks[idx].push_back(t);
    trimClicks(clicks[idx], t);
    lastClick = t;
}

static bool dispatchMouse(MouseEvent ev) {
    if (ev.button == MouseButton::Left) keys[VK_LBUTTON] = ev.down;
    if (ev.button == MouseButton::Right) keys[VK_RBUTTON] = ev.down;
    if (ev.button == MouseButton::Middle) keys[VK_MBUTTON] = ev.down;
    if (ev.button == MouseButton::X1) keys[VK_XBUTTON1] = ev.down;
    if (ev.button == MouseButton::X2) keys[VK_XBUTTON2] = ev.down;

    modules::dispatchMouse(ev);
    if (ev.cancel) return true;
    if (ev.down && (ev.button == MouseButton::Left || ev.button == MouseButton::Right))
        recordClick(ev.button == MouseButton::Left ? 0 : 1, ev.qpc);
    return false;
}

static bool rawPass = false;
static std::array<bool, 3> rawEaten{};

static bool handleRaw(LPARAM lp) {
    UINT size = 0;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    if (size == 0 || size > 1024) return false;

    alignas(8) BYTE buf[1024];
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size)
        return false;

    auto* raw = reinterpret_cast<RAWINPUT*>(buf);
    if (raw->header.dwType != RIM_TYPEMOUSE) return false;

    auto& m = raw->data.mouse;
    int64_t t = qpc();
    mice::onRaw(raw->header.hDevice, m.lLastX, m.lLastY, t);
    bool cancel = false;

    if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE) && (m.lLastX || m.lLastY)) {
        motionX += m.lLastX;
        motionY += m.lLastY;
        lastMove = t;
    }

    struct Map {
        USHORT down, up;
        MouseButton button;
    };
    static constexpr Map map[] = {
        {RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, MouseButton::Left},
        {RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, MouseButton::Right},
        {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, MouseButton::Middle},
    };
    // the game asks for raw mouse input, so the menu gets its clicks and wheel from here as well
    bool menu = ui::capturing();
    auto feed = [&](MouseButton b, bool down) {
        if (menu) ui::mouseButton(b == MouseButton::Left ? 0 : b == MouseButton::Right ? 1 : 2, down);
    };
    rawPass = false;
    for (auto& e : map) {
        int idx = e.button == MouseButton::Left ? 0 : e.button == MouseButton::Right ? 1 : 2;
        if (m.usButtonFlags & e.down) {
            rawButtons = true;
            if (menu) rawEaten[idx] = true;
            feed(e.button, true);
            cancel |= dispatchMouse({e.button, true, 0, 0, 0, t});
        }
        if (m.usButtonFlags & e.up) {
            // a button the game saw go down has to see it come up, or it keeps attacking behind the menu
            if (menu && !rawEaten[idx]) rawPass = true;
            rawEaten[idx] = false;
            feed(e.button, false);
            cancel |= dispatchMouse({e.button, false, 0, 0, 0, t});
        }
    }
    if (m.usButtonFlags & RI_MOUSE_WHEEL) {
        if (menu) ui::mouseWheel(static_cast<short>(m.usButtonData) / 120.f);
        MouseEvent ev{MouseButton::None, false, (short)m.usButtonData, 0, 0, t};
        modules::dispatchMouse(ev);
        cancel |= ev.cancel;
    }
    return cancel;
}

static bool isPointerMessage(UINT msg) { return msg >= 0x0240 && msg <= 0x0253; }

static bool isMouseMessage(UINT msg) {
    return (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_INPUT || isPointerMessage(msg);
}

static bool isKeyMessage(UINT msg) {
    return msg >= WM_KEYFIRST && msg <= WM_KEYLAST;
}

static int resolveVk(WPARAM wp, LPARAM lp) {
    int vk = (int)wp;
    UINT scan = (lp >> 16) & 0xFF;
    bool extended = (lp >> 24) & 1;
    if (vk == VK_SHIFT) return (int)MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);
    if (vk == VK_CONTROL) return extended ? VK_RCONTROL : VK_LCONTROL;
    if (vk == VK_MENU) return extended ? VK_RMENU : VK_LMENU;
    return vk;
}

static std::array<bool, 259> eaten{};

static int eatSlot(UINT msg, WPARAM wp, LPARAM lp, bool& down) {
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        down = true;
        return resolveVk(wp, lp) & 0xFF;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        return resolveVk(wp, lp) & 0xFF;
    case WM_LBUTTONDOWN: down = true; return 256;
    case WM_RBUTTONDOWN: down = true; return 257;
    case WM_MBUTTONDOWN: down = true; return 258;
    case WM_LBUTTONUP: return 256;
    case WM_RBUTTONUP: return 257;
    case WM_MBUTTONUP: return 258;
    default: return -1;
    }
}

static bool process(HWND w, UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
    if (client::unloading()) return false;

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        bool isDown = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
        int vk = resolveVk(wp, lp);
        bool repeat = isDown && ((lp >> 30) & 1);
        keys[vk & 0xFF] = isDown;
        if (vk != (int)wp) keys[wp & 0xFF] = isDown;

        if (!inject::ours() && (vk == VK_LCONTROL || vk == VK_RCONTROL)) realCtrl[vk == VK_RCONTROL] = isDown;
        // AltGr arrives as left Ctrl plus right Alt; typing with it must not unload the client
        if (isDown && !repeat && vk == 'L' && (realCtrl[0] || realCtrl[1]) && !keys[VK_RMENU] && !inject::ours()) {
            client::requestUnload();
            return true;
        }

        KeyEvent ev{vk, isDown, repeat};
        modules::dispatchKey(ev);
        if (!isDown) gameinput::cancelKey(vk, false);
        else if (ev.cancel) gameinput::cancelKey(vk, true);
        if (ev.cancel) return true;
        if (!ui::capturing() && flarialModules::key(w, msg, wp, lp)) return true;
    }

    int64_t t = qpc();
    switch (msg) {
    case WM_INPUT:
        if (handleRaw(lp)) {
            result = DefWindowProcW(w, msg, wp, lp);
            return true;
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (!rawButtons && dispatchMouse({msg == WM_LBUTTONDOWN ? MouseButton::Left : MouseButton::Right, true, 0, 0, 0, t}))
            return true;
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        if (!rawButtons && dispatchMouse({msg == WM_LBUTTONUP ? MouseButton::Left : MouseButton::Right, false, 0, 0, 0, t}))
            return true;
        break;
    case WM_MOUSEWHEEL: {
        if (rawButtons) break;
        MouseEvent ev{MouseButton::None, false, GET_WHEEL_DELTA_WPARAM(wp), 0, 0, t};
        modules::dispatchMouse(ev);
        if (ev.cancel) return true;
        break;
    }
    case WM_SETFOCUS:
        sampled.fetch_or(1);
        break;
    case WM_KILLFOCUS:
        sampled.fetch_and(~1);
        focusLost = true;
        flarialModules::key(w, msg, wp, lp);
        // the release of a held key goes to the window that takes the focus; the modules get it from here
        for (int vk = 8; vk < 256; ++vk) {
            if (!keys[vk]) continue;
            KeyEvent up{vk, false};
            modules::dispatchKey(up);
        }
        for (auto& k : keys) k = false;
        eaten.fill(false);
        for (int vk = 0; vk < 256; ++vk) gameinput::cancelKey(vk, false);
        realCtrl[0] = realCtrl[1] = false;
        break;
    default:
        break;
    }

    bool take = ui::wndProc(w, msg, wp, lp);
    bool down = false;
    if (int slot = eatSlot(msg, wp, lp, down); slot >= 0) {
        if (take && down) eaten[slot] = true;
        else if (!down && eaten[slot]) {
            eaten[slot] = false;
            take = true;
        } else if (!down) {
            take = false;
        }
    }
    if (msg == WM_INPUT && rawPass) take = false;
    if (inject::ours()) take = false;
    if (take && (isMouseMessage(msg) || isKeyMessage(msg))) {
        result = msg == WM_INPUT || isPointerMessage(msg) ? DefWindowProcW(w, msg, wp, lp) : 0;
        return true;
    }
    return false;
}

static LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (client::unloading()) return CallWindowProcW(original, w, msg, wp, lp);
    tweaks::threadBoost(tweaks::wantsInputBoost());
    LRESULT result = 0;
    bool handled = false;
    guard::call("wndproc", [&] {
        Ours ours;
        handled = process(w, msg, wp, lp, result);
    });
    if (handled) return result;
    return CallWindowProcW(original, w, msg, wp, lp);
}

static RECT lastClip{};
static bool haveClip = false;
static bool clipReleased = false;

static BOOL WINAPI clipCursor(const RECT* r) {
    haveClip = r != nullptr;
    if (r) lastClip = *r;
    if (ui::wantsCursor()) return oClipCursor(nullptr);
    return oClipCursor(r);
}

static void releaseButton(int vk, DWORD flag) {
    if (!keys[vk]) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flag;
    in.mi.dwExtraInfo = 0x4D4F4348;
    SendInput(1, &in, sizeof(in));
}

void releaseHeld() {
    if (!inject::focused()) return;
    releaseButton(VK_LBUTTON, MOUSEEVENTF_LEFTUP);
    releaseButton(VK_RBUTTON, MOUSEEVENTF_RIGHTUP);
    releaseButton(VK_MBUTTON, MOUSEEVENTF_MIDDLEUP);
    for (int vk = 8; vk < 255; vk++) {
        if (vk == VK_RSHIFT || vk <= VK_XBUTTON2 || !keys[vk]) continue;
        inject::key(vk, false);
        eaten[vk] = true;
    }
}

void syncCursor(bool menuOpen) {
    if (!oClipCursor) return;
    if (menuOpen) {
        oClipCursor(nullptr);
        clipReleased = true;
    } else if (clipReleased) {
        clipReleased = false;
        oClipCursor(haveClip ? &lastClip : nullptr);
    }
}

static BOOL WINAPI setCursorPos(int x, int y) {
    if (ui::wantsCursor()) return TRUE;
    return oSetCursorPos(x, y);
}

static BOOL WINAPI getCursorPos(LPPOINT p) {
    if (!p || !blocking()) {
        wasBlocking = false;
        return oGetCursorPos(p);
    }
    spy[0]++;
    if (!wasBlocking) {
        wasBlocking = true;
        if (!oGetCursorPos(&frozen)) frozen = {};
    }
    *p = frozen;
    return TRUE;
}

static SHORT WINAPI getAsyncKeyState(int vk) {
    if (!blocking()) return oGetAsyncKeyState(vk);
    spy[1]++;
    return 0;
}

static SHORT WINAPI getKeyState(int vk) {
    if (!blocking()) return oGetKeyState(vk);
    spy[2]++;
    return 0;
}

static UINT WINAPI getRawInputData(HRAWINPUT h, UINT cmd, LPVOID data, PUINT size, UINT header) {
    UINT r = oGetRawInputData(h, cmd, data, size, header);
    if (!data || r == UINT(-1) || cmd != RID_INPUT || !blocking()) return r;
    spy[3]++;
    auto* raw = static_cast<RAWINPUT*>(data);
    if (raw->header.dwType == RIM_TYPEMOUSE) {
        raw->data.mouse.lLastX = raw->data.mouse.lLastY = 0;
        raw->data.mouse.usButtonFlags &= RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_RIGHT_BUTTON_UP | RI_MOUSE_MIDDLE_BUTTON_UP |
                                         RI_MOUSE_BUTTON_4_UP | RI_MOUSE_BUTTON_5_UP;
        raw->data.mouse.usButtonData = 0;
    } else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        raw->data.keyboard.VKey = 0xFF;
        raw->data.keyboard.MakeCode = 0;
        raw->data.keyboard.Flags |= RI_KEY_BREAK;
    }
    return r;
}

static UINT WINAPI getRawInputBuffer(PRAWINPUT data, PUINT size, UINT header) {
    UINT r = oGetRawInputBuffer(data, size, header);
    if (!data || r == 0 || r == UINT(-1) || !blocking()) return r;
    spy[4]++;
    return 0;
}

static bool pumpedInput(const MSG& m) {
    return (m.message >= WM_KEYFIRST && m.message <= WM_KEYLAST) || isMouseMessage(m.message);
}

static void noteStray(const MSG& m) {
    static std::mutex lock;
    static std::vector<std::pair<HWND, UINT>> seen;
    std::scoped_lock g(lock);
    for (auto& s : seen)
        if (s.first == m.hwnd && s.second == m.message) return;
    if (seen.size() > 64) return;
    seen.push_back({m.hwnd, m.message});
    wchar_t cls[64]{};
    GetClassNameW(m.hwnd, cls, 64);
    logger::info("pump: input message 0x{:04X} for window {} class '{}' (game window {})", m.message,
                 (void*)m.hwnd, logger::narrow(cls), (void*)target);
}

static void swallow(MSG* m, int slot) {
    if (m && ui::capturing() && !oursDepth && pumpedInput(*m) && m->hwnd != target) noteStray(*m);
    if (!m || m->hwnd != target || !pumpedInput(*m) || oursDepth) return;
    bool down = false;
    int eat = eatSlot(m->message, m->wParam, m->lParam, down);
    if (!ui::capturing() && !(eat >= 0 && !down && eaten[eat])) return;
    LRESULT result = 0;
    bool handled = false;
    guard::call("pump", [&] {
        Ours ours;
        handled = process(m->hwnd, m->message, m->wParam, m->lParam, result);
    });
    if (!handled) return;
    spy[slot]++;
    // the game never sees this key, so it never calls TranslateMessage on it; without that no WM_CHAR is posted
    // and the menu's text fields get no letters
    if (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN) TranslateMessage(m);
    if (isPointerMessage(m->message)) {
        spy[7]++;
        DefWindowProcW(m->hwnd, m->message, m->wParam, m->lParam);
    }
    m->message = WM_NULL;
}

static BOOL WINAPI peekMessage(LPMSG m, HWND w, UINT lo, UINT hi, UINT remove) {
    BOOL r = oPeekMessage(m, w, lo, hi, remove);
    if (r && (remove & PM_REMOVE)) swallow(m, 5);
    return r;
}

static void flushSpy(int64_t now) {
    if (now - spyAt < qpf.QuadPart) return;
    spyAt = now;
    int n[8];
    int total = 0;
    for (int i = 0; i < 8; i++) total += n[i] = spy[i].exchange(0);
    if (total)
        logger::info("menu open, game polled: GetCursorPos={} GetAsyncKeyState={} GetKeyState={} RawData={} RawBuffer={} Peek={} Pointer={}",
                     n[0], n[1], n[2], n[3], n[4], n[5], n[7]);
}

bool inMinecraft() {
    static const bool yes = [] {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring exe = path;
        exe = exe.substr(exe.find_last_of(L"\\/") + 1);
        return exe.starts_with(L"Minecraft");
    }();
    return yes;
}

// Focus and cursor state are taken from Windows once per frame (sample) and answered from there; before the first
// frame the answer is fetched on the spot.
static int readState() {
    DWORD owner = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &owner);
    CURSORINFO ci{sizeof(ci)};
    bool shown = GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING);
    return (owner == GetCurrentProcessId() ? 1 : 0) | (shown ? 2 : 0);
}

void sample() {
    static int64_t last = 0;
    int64_t now = qpc();
    if (sampled.load(std::memory_order_relaxed) >= 0 && now - last < qpf.QuadPart / 125) return;
    last = now;
    sampled = readState();
}

static int state() {
    int s = sampled.load(std::memory_order_relaxed);
    return s >= 0 ? s : readState();
}

static bool focusedHere() { return (state() & 1) != 0; }

bool cursorShown() { return (state() & 2) != 0; }

bool takeFocusLost() { return focusLost.exchange(false); }

bool focused() { return !inMinecraft() || focusedHere(); }

bool grabbed() {
    if (!inMinecraft()) return true;
    int s = state();
    if (!(s & 1)) return false;
    if (int screen = gameScreen.load()) return screen == 1;
    return !(s & 2);
}

void screenHint(int screen) { gameScreen.store(screen); }
int screen() { return gameScreen.load(); }

// Without game data the cursor is the best hint: hidden means the player is in a world. Menus inside a world
// (pause, inventory, chat, settings) show it, so the HUD stays for 15 minutes after the cursor appears instead
// of blinking off and on, and alt-tab keeps whatever was there. Only the very first entry waits a moment, so
// loading screens with a hidden cursor don't count. Leaving a server ends it at once (forgetPlay).
bool gameplay() {
    int64_t now = qpc();
    flushSpy(now);
    if (!inMinecraft()) return true;

    int64_t sec = qpf.QuadPart;
    int64_t last = lastPlaying;
    if (!focusedHere()) {
        hiddenSince = 0;
        return playing;
    }
    bool hidden = !cursorShown();
    bool recent = last && now - last < sec * 900;
    if (!hidden) {
        hiddenSince = 0;
    } else {
        if (!hiddenSince) hiddenSince = now;
        if (recent || now - hiddenSince > sec * 12 / 10) lastPlaying = last = now;
    }

    bool on = last && now - last < sec * 900;
    if (on != playing.exchange(on)) logger::info("gameplay heuristic: {} (cursor {})", on ? "in game" : "menus", hidden ? "hidden" : "visible");
    return on;
}

void forgetPlay() {
    lastPlaying = 0;
    hiddenSince = 0;
}

static void logInputImports() {
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return;

    static const char* words[] = {"Raw", "Cursor", "Mouse", "Keyboard", "Input", "Pointer", "Hid", "GetKey", "Capture", "Gamepad"};
    std::string out;
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; d++) {
        std::string dll = reinterpret_cast<const char*>(base + d->Name);
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (; thunk->u1.AddressOfData; thunk++) {
            if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) continue;
            std::string fn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + thunk->u1.AddressOfData)->Name;
            for (auto* w : words)
                if (fn.find(w) != std::string::npos) {
                    out += dll + "!" + fn + " ";
                    break;
                }
        }
    }
    logger::info("game imports (input related): {}", out);
}

bool install(HWND window) {
    QueryPerformanceFrequency(&qpf);
    guard::call("imports", logInputImports);
    target = window;
    original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc)));
    if (!original) {
        logger::error("wndproc subclass failed: {}", GetLastError());
        return false;
    }

    hook::create("ClipCursor", hook::exported(L"user32.dll", "ClipCursor"), clipCursor, &oClipCursor);
    hook::create("SetCursorPos", hook::exported(L"user32.dll", "SetCursorPos"), setCursorPos, &oSetCursorPos);
    hook::create("GetCursorPos", hook::exported(L"user32.dll", "GetCursorPos"), getCursorPos, &oGetCursorPos);
    hook::create("GetAsyncKeyState", hook::exported(L"user32.dll", "GetAsyncKeyState"), getAsyncKeyState, &oGetAsyncKeyState);
    hook::create("GetKeyState", hook::exported(L"user32.dll", "GetKeyState"), getKeyState, &oGetKeyState);
    hook::create("GetRawInputData", hook::exported(L"user32.dll", "GetRawInputData"), getRawInputData, &oGetRawInputData);
    hook::create("GetRawInputBuffer", hook::exported(L"user32.dll", "GetRawInputBuffer"), getRawInputBuffer, &oGetRawInputBuffer);
    hook::create("PeekMessageW", hook::exported(L"user32.dll", "PeekMessageW"), peekMessage, &oPeekMessage);
    // GetMessageW is left alone on purpose: a game thread blocks inside it, and after Ctrl+L that thread would
    // return into the unloaded DLL. The game's own window loop uses PeekMessageW.
    hook::enableAll();
    return true;
}

bool uninstall() {
    return detachWindow(target, proc, original);
}

void reconcile() {
    static std::array<unsigned char, 256> gone{};
    HWND w = dx::window();
    if (!w || GetForegroundWindow() != w) return;
    for (int vk = 1; vk < 256; ++vk) {
        if (!keys[vk]) {
            gone[size_t(vk)] = 0;
            continue;
        }
        bool physical = ((oGetAsyncKeyState ? oGetAsyncKeyState(vk) : GetAsyncKeyState(vk)) & 0x8000) != 0;
        if (physical) gone[size_t(vk)] = 0;
        else if (++gone[size_t(vk)] >= 3) {
            gone[size_t(vk)] = 0;
            keys[vk] = false;
            // the release goes through the window procedure, where the modules are told on the thread they expect
            if (vk == VK_LBUTTON) PostMessageW(w, WM_LBUTTONUP, 0, 0);
            else if (vk == VK_RBUTTON) PostMessageW(w, WM_RBUTTONUP, 0, 0);
            else if (vk == VK_MBUTTON) PostMessageW(w, WM_MBUTTONUP, 0, 0);
            else if (vk > VK_XBUTTON2) {
                UINT scan = MapVirtualKeyW(UINT(vk), MAPVK_VK_TO_VSC);
                PostMessageW(w, WM_KEYUP, WPARAM(vk), LPARAM(1 | (scan << 16) | (1u << 30) | (1u << 31)));
            }
            static int told = 0;
            if (told++ < 5) logger::info("input: key {} had lost its release, let go", vk);
        }
    }
}

bool down(int vk) {
    if (vk == VK_CONTROL) return keys[VK_LCONTROL] || keys[VK_RCONTROL] || keys[VK_CONTROL];
    if (vk == VK_SHIFT) return keys[VK_LSHIFT] || keys[VK_RSHIFT];
    if (vk == VK_MENU) return keys[VK_LMENU] || keys[VK_RMENU];
    return keys[vk & 0xFF];
}

int cps(MouseButton button) {
    int idx = button == MouseButton::Left ? 0 : 1;
    std::scoped_lock g(clickLock);
    trimClicks(clicks[idx], qpc());
    return (int)clicks[idx].size();
}

int64_t lastClickQpc() { return lastClick; }
int64_t lastMoveQpc() { return lastMove; }

void consumeMotion(int& dx, int& dy) {
    dx = motionX.exchange(0);
    dy = motionY.exchange(0);
}

}
