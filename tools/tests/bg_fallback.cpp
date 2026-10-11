#include <windows.h>
#include "core/Guard.hpp"
#include "core/Log.hpp"
#include <future>
#include <thread>
#include <cstdlib>

HANDLE WINAPI failedThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return nullptr;
}
#define CreateThread failedThread
#include "../../dll/src/core/Bg.cpp"
#undef CreateThread

namespace logger { void write(std::string_view, std::string_view) {} }
namespace guard {
bool run(const char*, void (*fn)(void*), void* value) { fn(value); return true; }
void report(const char*, const char*) {}
}
int main() {
    bool nested = false;
    std::promise<void> entered, release;
    auto gate = release.get_future();
    std::thread caller([&] {
        bg::run([&] {
            bg::run([&] { nested = true; });
            entered.set_value();
            gate.wait();
        });
    });
    entered.get_future().wait();
    if (!nested || bg::drain(20)) std::abort();
    release.set_value();
    caller.join();
    if (!bg::drain(20)) std::abort();
    bool late = false;
    bg::run([&] { late = true; });
    if (late) std::abort();
}
