#include "sdk/ResidentPages.hpp"
#include <cstdlib>
#include <cstdio>

void check(bool ok) { if (!ok) std::abort(); }

int main() {
    constexpr size_t bytes = 1 << 20;
    auto p = static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(p != nullptr);
    p[0] = 42;
    p[2 * 4096] = 43;
    size_t visited = 0;
    bool first = false, second = false;
    resident::runs(uintptr_t(p), bytes, [&](uintptr_t at, size_t n) {
        check(at % 4096 == 0 && n % 4096 == 0);
        check(at >= uintptr_t(p) && at + n <= uintptr_t(p) + bytes);
        visited += n;
        first |= at <= uintptr_t(p) && uintptr_t(p) < at + n;
        second |= at <= uintptr_t(p) + 8192 && uintptr_t(p) + 8192 < at + n;
    });
    check(first && second);
    check(visited < bytes);
    std::printf("resident scan: %zu of %zu bytes, cold pages untouched\n", visited, bytes);
    check(VirtualFree(p, 0, MEM_RELEASE) != 0);
}
