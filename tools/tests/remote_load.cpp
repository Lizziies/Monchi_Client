#include "RemoteLoad.hpp"
#include <cstdlib>

void check(bool ok) { if (!ok) std::abort(); }
int main() {
    HANDLE gate = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    check(gate != nullptr);
    HANDLE worker = CreateThread(nullptr, 0, [](void* arg) -> DWORD {
        WaitForSingleObject(static_cast<HANDLE>(arg), INFINITE);
        return 1;
    }, gate, 0, nullptr);
    check(worker != nullptr);
    auto pending = game::waitLoad(worker, 0);
    check(!pending.finished && !pending.loaded);
    SetEvent(gate);
    auto completed = game::waitLoad(worker, 2000);
    check(completed.finished && completed.loaded);
    CloseHandle(worker);
    CloseHandle(gate);
    auto missing = game::waitLoad(nullptr, 0);
    check(missing.finished && !missing.loaded);
    auto invalid = game::waitLoad(reinterpret_cast<HANDLE>(1), 0);
    check(!invalid.finished && !invalid.loaded);
}
