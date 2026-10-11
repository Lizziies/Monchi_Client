#pragma once

#include <imgui.h>

#include <algorithm>

namespace bars {

inline void draw(ImDrawList* dl, float fraction, ImU32 color = IM_COL32(0, 0, 0, 255)) {
    if (fraction <= 0.001f) return;
    auto ds = ImGui::GetIO().DisplaySize;
    float h = ds.y * std::min(fraction, 0.4f);
    dl->AddRectFilled({0.f, 0.f}, {ds.x, h}, color);
    dl->AddRectFilled({0.f, ds.y - h}, {ds.x, ds.y}, color);
}

}
