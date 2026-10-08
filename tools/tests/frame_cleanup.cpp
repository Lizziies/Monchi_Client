#include "core/Finally.hpp"
#include <windows.h>
#include <cstdlib>
#include <stdexcept>

void done(void* value) { ++*static_cast<int*>(value); }
void fail(void*) { throw std::runtime_error("frame failed"); }
void fault(void*) { RaiseException(0xE1234567, 0, 0, nullptr); }
bool handleFault(int* value) {
    __try { guard::withCleanup(fault, done, value); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
    return false;
}
int main() {
    int releases = 0;
    guard::withCleanup([](void*) {}, done, &releases);
    if (releases != 1) std::abort();
    try { guard::withCleanup(fail, done, &releases); }
    catch (const std::runtime_error&) {}
    if (releases != 2) std::abort();
    if (!handleFault(&releases) || releases != 3) std::abort();
}
