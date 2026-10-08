#pragma once
#include <array>

namespace inventoryKeys {
inline bool blocked(int key, const std::array<int, 9>& bindings, int offhand) {
    if (key < 8 || key > 255 || (offhand && key == offhand)) return false;
    if (key >= '1' && key <= '9') return true;
    for (int bound : bindings) if (key == bound) return true;
    return false;
}
}
