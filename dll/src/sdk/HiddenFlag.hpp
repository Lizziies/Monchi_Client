#pragma once
#include <cstdint>
namespace hiddenFlag {
constexpr uint8_t value(uint8_t current, uint8_t mask, bool original, bool expired) {
    return expired && !original ? uint8_t(current & ~mask) : uint8_t(current | mask);
}
}
