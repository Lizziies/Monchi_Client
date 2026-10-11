// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "sdk/Memory.hpp"
#include <array>
#include <cstring>

namespace codePatch {

// Applied: the bytes are now `bytes` (also when they already were). Foreign: someone else changed them, they are
// left alone and no longer ours. Failed: unchanged and still ours, the caller keeps it and tries again.
enum class Result { Applied, Foreign, Failed };

template <size_t N>
Result replace(uintptr_t at, const std::array<uint8_t, N>& expected, const std::array<uint8_t, N>& bytes) {
    std::array<uint8_t, N> current{};
    if (!mem::read(at, current)) return Result::Failed;
    if (current == bytes) return Result::Applied;
    if (current != expected) return Result::Foreign;
    DWORD protection = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(at), bytes.size(), PAGE_EXECUTE_READWRITE, &protection)) return Result::Failed;
    std::memcpy(reinterpret_cast<void*>(at), bytes.data(), bytes.size());
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(at), bytes.size());
    DWORD ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(at), bytes.size(), protection, &ignored);
    return Result::Applied;
}

}
