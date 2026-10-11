#include "VtableIndex.hpp"
#include <array>
#include <cstdlib>

int main() {
    std::array<std::uint8_t, 17> current{0x48,0x8b,0x8e,0xb8,1,0,0,
        0x48,0x8b,1,0x48,0x8b,0x80,0xf8,0,0,0};
    std::array<std::uint8_t, 17> previous{0x49,0x8b,0,0x49,0x8b,0xc8,
        0x48,0x8b,0x80,0x40,5,0,0};
    if (localPlayerIndex(current) != 31 || localPlayerIndex(previous) != 168) return 1;
    if (localPlayerIndex({}) != -1 || localPlayerIndex(std::span(current).first(16)) != -1) return 2;
    current[13] = 0xf9;
    if (localPlayerIndex(current) != -1) return 3;
    current[13] = 0xf8; current[16] = 0x80;
    if (localPlayerIndex(current) != -1) return 4;
    current[16] = 0; current[14] = 0x10;
    if (localPlayerIndex(current) != -1) return 5;
    current[0] = 0x90;
    if (localPlayerIndex(current) != -1) return 6;
    return 0;
}
