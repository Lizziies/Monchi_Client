#pragma once

#include "gui/Theme.hpp"

#include <imgui.h>

#include <algorithm>

inline ImVec4 rampColor(float v, float good, float bad, const ImVec4& lo, const ImVec4& mid, const ImVec4& hi) {
    if (good == bad) return lo;
    float t = std::clamp((v - good) / (bad - good), 0.f, 1.f);
    if (t < 0.5f) return theme::mix(lo, mid, t * 2.f);
    return theme::mix(mid, hi, (t - 0.5f) * 2.f);
}

inline ImVec4 withAlpha(ImVec4 c, float a) {
    c.w *= a;
    return c;
}
