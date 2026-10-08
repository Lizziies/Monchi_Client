#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace layout {

inline float guiScale(ImVec2 display, float forced = 0.f) {
    if (forced > 0.f) return forced;
    return std::clamp(std::floor(std::min(display.x / 320.f, display.y / 240.f)), 1.f, 6.f);
}

}
