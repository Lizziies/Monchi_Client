#include "hook/FrameWait.hpp"
#include <atomic>
#include <cassert>
#include <thread>

int main() {
    HANDLE timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    assert(timer);
    LARGE_INTEGER due{};
    due.QuadPart = -2000000;
    assert(SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE));
    std::atomic<bool> focused{false};
    std::thread resume([&] { Sleep(20); focused = true; });
    ULONGLONG began = GetTickCount64();
    bool finished = pacing::waitBackground(timer, 250, [&] { return focused.load(); });
    ULONGLONG elapsed = GetTickCount64() - began;
    resume.join();
    assert(!finished && elapsed < 180);
    CancelWaitableTimer(timer);
    due.QuadPart = -200000;
    assert(SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE));
    assert(pacing::waitBackground(timer, 100, [] { return false; }));
    assert(pacing::waitBackground(timer, 10, [] { return false; }));
    assert(!pacing::waitBackground(timer, 100, [] { return true; }));
    CloseHandle(timer);
}
