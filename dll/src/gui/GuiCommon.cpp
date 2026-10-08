#include "GuiInternal.hpp"
#include "Theme.hpp"
#include "render/Draw.hpp"
#include "render/Ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace gui {

std::string fitText(ImFont* font, float size, std::string text, float maxW) {
    auto width = [&](const std::string& t) { return font->CalcTextSizeA(size, FLT_MAX, 0.f, t.c_str()).x; };
    if (width(text) <= maxW) return text;
    while (!text.empty()) {
        do text.pop_back();
        while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80);
        if (width(text + "…") <= maxW) break;
    }
    return text + "…";
}

void smoothScroll(bool top) {
    ImGuiID id = ImGui::GetCurrentWindow()->ID;
    auto* store = ImGui::GetStateStorage();
    float& target = *store->GetFloatRef(id ^ 0x5CA1, 0.f);
    float& last = *store->GetFloatRef(id ^ 0x5CA2, 0.f);
    if (top) {
        target = last = 0.f;
        ImGui::SetScrollY(0.f);
        return;
    }
    float cur = ImGui::GetScrollY();
    float maxY = ImGui::GetScrollMaxY();
    auto& io = ImGui::GetIO();

    if (std::fabs(cur - last) > 2.f) target = cur;
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.MouseWheel != 0.f)
        target -= io.MouseWheel * 84.f * ui::scale();
    target = std::clamp(target, 0.f, maxY);

    float next = draw::approach(cur, target, 13.f * theme::current().animSpeed);
    ImGui::SetScrollY(next);
    last = next;
}

// The scrollbar is drawn inside BeginChild from the previous frame's sizes, so whether there is anything
// to scroll is remembered from the last frame and the grab stays invisible when there is not.
void beginScroll(const char* id, ImVec2 size, bool top) {
    bool idle = !ImGui::GetStateStorage()->GetBool(ImGui::GetID(id) ^ 0x5C3, false);
    if (idle)
        for (ImGuiCol c : {ImGuiCol_ScrollbarGrab, ImGuiCol_ScrollbarGrabHovered, ImGuiCol_ScrollbarGrabActive})
            ImGui::PushStyleColor(c, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild(id, size, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (idle) ImGui::PopStyleColor(3);
    smoothScroll(top);
}

void endScroll(const char* id) {
    bool scrollable = ImGui::GetScrollMaxY() > 0.5f;
    ImGui::EndChild();
    ImGui::GetStateStorage()->SetBool(ImGui::GetID(id) ^ 0x5C3, scrollable);
}

void star(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool filled) {
    ImVec2 pts[10];
    for (int i = 0; i < 10; i++) {
        float a = -1.5708f + i * 0.62832f;
        float rr = i % 2 ? r * 0.45f : r;
        pts[i] = {c.x + std::cos(a) * rr, c.y + std::sin(a) * rr};
    }
    if (filled) dl->AddConvexPolyFilled(pts, 10, col);
    else dl->AddPolyline(pts, 10, col, ImDrawFlags_Closed, 1.4f);
}

}
