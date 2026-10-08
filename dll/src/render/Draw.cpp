#include "Draw.hpp"
#include "Ui.hpp"

#include <algorithm>
#include <cmath>

namespace draw {

static bool motionOn = true;

void setMotion(bool on) { motionOn = on; }

bool motion() { return motionOn; }

float approach(float current, float target, float speed) {
    if (!motionOn) return target;
    float k = 1.f - std::exp(-speed * ui::dt());
    float v = current + (target - current) * k;
    if (std::fabs(v - target) < 0.001f) v = target;
    return v;
}

float easeOutBack(float t) {
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    t -= 1.f;
    return 1.f + c3 * t * t * t + c1 * t * t;
}

float easeOutCubic(float t) {
    t = 1.f - t;
    return 1.f - t * t * t;
}

float easeInOutCubic(float t) {
    if (t < 0.5f) return 4.f * t * t * t;
    t = -2.f * t + 2.f;
    return 1.f - t * t * t * 0.5f;
}

float easeInOutSine(float t) { return 0.5f - 0.5f * std::cos(std::clamp(t, 0.f, 1.f) * 3.14159265f); }

void heart(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float r = size * 0.27f;
    ImVec2 l{c.x - r * 0.95f, c.y - size * 0.12f};
    ImVec2 rr{c.x + r * 0.95f, c.y - size * 0.12f};
    dl->AddCircleFilled(l, r, color, 20);
    dl->AddCircleFilled(rr, r, color, 20);
    dl->AddTriangleFilled({c.x - r * 1.9f, c.y - size * 0.02f}, {c.x + r * 1.9f, c.y - size * 0.02f},
                          {c.x, c.y + size * 0.45f}, color);
}

void sparkle(ImDrawList* dl, ImVec2 c, float s, ImU32 color) {
    float w = s * 0.18f;
    ImVec2 pts[8] = {
        {c.x, c.y - s}, {c.x + w, c.y - w}, {c.x + s, c.y}, {c.x + w, c.y + w},
        {c.x, c.y + s}, {c.x - w, c.y + w}, {c.x - s, c.y}, {c.x - w, c.y - w},
    };
    dl->AddConcavePolyFilled(pts, 8, color);
}

void glow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, ImU32 color, float spread) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
    const int steps = 6;
    for (int i = steps; i >= 1; i--) {
        float t = float(i) / steps;
        float grow = spread * t;
        ImVec4 cc = c;
        cc.w *= (1.f - t) * 0.35f;
        dl->AddRectFilled(min - ImVec2(grow, grow), max + ImVec2(grow, grow), ImGui::ColorConvertFloat4ToU32(cc),
                          rounding + grow);
    }
}

void gradientRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 left, ImU32 right, float rounding) {
    if (rounding <= 0.f) {
        dl->AddRectFilledMultiColor(min, max, left, right, right, left);
        return;
    }
    int start = dl->VtxBuffer.Size;
    dl->AddRectFilled(min, max, IM_COL32_WHITE, rounding);
    int end = dl->VtxBuffer.Size;
    ImVec4 a = ImGui::ColorConvertU32ToFloat4(left);
    ImVec4 b = ImGui::ColorConvertU32ToFloat4(right);
    float w = std::max(1.f, max.x - min.x);
    for (int i = start; i < end; i++) {
        auto& v = dl->VtxBuffer[i];
        float t = std::clamp((v.pos.x - min.x) / w, 0.f, 1.f);
        ImVec4 c{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
        ImVec4 orig = ImGui::ColorConvertU32ToFloat4(v.col);
        c.w *= orig.w;
        v.col = ImGui::ColorConvertFloat4ToU32(c);
    }
}

void pill(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color) {
    dl->AddRectFilled(min, max, color, (max.y - min.y) * 0.5f);
}

void textCentered(ImDrawList* dl, ImFont* font, float size, ImVec2 center, ImU32 color, const char* text) {
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.f, text);
    dl->AddText(font, size, center - ts * 0.5f, color, text);
}

}
