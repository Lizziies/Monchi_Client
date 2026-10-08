#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace synchedItem {

inline std::optional<int32_t> integer(std::span<const uint8_t> bytes, uint16_t id) {
    if (bytes.size() < 16 || bytes[8] != 2) return {};
    uint16_t actual;
    std::memcpy(&actual, bytes.data() + 10, sizeof(actual));
    if (actual != id) return {};
    int32_t value;
    std::memcpy(&value, bytes.data() + 12, sizeof(value));
    return value;
}

}
