#pragma once

#include "Memory.hpp"
#include <optional>

namespace dimensionRead {

inline std::optional<int> id(uintptr_t actor, uintptr_t image, int field, int getter, int value) {
    if (!actor || !image || field < 0 || getter <= 0 || value < 0) return {};
    uintptr_t dimension = mem::pointer(actor + field);
    uintptr_t table = dimension ? mem::pointer(dimension) : 0;
    if (!table || mem::pointer(table + 2 * 8) != image + getter) return {};
    int result = -1;
    if (!mem::read(dimension + value, result) || result < 0 || result > 2) return {};
    return result;
}

}
