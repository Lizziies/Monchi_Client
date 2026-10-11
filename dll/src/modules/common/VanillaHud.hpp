#pragma once

#include "Layout.hpp"

#include <imgui.h>

#include <cmath>

// Where Bedrock's classic HUD sits on screen. Everything is derived from the window size and the GUI scale;
// `tune` shifts it by whole GUI pixels and scales it for GUI scales the formula does not match.
namespace vanilla {

struct Tune {
    float guiScale = 0.f;
    float offsetX = 0.f;
    float offsetY = 0.f;
    float scale = 1.f;
};

struct Hud {
    float k = 1.f;
    ImVec2 hotbarMin;

    ImVec2 at(float x, float y) const { return {std::floor(hotbarMin.x + x * k), std::floor(hotbarMin.y + y * k)}; }

    // the 20x20 cell of a slot, 16x16 inside it
    ImVec2 slotMin(int slot) const { return at(1.f + 20.f * float(slot), 1.f); }
    ImVec2 slotMax(int slot) const { return at(21.f + 20.f * float(slot), 21.f); }
    ImVec2 slotInnerMin(int slot) const { return at(3.f + 20.f * float(slot), 3.f); }

    ImVec2 hotbarMax() const { return at(182.f, 22.f); }

    // icon 0 is the left-most heart; hunger icon 0 is the right-most drumstick, the bar empties right to left
    ImVec2 heart(int i) const { return at(8.f * float(i), -25.f); }
    ImVec2 drumstick(int i) const { return at(182.f - 8.f * float(i + 1), -25.f); }
};

// the icons end 39 GUI pixels above the screen edge, the hotbar is 22 pixels high, so they sit 17 above it
inline Hud hud(ImVec2 display, const Tune& t) {
    Hud h;
    float base = layout::guiScale(display, t.guiScale);
    h.k = base * t.scale;
    float width = 182.f * h.k;
    ImVec2 min{(display.x - width) * 0.5f + t.offsetX * h.k, display.y - 22.f * h.k + t.offsetY * h.k};
    h.hotbarMin = min;
    return h;
}

}
