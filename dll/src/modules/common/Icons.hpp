#pragma once

#include "sdk/Types.hpp"

#include <imgui.h>

namespace icons {

inline void head(ImDrawList* dl, const game::TabEntry& e, ImVec2 p, float size) {
    float px = size / 8.f;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            uint32_t argb = e.head[size_t(y * 8 + x)];
            ImU32 col = IM_COL32((argb >> 16) & 255, (argb >> 8) & 255, argb & 255, 255);
            dl->AddRectFilled(p + ImVec2(x * px, y * px), p + ImVec2((x + 1) * px + 0.5f, (y + 1) * px + 0.5f), col);
        }
}

inline void initial(ImDrawList* dl, const std::string& name, ImVec2 p, float size, ImU32 text) {
    unsigned h = 2166136261u;
    for (unsigned char c : name) h = (h ^ c) * 16777619u;
    ImVec4 col = ImColor::HSV(float(h % 360) / 360.f, 0.45f, 0.75f).Value;
    dl->AddRectFilled(p, p + ImVec2(size, size), ImGui::GetColorU32(col), size * 0.2f);
    char ch[2] = {name.empty() ? '?' : char(std::toupper((unsigned char)name[0])), 0};
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(size * 0.7f, 1000.f, 0.f, ch);
    dl->AddText(ImGui::GetFont(), size * 0.7f, p + (ImVec2(size, size) - ts) * 0.5f, text, ch);
}

inline void platform(ImDrawList* dl, game::Platform kind, ImVec2 p, float size, ImU32 color) {
    float u = size / 10.f;
    switch (kind) {
    case game::Platform::Desktop:
        dl->AddRect(p + ImVec2(0.5f * u, 1.5f * u), p + ImVec2(9.5f * u, 7.f * u), color, u * 0.8f, 0, u * 1.1f);
        dl->AddLine(p + ImVec2(5.f * u, 7.f * u), p + ImVec2(5.f * u, 8.8f * u), color, u * 1.1f);
        dl->AddLine(p + ImVec2(3.f * u, 8.8f * u), p + ImVec2(7.f * u, 8.8f * u), color, u * 1.1f);
        break;
    case game::Platform::Mobile:
        dl->AddRect(p + ImVec2(2.8f * u, 0.5f * u), p + ImVec2(7.2f * u, 9.5f * u), color, u * 1.1f, 0, u * 1.1f);
        dl->AddCircleFilled(p + ImVec2(5.f * u, 8.3f * u), u * 0.5f, color);
        break;
    case game::Platform::Console:
        dl->AddRect(p + ImVec2(0.5f * u, 2.5f * u), p + ImVec2(9.5f * u, 7.5f * u), color, u * 2.2f, 0, u * 1.1f);
        dl->AddLine(p + ImVec2(2.5f * u, 5.f * u), p + ImVec2(4.5f * u, 5.f * u), color, u * 1.1f);
        dl->AddLine(p + ImVec2(3.5f * u, 4.f * u), p + ImVec2(3.5f * u, 6.f * u), color, u * 1.1f);
        dl->AddCircleFilled(p + ImVec2(6.6f * u, 4.6f * u), u * 0.6f, color);
        dl->AddCircleFilled(p + ImVec2(7.8f * u, 5.6f * u), u * 0.6f, color);
        break;
    default:
        dl->AddText(ImGui::GetFont(), size, p + ImVec2(size * 0.25f, 0.f), color, "?");
        break;
    }
}

}
