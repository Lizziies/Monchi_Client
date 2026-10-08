#include "GameInput.hpp"
#include "MouseMotion.hpp"
#include "Dx.hpp"
#include "FrameStart.hpp"
#include "DropKeys.hpp"
#include "Input.hpp"
#include "Hook.hpp"
#include "core/Log.hpp"
#include "system/GpuLatency.hpp"
#include "render/Ui.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <bitset>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>

namespace gameinput {

namespace {

std::array<std::atomic<bool>, 256> cancelled{};

// Slots and layouts come from the public GameInput.h of every API version (v0 to v3). IGameInput keeps
// GetCurrentReading at slot 4 in all of them. IGameInputReading lost GetSequenceNumber and GetRawReport after
// v0, so its key and mouse getters sit two slots lower from v1 on, and v1 added the positions field and the
// absolute position pair to GameInputMouseState, which moves the wheel values back.
constexpr int currentReadingSlot = 4;
constexpr unsigned kindKeyboard = 0x10;
constexpr unsigned kindMouse = 0x20;
constexpr int maxKeys = 64;
constexpr int maxHooks = 12;

struct KeyState {
    uint32_t scanCode;
    uint32_t codePoint;
    uint8_t virtualKey;
    bool isDeadKey;
};

struct Layout {
    int count, keys, mouse, device, wheel;
};

constexpr Layout layoutV0{14, 15, 16, 6, 24};
constexpr Layout layoutV1{12, 13, 14, 5, 40};

struct Api {
    const char* name;
    GUID iid;
    const Layout* layout;
};

const Api apis[] = {
    {"v3", {0x20EFC1C7, 0x5D9A, 0x43BA, {0xB2, 0x6F, 0xB8, 0x07, 0xFA, 0x48, 0x60, 0x9C}}, &layoutV1},
    {"v2", {0xBBAA66D2, 0x837A, 0x40F7, {0xA3, 0x03, 0x91, 0x7D, 0x50, 0x09, 0x55, 0xF4}}, &layoutV1},
    {"v1", {0x40FFB7E4, 0x6150, 0x407A, {0xB4, 0x39, 0x13, 0x2B, 0xAD, 0xC0, 0x8D, 0x2D}}, &layoutV1},
    {"v0", {0x11BE2A7E, 0x4254, 0x445A, {0x9C, 0x09, 0xFF, 0xC4, 0x0F, 0x00, 0x69, 0x18}}, &layoutV0},
};

using InitFn = HRESULT(WINAPI*)(REFIID, void**);
using CreateFn = HRESULT(WINAPI*)(void**);
using QueryFn = HRESULT(STDMETHODCALLTYPE*)(void*, REFIID, void**);
using RefFn = ULONG(STDMETHODCALLTYPE*)(void*);
using CurrentFn = HRESULT(STDMETHODCALLTYPE*)(void*, unsigned, void*, void**);
using CountFn = uint32_t(STDMETHODCALLTYPE*)(void*);
using KeysFn = uint32_t(STDMETHODCALLTYPE*)(void*, uint32_t, KeyState*);
using MouseFn = bool(STDMETHODCALLTYPE*)(void*, uint8_t*);
using DeviceFn = void(STDMETHODCALLTYPE*)(void*, void**);

struct Known {
    void** vtable = nullptr;
    const Layout* layout = nullptr;
    bool deviceRefs = false;
};

enum class Phase { Searching, Hooked, Absent };

std::atomic<Phase> phase{Phase::Searching};
std::array<Known, 16> known{};
std::atomic<int> knownCount{0};
std::array<void*, maxHooks> originals{};
std::array<void*, maxHooks> targets{};
int hookCount = 0;
std::string summary = "searching";

thread_local int depth = 0;

struct Nest {
    Nest() { depth++; }
    ~Nest() { depth--; }
};

ULONG addRef(void* o) { return reinterpret_cast<RefFn>(hook::vfunc(o, 1))(o); }
ULONG release(void* o) { return reinterpret_cast<RefFn>(hook::vfunc(o, 2))(o); }

const Known* knownOf(void* self) {
    void** vt = *static_cast<void***>(self);
    int n = knownCount.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++)
        if (known[i].vtable == vt) return &known[i];
    return nullptr;
}

std::mutex overlayLock;
std::bitset<256> pendingHold, pendingDrop, activeHold, activeDrop, latched;
// mouse buttons a module takes out of the readings, as the bits of the mouse state
uint32_t pendingDropButtons = 0;
std::atomic<uint32_t> activeDropButtons{0};
std::array<ULONGLONG, 256> tapUntil{};
std::array<int, 2> pendingLimit{}, activeLimit{};
float pendingDebounce = 0.f;
std::atomic<int64_t> activeDebounce{0};
std::atomic<int> limitLeft{0}, limitRight{0};
std::array<float, 2> pendingScale{1.f, 1.f};
float pendingSmooth = 1.f;
bool pendingWheel = false;
std::atomic<uint64_t> activeScale{std::bit_cast<uint64_t>(std::array<float, 2>{1.f, 1.f})};
std::atomic<float> activeSmooth{1.f};
std::atomic<bool> activeWheel{false};

std::mutex keyLock;
std::bitset<256> stale;
// whether shift was in the keyboard reading and in what the game was handed of it, for the shift-click count
std::atomic<bool> rawShift{false}, passedShift{false};
std::atomic<unsigned> shiftClicks{0}, shiftClicksLost{0};
bool keysBlocked = false;
std::array<uint8_t, 512> learnedVk{};
int loggedKeys = 0;

int scanKey(uint32_t scan) { return scanIndex(scan); }

uint32_t scanOf(int vk) {
    UINT sc = MapVirtualKeyW(UINT(vk), MAPVK_VK_TO_VSC_EX);
    return sc;
}

bool blockedNow() { return ui::capturing() || !input::focused(); }

void learn(const KeyState& k) {
    int s = scanKey(k.scanCode);
    if (learnedVk[s] == k.virtualKey) return;
    learnedVk[s] = k.virtualKey;
    if (loggedKeys < 12) {
        loggedKeys++;
        logger::info("gameinput: key scan=0x{:X} vk=0x{:02X}", k.scanCode, k.virtualKey);
    }
}

uint32_t filter(KeyState* keys, uint32_t n, uint32_t cap) {
    std::scoped_lock g(keyLock);
    if (blockedNow()) {
        keysBlocked = true;
        passedShift = false;
        return 0;
    }
    std::bitset<256> present;
    for (uint32_t i = 0; i < n; i++) {
        present.set(keys[i].virtualKey);
        learn(keys[i]);
    }
    if (keysBlocked) {
        keysBlocked = false;
        stale = present;
    }
    stale &= present;

    std::bitset<256> holds, drops;
    {
        std::scoped_lock o(overlayLock);
        holds = activeHold;
        drops = activeDrop;
    }
    static DropKeys dropKeys;
    HKL layout = drops.any() ? GetKeyboardLayout(0) : nullptr;
    dropKeys.update(drops, reinterpret_cast<uintptr_t>(layout), [layout](int vk) {
        return MapVirtualKeyExW(UINT(vk), MAPVK_VK_TO_VSC_EX, layout);
    });
    uint32_t w = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (cancelled[keys[i].virtualKey] || stale[keys[i].virtualKey] || dropKeys.contains(keys[i].virtualKey, keys[i].scanCode)) continue;
        keys[w++] = keys[i];
    }
    rawShift = present[VK_SHIFT] || present[VK_LSHIFT] || present[VK_RSHIFT];
    bool shift = false;
    for (uint32_t i = 0; i < w; i++) shift |= keys[i].virtualKey == VK_SHIFT || keys[i].virtualKey == VK_LSHIFT || keys[i].virtualKey == VK_RSHIFT;
    passedShift = shift || holds[VK_SHIFT] || holds[VK_LSHIFT] || holds[VK_RSHIFT];
    if (holds.none()) return w;
    for (int vk = 0; vk < 256 && w < cap; vk++) {
        if (!holds[vk]) continue;
        uint32_t sc = scanOf(vk);
        uint8_t gameVk = learnedVk[scanKey(sc)] ? learnedVk[scanKey(sc)] : uint8_t(vk == VK_LSHIFT || vk == VK_RSHIFT ? VK_SHIFT :
                vk == VK_LCONTROL || vk == VK_RCONTROL ? VK_CONTROL :
                vk == VK_LMENU || vk == VK_RMENU ? VK_MENU : vk);
        bool there = false;
        for (uint32_t i = 0; i < w && !there; i++) there = keys[i].virtualKey == gameVk || scanKey(keys[i].scanCode) == scanKey(sc);
        if (!there) keys[w++] = {sc, 0, gameVk, false};
    }
    return w;
}

uint32_t onKeys(int slot, void* self, uint32_t max, KeyState* out) {
    auto original = reinterpret_cast<KeysFn>(originals[slot]);
    if (depth || !out) return original(self, max, out);
    Nest nest;
    KeyState keys[maxKeys + 16];
    uint32_t n = std::min<uint32_t>(original(self, maxKeys, keys), maxKeys);
    n = filter(keys, n, maxKeys + 16);
    uint32_t c = std::min(n, max);
    std::memcpy(out, keys, c * sizeof(KeyState));
    return c;
}

uint32_t onCount(int slot, void* self) {
    auto original = reinterpret_cast<CountFn>(originals[slot]);
    if (depth) return original(self);
    const Known* k = knownOf(self);
    if (!k) return original(self);
    Nest nest;
    KeyState keys[maxKeys + 16];
    auto read = reinterpret_cast<KeysFn>(hook::vfunc(self, k->layout->keys));
    uint32_t n = std::min<uint32_t>(read(self, maxKeys, keys), maxKeys);
    return filter(keys, n, maxKeys + 16);
}

struct Mouse {
    void* id = nullptr;
    bool used = false;
    bool blocked = false;
    bool blockedByMenu = false;
    uint32_t seenButtons = 0;
    uint32_t suppressed = 0;
    int64_t lastUp[2]{};
    std::array<int64_t, 64> clicks[2]{};
    int clickCount[2]{};
    int64_t raw[4]{};
    int64_t out[4]{};
    MouseMotion motion[2];
    int64_t readAt = 0;
    uint32_t stale = 0;
};

std::mutex mouseLock;
std::array<Mouse, 8> mice;
// presses of a mouse button the game did not get to see, by reason: Monchi's menu open, window not focused, still
// held from before the menu closed, taken out by a module, click limiter
std::array<std::atomic<unsigned>, 5> hiddenPresses{};
uint32_t rawBefore = 0;
int wheelLogs = 0;

Mouse& mouseFor(void* id) {
    for (auto& m : mice)
        if (m.used && m.id == id) return m;
    for (auto& m : mice)
        if (!m.used) {
            m = {};
            m.id = id;
            return m;
        }
    mice[0] = {};
    mice[0].id = id;
    return mice[0];
}

// Runs on the reading the game takes its buttons from, so a press that is over the limit is never seen at all. It stays
// hidden until the button comes up again, otherwise the game would get half a click.
void limit(Mouse& m, uint32_t& buttons, int64_t now, int64_t freq) {
    uint32_t raw = buttons;
    uint32_t rising = raw & ~m.seenButtons, falling = ~raw & m.seenButtons;
    m.seenButtons = raw;
    int limits[2] = {limitLeft.load(std::memory_order_relaxed), limitRight.load(std::memory_order_relaxed)};
    int64_t debounce = activeDebounce.load(std::memory_order_relaxed);
    for (int b = 0; b < 2; b++) {
        uint32_t bit = 1u << b;
        if (falling & bit) {
            m.suppressed &= ~bit;
            m.lastUp[b] = now;
        }
        if (!(rising & bit)) continue;
        bool hide = debounce > 0 && m.lastUp[b] && (now - m.lastUp[b]) * 1000000 / freq < debounce;
        if (!hide && limits[b] > 0) {
            int kept = 0;
            for (int k = 0; k < m.clickCount[b]; k++)
                if (now - m.clicks[b][k] <= freq) m.clicks[b][kept++] = m.clicks[b][k];
            m.clickCount[b] = kept;
            if (kept >= limits[b] || kept >= int(m.clicks[b].size())) hide = true;
            else m.clicks[b][m.clickCount[b]++] = now;
        }
        if (hide) m.suppressed |= bit;
    }
    buttons &= ~m.suppressed;
}

void adjust(void* id, uint8_t* state, int wheel) {
    std::scoped_lock g(mouseLock);
    auto* buttons = reinterpret_cast<uint32_t*>(state);
    int64_t* v[4] = {reinterpret_cast<int64_t*>(state + 8), reinterpret_cast<int64_t*>(state + 16),
                     reinterpret_cast<int64_t*>(state + wheel), reinterpret_cast<int64_t*>(state + wheel + 8)};
    bool blocked = blockedNow();
    uint32_t rawNow = *buttons, pressed = rawNow & ~rawBefore & 3u;
    rawBefore = rawNow;
    bool wheelHeld = blocked || activeWheel.load();
    auto scale = std::bit_cast<std::array<float, 2>>(activeScale.load());
    float smooth = activeSmooth.load();
    Mouse& m = mouseFor(id);
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    static const int64_t timerFrequency = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
    freq.QuadPart = timerFrequency;
    if (m.readAt && now.QuadPart - m.readAt > freq.QuadPart / 4) {
        m.used = false;
        m.motion[0] = m.motion[1] = {};
    }
    double dt = m.readAt ? std::clamp(double(now.QuadPart - m.readAt) / double(freq.QuadPart), 0.0, 0.1) : 0.0;
    m.readAt = now.QuadPart;
    // smoothing holds back motion and lets out a share of it per 1/60 s, so it glides the same at any frame rate
    double release = smooth >= 1.f ? 1.0 : 1.0 - std::pow(1.0 - double(smooth), dt * 60.0);
    if (!m.used) {
        m.used = true;
        for (int i = 0; i < 4; i++) m.raw[i] = m.out[i] = *v[i];
    }
    for (int i = 0; i < 4; i++) {
        int64_t delta = *v[i] - m.raw[i];
        if (i >= 2 && delta && wheelLogs < 3) {
            wheelLogs++;
            logger::info("gameinput: wheel {} -> {}", m.raw[i], *v[i]);
        }
        m.raw[i] = *v[i];
        if (blocked || (i >= 2 && wheelHeld)) {
            delta = 0;
            if (i < 2) m.motion[i] = {};
        } else if (i < 2 && (scale[i] != 1.f || release < 1.0 || m.motion[i].held != 0.0)) {
            delta = m.motion[i].step(delta, scale[i], release);
        }
        m.out[i] += delta;
        *v[i] = m.out[i];
    }
    limit(m, *buttons, now.QuadPart, freq.QuadPart);
    uint32_t afterLimit = *buttons;
    *buttons &= ~activeDropButtons.load();
    uint32_t afterDrop = *buttons;
    if (blocked) {
        bool menu = ui::capturing();
        m.blocked = true;
        m.blockedByMenu = m.blockedByMenu || menu;
        *buttons = 0;
        if (pressed) hiddenPresses[menu ? 0 : 1]++;
        return;
    }
    if (m.blocked) {
        m.blocked = false;
        // A button still down when Monchi's menu closes belongs to the menu and is kept from the game until it comes up.
        // A button that is down when the window gets its focus back is the click that brought the focus: the game gets
        // it, as it does without Monchi. Holding it back made every first click after switching windows do nothing.
        m.stale = m.blockedByMenu ? *buttons : 0;
        m.blockedByMenu = false;
    }
    m.stale &= *buttons;
    *buttons &= ~m.stale;
    if ((pressed & 1) && rawShift) {
        shiftClicks++;
        if (!(*buttons & 1) || !passedShift) {
            unsigned n = ++shiftClicksLost;
            if (n <= 5) logger::warn("gameinput: a shift-click did not reach the game whole (click {}, shift {})", (*buttons & 1) ? "passed" : "held back", passedShift ? "passed" : "held back");
        }
    }
    if (pressed & ~afterLimit) hiddenPresses[4]++;
    else if (pressed & ~afterDrop) hiddenPresses[3]++;
    else if (pressed & ~*buttons) hiddenPresses[2]++;
}

bool onMouse(int slot, void* self, uint8_t* state) {
    auto original = reinterpret_cast<MouseFn>(originals[slot]);
    if (depth) return original(self, state);
    // a newer reading interface forwards to an older one we also hook; adjusting both scaled the motion twice
    Nest nest;
    bool ok = original(self, state);
    if (!ok || !state) return ok;
    const Known* k = knownOf(self);
    if (!k) return ok;
    void* device = nullptr;
    reinterpret_cast<DeviceFn>(hook::vfunc(self, k->layout->device))(self, &device);
    adjust(device, state, k->layout->wheel);
    if (device && k->deviceRefs) release(device);
    return ok;
}

template <int N>
uint32_t STDMETHODCALLTYPE countDetour(void* self) {
    return onCount(N, self);
}

template <int N>
uint32_t STDMETHODCALLTYPE keysDetour(void* self, uint32_t max, KeyState* out) {
    return onKeys(N, self, max, out);
}

template <int N>
bool STDMETHODCALLTYPE mouseDetour(void* self, uint8_t* state) {
    return onMouse(N, self, state);
}

enum Role { RoleCount, RoleKeys, RoleMouse };

template <int... N>
std::array<void*, 3 * maxHooks> detourTable(std::integer_sequence<int, N...>) {
    return {reinterpret_cast<void*>(&countDetour<N>)..., reinterpret_cast<void*>(&keysDetour<N>)...,
            reinterpret_cast<void*>(&mouseDetour<N>)...};
}

const auto detours = detourTable(std::make_integer_sequence<int, maxHooks>{});

bool hookOnce(void* target, Role role, const char* api) {
    for (int i = 0; i < hookCount; i++)
        if (targets[i] == target) return true;
    if (!target || hookCount >= maxHooks) return false;
    int slot = hookCount;
    static const char* names[] = {"GetKeyCount", "GetKeyState", "GetMouseState"};
    std::string name = std::string("GameInput ") + api + " " + names[role];
    if (!hook::create(name.c_str(), target, detours[role * maxHooks + slot], &originals[slot])) return false;
    targets[slot] = target;
    hookCount++;
    return true;
}

// ---- frame start

constexpr int maxCurrent = 4;
std::array<void*, maxCurrent> currentOriginals{}, currentTargets{};
int currentCount = 0;

std::mutex startLock;
std::atomic<float> paceFps{0.f};
std::atomic<bool> startVerified{false};
std::atomic<int64_t> lastSample{0};
std::atomic<float> shownWait{0.f}, shownAge{0.f}, shownPolls{0.f};
framestart::Detector detector;
framestart::Limiter limiter;

int64_t ticks() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

int64_t frequency() {
    static const int64_t f = [] {
        LARGE_INTEGER q;
        QueryPerformanceFrequency(&q);
        return q.QuadPart;
    }();
    return f;
}

// sleeps on a high resolution timer and spins through the last half millisecond, like the limiter at Present
void waitUntil(int64_t target) {
    thread_local HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    int64_t freq = frequency(), spin = freq / 2000;
    for (;;) {
        int64_t remaining = target - ticks();
        if (remaining <= 0) return;
        if (remaining > spin && timer) {
            LARGE_INTEGER due;
            due.QuadPart = -((remaining - spin) * 10000000 / freq);
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, DWORD(remaining * 1000 / freq + 2));
        } else {
            YieldProcessor();
        }
    }
}

// Runs in front of every reading the game asks for. On the poll that opens a frame (framestart::Detector) the frame
// limit waits here, then the GPU vendor's pacing runs, and only then the game gets its input: the wait sits in front
// of the input instead of behind it. Until the rhythm is proven the limit stays where it was, after Present.
void frameBoundary() {
    std::unique_lock g(startLock, std::try_to_lock);
    if (!g) return;
    int64_t now = ticks(), freq = frequency();
    static bool foreground = true;
    bool focused = input::focused();
    if (focused != foreground) {
        foreground = focused;
        if (!focused) detector.suspend();
        limiter = {};
        startVerified = false;
        shownWait = shownAge = shownPolls = 0.f;
        gpuLatency::bindFrameStart(false);
        logger::info("gameinput: focus {}, input pacing reset", focused ? "returned" : "lost");
    }
    if (!focused) return;
    if (!detector.poll(now, freq, GetCurrentThreadId(), dx::frame().presentQpc)) return;
    shownPolls = shownPolls + (float(detector.polls()) - shownPolls) * 0.05f;
    bool ok = detector.verified();
    if (ok != startVerified.exchange(ok)) {
        gpuLatency::bindFrameStart(ok);
        logger::info("gameinput: frame start {}", ok ? "found, the frame limit and gpu pacing wait in front of the input" : "lost, back to waiting after Present");
    }

    float waited = 0.f;
    int64_t target = limiter.next(now, freq, ok ? paceFps.load(std::memory_order_relaxed) : 0.f);
    if (target > now) {
        waited = float(double(target - now) * 1000.0 / double(freq));
        waitUntil(target);
    }
    shownWait = shownWait + (waited - shownWait) * 0.05f;
    int64_t gpuStart = ticks();
    if (ok) gpuLatency::beforeInput(uint64_t(now));
    int64_t done = ticks();
    double gpuMs = double(done - gpuStart) * 1000.0 / double(freq);
    if (gpuMs >= 250.0) logger::warn("gameinput: GPU pacing stalled {:.1f} ms", gpuMs);
    lastSample = done;
    detector.resume(done);
}

HRESULT onCurrent(int slot, void* self, unsigned kind, void* device, void** reading) {
    auto original = reinterpret_cast<CurrentFn>(currentOriginals[slot]);
    if (!depth) frameBoundary();
    return original(self, kind, device, reading);
}

template <int N>
HRESULT STDMETHODCALLTYPE currentDetour(void* self, unsigned kind, void* device, void** reading) {
    return onCurrent(N, self, kind, device, reading);
}

void* const currentDetours[maxCurrent] = {reinterpret_cast<void*>(&currentDetour<0>), reinterpret_cast<void*>(&currentDetour<1>),
                                          reinterpret_cast<void*>(&currentDetour<2>), reinterpret_cast<void*>(&currentDetour<3>)};

void hookCurrent(void* gi, const char* api) {
    void* target = hook::vfunc(gi, currentReadingSlot);
    for (int i = 0; i < currentCount; i++)
        if (currentTargets[i] == target) return;
    if (!target || currentCount >= maxCurrent) return;
    std::string name = std::string("GameInput ") + api + " GetCurrentReading";
    if (!hook::create(name.c_str(), target, currentDetours[currentCount], &currentOriginals[currentCount])) return;
    currentTargets[currentCount++] = target;
}

bool deviceAddsRef(void* reading, const Layout& layout) {
    auto device = reinterpret_cast<DeviceFn>(hook::vfunc(reading, layout.device));
    void* first = nullptr;
    device(reading, &first);
    if (!first) return false;
    ULONG a = addRef(first);
    release(first);
    void* second = nullptr;
    device(reading, &second);
    ULONG b = second ? addRef(second) : a;
    if (second) release(second);
    bool refs = b == a + 1;
    if (refs) {
        release(first);
        release(second);
    }
    return refs;
}

bool study(void* gi, unsigned kind, const Api& api) {
    void* reading = nullptr;
    auto current = reinterpret_cast<CurrentFn>(hook::vfunc(gi, currentReadingSlot));
    if (FAILED(current(gi, kind, nullptr, &reading)) || !reading) return false;
    void** vt = *static_cast<void***>(reading);
    bool fresh = true;
    int n = knownCount.load();
    for (int i = 0; i < n; i++)
        if (known[i].vtable == vt) fresh = false;
    bool ok = true;
    if (fresh && n < int(known.size())) {
        known[n] = {vt, api.layout, deviceAddsRef(reading, *api.layout)};
        knownCount.store(n + 1, std::memory_order_release);
        const Layout& l = *api.layout;
        ok = hookOnce(vt[l.count], RoleCount, api.name) && hookOnce(vt[l.keys], RoleKeys, api.name) &&
             hookOnce(vt[l.mouse], RoleMouse, api.name);
    }
    release(reading);
    return ok;
}

HMODULE runtime() {
    if (HMODULE m = GetModuleHandleW(L"GameInputRedist.dll")) return m;
    return GetModuleHandleW(L"GameInput.dll");
}

bool attach() {
    HMODULE m = runtime();
    if (!m) return false;
    auto init = reinterpret_cast<InitFn>(GetProcAddress(m, "GameInputInitialize"));
    auto create = reinterpret_cast<CreateFn>(GetProcAddress(m, "GameInputCreate"));
    void* root = nullptr;
    if (!init && (!create || FAILED(create(&root)) || !root)) return false;

    std::string found;
    bool keyboard = false, mouse = false;
    for (auto& api : apis) {
        void* gi = nullptr;
        if (init) init(api.iid, &gi);
        else reinterpret_cast<QueryFn>(hook::vfunc(root, 0))(root, api.iid, &gi);
        if (!gi) continue;
        hookCurrent(gi, api.name);
        bool k = study(gi, kindKeyboard, api);
        bool ms = study(gi, kindMouse, api);
        keyboard |= k;
        mouse |= ms;
        if (k || ms) found += std::string(found.empty() ? "" : ", ") + api.name;
        release(gi);
    }
    if (root) release(root);
    if (!keyboard || !mouse) return false;
    hook::enableAll();
    summary = "hooked (" + found + ")";
    logger::info("gameinput: readings {} with {} hooks, menu now blocks keyboard and mouse", summary, hookCount);
    return true;
}

}

void tick() {
    if (phase != Phase::Searching) return;
    static ULONGLONG next = 0;
    static int tries = 0;
    ULONGLONG now = GetTickCount64();
    if (now < next) return;
    next = now + 2000;
    if (attach()) {
        phase = Phase::Hooked;
        return;
    }
    if (!runtime() && ++tries >= 5) {
        phase = Phase::Absent;
        summary = "not used by this game";
        logger::info("gameinput: runtime not loaded, the game reads input through window messages");
    } else if (runtime() && ++tries % 15 == 0) {
        logger::info("gameinput: runtime loaded, waiting for a keyboard and a mouse reading");
    }
}

bool active() { return phase == Phase::Hooked; }

const char* status() { return summary.c_str(); }

void hold(int vk) {
    std::scoped_lock g(overlayLock);
    pendingHold.set(vk & 0xFF);
}

void cancelKey(int vk, bool cancel) { cancelled[vk & 0xff] = cancel; }

// the mouse state holds one bit per button: left, right, middle, then the two side buttons
static uint32_t buttonBit(int vk) {
    switch (vk) {
    case VK_LBUTTON: return 0x1;
    case VK_RBUTTON: return 0x2;
    case VK_MBUTTON: return 0x4;
    case VK_XBUTTON1: return 0x8;
    case VK_XBUTTON2: return 0x10;
    default: return 0;
    }
}

void drop(int vk) {
    std::scoped_lock g(overlayLock);
    if (uint32_t bit = buttonBit(vk)) pendingDropButtons |= bit;
    else pendingDrop.set(vk & 0xFF);
}

void scaleMouse(float factor) {
    scaleMouse(factor, factor);
}

void scaleMouse(float x, float y) {
    std::scoped_lock g(overlayLock);
    pendingScale[0] *= x;
    pendingScale[1] *= y;
}

void smoothMouse(float factor) {
    std::scoped_lock g(overlayLock);
    pendingSmooth = std::min(pendingSmooth, std::clamp(factor, 0.02f, 1.f));
}

void holdWheel() {
    std::scoped_lock g(overlayLock);
    pendingWheel = true;
}

void latch(int vk, bool down) {
    std::scoped_lock g(overlayLock);
    latched.set(vk & 0xFF, down);
}

void tap(int vk, int ms) {
    std::scoped_lock g(overlayLock);
    tapUntil[vk & 0xFF] = GetTickCount64() + ULONGLONG(ms > 0 ? ms : 1);
}

void clearLatches() {
    std::scoped_lock g(overlayLock);
    latched.reset();
    tapUntil.fill(0);
}

void limitClicks(int leftPerSecond, int rightPerSecond, float debounceMs) {
    std::scoped_lock g(overlayLock);
    pendingLimit = {leftPerSecond, rightPerSecond};
    pendingDebounce = debounceMs;
}

void pace(float fps) { paceFps = fps; }

std::array<unsigned, 5> hiddenClicks() {
    std::array<unsigned, 5> out{};
    for (size_t i = 0; i < out.size(); i++) out[i] = hiddenPresses[i].load();
    return out;
}

std::array<unsigned, 2> shiftClickCount() { return {shiftClicks.load(), shiftClicksLost.load()}; }

bool frameStartVerified() { return startVerified; }

FrameStart frameStart() { return {currentCount > 0, startVerified.load(), shownWait.load(), shownAge.load(), shownPolls.load()}; }

void notePresent() {
    int64_t sample = lastSample.load(std::memory_order_relaxed);
    if (!sample) return;
    float age = float(double(ticks() - sample) * 1000.0 / double(frequency()));
    if (age >= 0.f && age < 1000.f) shownAge = shownAge + (age - shownAge) * 0.05f;
}

void beginFrame() {
    std::scoped_lock g(overlayLock);
    pendingLimit = {0, 0};
    pendingDebounce = 0.f;
    pendingHold.reset();
    pendingDrop.reset();
    pendingDropButtons = 0;
    pendingScale = {1.f, 1.f};
    pendingSmooth = 1.f;
    pendingWheel = false;
}

void publish() {
    std::scoped_lock g(overlayLock);
    activeHold = pendingHold | latched;
    // a tap is held for its time and for this frame at least, so the game cannot miss a short one
    ULONGLONG now = GetTickCount64();
    for (int vk = 0; vk < 256; vk++) {
        if (!tapUntil[vk]) continue;
        activeHold.set(vk);
        if (now >= tapUntil[vk]) tapUntil[vk] = 0;
    }
    limitLeft = pendingLimit[0];
    limitRight = pendingLimit[1];
    activeDebounce = int64_t(pendingDebounce * 1000.f);
    activeDrop = pendingDrop;
    activeDropButtons = pendingDropButtons;
    activeScale = std::bit_cast<uint64_t>(pendingScale);
    activeSmooth = pendingSmooth;
    activeWheel = pendingWheel;
}

}
