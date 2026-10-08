#pragma once

#include <windows.h>
#include <psapi.h>
#include <array>
#include <algorithm>
#include <cstdint>

namespace resident {

template<class Fn>
void runs(uintptr_t address, size_t size, Fn&& fn) {
    constexpr size_t page = 4096;
    std::array<PSAPI_WORKING_SET_EX_INFORMATION, 256> info{};
    size_t count = (size + page - 1) / page;
    if (!size || size > page * info.size() || address % page) return;
    for (size_t i = 0; i < count; ++i) info[i].VirtualAddress = reinterpret_cast<void*>(address + i * page);
    if (!QueryWorkingSetEx(GetCurrentProcess(), info.data(), DWORD(count * sizeof(info[0])))) return;
    for (size_t i = 0; i < count;) {
        if (!info[i].VirtualAttributes.Valid) { ++i; continue; }
        size_t first = i++;
        while (i < count && info[i].VirtualAttributes.Valid) ++i;
        size_t begin = first * page;
        fn(address + begin, (std::min)(size, i * page) - begin);
    }
}

}
