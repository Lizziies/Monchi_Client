#include "core/Guard.hpp"
#include "Inject.hpp"
#include "core/Log.hpp"
#include "hook/Dx.hpp"
#include "hook/Input.hpp"
#include "hook/GameInput.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace inject {

namespace {

constexpr ULONG_PTR marker = 0x4D4F4348;

struct Job {
    std::string text;
    int chatKey;
    bool tapOnly = false;
};

std::mutex lock;
std::condition_variable wake;
std::deque<Job> jobs;
HANDLE worker = nullptr;
std::atomic<bool> stopping{false};
std::deque<Sent> history;

bool extended(int vk) {
    switch (vk) {
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_RCONTROL:
    case VK_RMENU:
        return true;
    default:
        return false;
    }
}

void send(INPUT& in) { SendInput(1, &in, sizeof(INPUT)); }

void windowKey(int vk, bool down) {
    if (!focused()) return;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP) | (extended(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    in.ki.dwExtraInfo = marker;
    send(in);
}

void windowTap(int vk) {
    windowKey(vk, true);
    Sleep(25);
    windowKey(vk, false);
}

bool type(wchar_t ch) {
    HWND window = dx::window();
    HKL layout = GetKeyboardLayout(GetWindowThreadProcessId(window, nullptr));
    SHORT mapped = VkKeyScanExW(ch, layout);
    if (mapped == -1) {
        INPUT in[2]{};
        for (int i = 0; i < 2; ++i) {
            in[i].type = INPUT_KEYBOARD;
            in[i].ki.wScan = ch;
            in[i].ki.dwFlags = KEYEVENTF_UNICODE | (i ? KEYEVENTF_KEYUP : 0);
            in[i].ki.dwExtraInfo = marker;
        }
        return SendInput(2, in, sizeof(INPUT)) == 2;
    }
    int modifiers = (mapped >> 8) & 0xff;
    if (modifiers & 1) windowKey(VK_LSHIFT, true);
    if (modifiers & 2) windowKey(VK_LCONTROL, true);
    if (modifiers & 4) windowKey(VK_RMENU, true);
    windowTap(mapped & 0xff);
    if (modifiers & 4) windowKey(VK_RMENU, false);
    if (modifiers & 2) windowKey(VK_LCONTROL, false);
    if (modifiers & 1) windowKey(VK_LSHIFT, false);
    return focused();
}

void run() {
    for (;;) {
        Job job;
        {
            std::unique_lock g(lock);
            wake.wait(g, [] { return stopping || !jobs.empty(); });
            if (stopping) return;
            job = std::move(jobs.front());
            jobs.pop_front();
        }
        if (!focused()) continue;
        // with the chat already open the text would land behind what the player is typing and be sent with it
        if (!job.tapOnly && input::screen() == 4) {
            logger::warn("chat hotkey: the chat is open; message was not typed");
            continue;
        }
        tap(job.chatKey);
        if (job.tapOnly) continue;
        for (int i = 0; i < 100 && !stopping && focused() && input::screen() != 4; ++i) Sleep(20);
        if (stopping || !focused() || input::screen() != 4) {
            logger::warn("chat hotkey: chat did not open; message was not typed");
            continue;
        }
        Sleep(80);
        auto wide = logger::widen(job.text);
        bool complete = true;
        for (wchar_t c : wide) {
            if (stopping || !focused() || input::screen() != 4 || !type(c)) {
                complete = false;
                break;
            }
            Sleep(12);
        }
        if (!complete) {
            logger::warn("chat hotkey: interrupted before the message was complete");
            continue;
        }
        Sleep(100);
        if (focused() && input::screen() == 4) windowTap(VK_RETURN);
        for (int i = 0; i < 12 && !stopping; i++) Sleep(100);
    }
}

}

const std::deque<Sent>& sent() { return history; }

bool ours() { return GetMessageExtraInfo() == (LPARAM)marker; }

bool focused() {
    HWND w = dx::window();
    return w && GetForegroundWindow() == w;
}

void key(int vk, bool down) {
    // the game takes its keys from GameInput; SendInput is only the way for builds that read window messages
    if (gameinput::active()) {
        if (down && !focused()) return;
        gameinput::latch(vk, down);
        return;
    }
    if (!focused()) return;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP) | (extended(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    in.ki.dwExtraInfo = marker;
    send(in);
}

void tap(int vk) {
    if (gameinput::active()) {
        if (focused()) gameinput::tap(vk, 60);
        Sleep(90);
        return;
    }
    key(vk, true);
    Sleep(25);
    key(vk, false);
}

void tapLater(int vk) {
    std::scoped_lock g(lock);
    if (jobs.size() >= 8) return;
    jobs.push_back({"", vk, true});
    if (!worker) {
        worker = CreateThread(nullptr, 0, [](void*) -> DWORD {
            guard::call("inject", run);
            return 0;
        }, nullptr, 0, nullptr);
        if (!worker) {
            jobs.clear();
            logger::warn("chat hotkey: the input worker could not be started");
            return;
        }
    }
    wake.notify_one();
}

static std::atomic<int> boundChatKey{0};

void gameChatKey(int vk) {
    if (boundChatKey.exchange(vk) != vk && vk) logger::info("chat hotkey: the game opens chat with key {}", vk);
}

void say(const std::string& text, int chatKey) {
    if (text.empty() || text.size() > 256) return;
    if (int bound = boundChatKey.load()) chatKey = bound;
    history.push_back({text, ui::time(), !game::demo()});
    if (history.size() > 8) history.pop_front();
    if (game::demo()) {
        logger::info("demo: would send '{}'", text);
        return;
    }
    std::scoped_lock g(lock);
    if (jobs.size() >= 4) return;
    jobs.push_back({text, chatKey});
    if (!worker) {
        worker = CreateThread(nullptr, 0, [](void*) -> DWORD {
            guard::call("inject", run);
            return 0;
        }, nullptr, 0, nullptr);
        if (!worker) {
            jobs.clear();
            logger::warn("chat hotkey: the input worker could not be started");
            return;
        }
    }
    wake.notify_one();
}

void shutdown() {
    {
        std::scoped_lock g(lock);
        stopping = true;
    }
    wake.notify_all();
    if (worker) {
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
        worker = nullptr;
    }
}

}
