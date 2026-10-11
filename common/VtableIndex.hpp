#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

inline int localPlayerIndex(std::span<const std::uint8_t> code) {
    std::size_t displacement;
    if (code.size() >= 17 && code[0] == 0x48 && code[1] == 0x8b &&
        code[2] == 0x8e && code[7] == 0x48 && code[8] == 0x8b &&
        code[9] == 0x01 && code[10] == 0x48 && code[11] == 0x8b && code[12] == 0x80)
        displacement = 13;
    else if (code.size() >= 13 && code[0] == 0x49 && code[1] == 0x8b &&
             code[2] == 0x00 && code[3] == 0x49 && code[4] == 0x8b &&
             code[5] == 0xc8 && code[6] == 0x48 && code[7] == 0x8b && code[8] == 0x80)
        displacement = 9;
    else
        return -1;
    std::int32_t bytes;
    std::memcpy(&bytes, code.data() + displacement, sizeof(bytes));
    if (bytes < 0 || bytes % 8 || bytes / 8 >= 512) return -1;
    return bytes / 8;
}
