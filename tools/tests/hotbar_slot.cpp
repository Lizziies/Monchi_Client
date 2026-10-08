#include "sdk/HotbarSlot.hpp"
#include <cstdlib>
#include <limits>

void check(bool ok) { if (!ok) std::abort(); }

int main() {
    for (float width : {1280.f, 1920.f, 2560.f}) {
        for (float scale : {1.f, 2.f, 3.f, 4.f}) {
            for (int slot = 0; slot < 9; ++slot) {
                float left = width * 0.5f - 92.f * scale + slot * 20.f * scale;
                for (float noise : {-1.f, 0.f, 1.f})
                    check(hotbarSlot(width, left + noise, 24.f * scale) == slot);
            }
        }
    }
    check(!hotbarSlot(1920.f, 868.f - 20.f, 24.f));
    check(!hotbarSlot(1920.f, 868.f + 180.f, 24.f));
    check(!hotbarSlot(1920.f, 878.f, 24.f));
    check(!hotbarSlot(1920.f, 868.f, 0.f));
    check(!hotbarSlot(std::numeric_limits<float>::quiet_NaN(), 868.f, 24.f));
    check(!hotbarSlot(1920.f, std::numeric_limits<float>::infinity(), 24.f));
}
