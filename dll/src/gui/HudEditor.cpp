#include "I18n.hpp"
#include "HudEditor.hpp"
#include "Gui.hpp"
#include "Theme.hpp"
#include "modules/HudModule.hpp"
#include "modules/Manager.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <imgui.h>

#include <cmath>
#include <format>
#include <string>
#include <vector>

namespace hudeditor {

static HudModule* dragging = nullptr;
static ImVec2 grabOffset{0, 0};

static void dashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float thick, float dash) {
    auto line = [&](ImVec2 p, ImVec2 q) {
        ImVec2 d = q - p;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len <= 0) return;
        ImVec2 dir = d / len;
        for (float t = 0; t < len; t += dash * 2) {
            float e = std::min(t + dash, len);
            dl->AddLine(p + dir * t, p + dir * e, col, thick);
        }
    };
    line(a, {b.x, a.y});
    line({b.x, a.y}, b);
    line(b, {a.x, b.y});
    line({a.x, b.y}, a);
}

struct Rect {
    float l, t, r, b;
};

static ImVec2 boxSize(HudModule* h) {
    float s = ui::scale();
    ImVec2 sz = h->size();
    return {std::max(sz.x, 34.f * s), std::max(sz.y, 22.f * s)};
}

static Rect rectOf(HudModule* h) {
    ImVec2 p = h->position(), sz = boxSize(h);
    return {p.x, p.y, p.x + sz.x, p.y + sz.y};
}

struct Snap {
    float delta = 0.f;
    float line = 0.f;
    bool hit = false;
};

static void consider(Snap& best, float threshold, float mine, float target) {
    float d = target - mine;
    if (std::fabs(d) > threshold || (best.hit && std::fabs(d) >= std::fabs(best.delta))) return;
    best = {d, target, true};
}

static HudModule* selected = nullptr;

struct FadeScope {
    explicit FadeScope(float f) { theme::setFade(f); }
    ~FadeScope() { theme::setFade(1.f); }
};

void draw() {
    static double lastDraw = -1.0;
    static float anim = 0.f;
    double now = ui::time();
    if (now - lastDraw > 0.25) anim = 0.f;
    lastDraw = now;
    anim = draw::motion() ? draw::approach(anim, 1.f, 12.f * theme::current().animSpeed) : 1.f;
    FadeScope fadeScope(anim);
    float rise = (1.f - draw::easeOutCubic(anim));

    auto& t = theme::current();
    float s = ui::scale();
    auto& io = ImGui::GetIO();
    auto ds = io.DisplaySize;
    auto* fg = ImGui::GetForegroundDrawList();
    auto* bg = ImGui::GetBackgroundDrawList();

    bg->AddRectFilled({0, 0}, ds, IM_COL32(10, 4, 12, int(70 * anim)));

    const char* help = i18n::tr("Drag = move  ·  Arrows = nudge  ·  Mouse wheel = size  ·  Double click = reset  ·  Right click = settings  ·  Shift = no snap  ·  ESC = done");
    ImVec2 hs = ImGui::CalcTextSize(help);
    ImVec2 hp{(ds.x - hs.x) * 0.5f - 16 * s, 18 * s - rise * 36 * s};
    fg->AddRectFilled(hp, hp + hs + ImVec2(32 * s, 16 * s), theme::col(t.surface, 0.95f), 99.f);
    fg->AddText(hp + ImVec2(16 * s, 8 * s), theme::col(t.text), help);

    HudModule* hovered = nullptr;
    float hoveredArea = 0.f;
    std::vector<HudModule*> shown;
    for (auto& m : modules::all()) {
        if (!m->enabled() || !m->isHud()) continue;
        auto* h = static_cast<HudModule*>(m.get());
        shown.push_back(h);
        ImVec2 p = h->position(), sz = boxSize(h);
        if (!ImGui::IsMouseHoveringRect(p, p + sz, false)) continue;
        float area = sz.x * sz.y;
        if (!hovered || area < hoveredArea) {
            hovered = h;
            hoveredArea = area;
        }
    }

    for (auto* h : shown) {
        ImVec2 p = h->position(), sz = boxSize(h);
        bool active = h == hovered || h == dragging || h == selected;
        ImU32 col = theme::col(active ? t.accent : t.accent2, active ? 1.f : 0.6f);
        dashedRect(fg, p - ImVec2(3 * s, 3 * s), p + sz + ImVec2(3 * s, 3 * s), col, 1.5f * s, 5 * s);
        if (h == selected) fg->AddRect(p - ImVec2(3 * s, 3 * s), p + sz + ImVec2(3 * s, 3 * s), theme::col(t.accent, 0.35f), 4 * s, 0, 3.f * s);
        if ((h == hovered && !dragging) || h == selected || h == dragging || h->size().x < 20.f * s) {
            std::string label = std::format("{}  ·  {:.0f}%", h->name(), h->scale() * 100.f);
            fg->AddText(fonts::regular(), 13 * s, p + ImVec2(0, -18 * s), theme::col(t.text), label.c_str());
        }
    }

    if (!dragging && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        dragging = selected = hovered;
        grabOffset = io.MousePos - hovered->position();
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            hovered->resetSettings([](const Setting& st) { return st.id == "x" || st.id == "y" || st.id == "scale"; });
            dragging = nullptr;
            return;
        }
    }
    if (!dragging && !hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) selected = nullptr;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        gui::setEditingHud(false);
        gui::showModule(hovered);
        return;
    }
    if (hovered && io.MouseWheel != 0.f) hovered->setScale(hovered->scale() + io.MouseWheel * 0.08f);

    if (selected && !dragging) {
        float step = io.KeyShift ? 10.f : 1.f;
        ImVec2 nudge{0, 0};
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) nudge.x -= step;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nudge.x += step;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) nudge.y -= step;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) nudge.y += step;
        if (nudge.x != 0.f || nudge.y != 0.f) selected->setPosition(selected->position() + nudge);
    }

    if (dragging) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            dragging = nullptr;
            return;
        }
        ImVec2 target = io.MousePos - grabOffset;
        ImVec2 sz = boxSize(dragging);
        if (!io.KeyShift) {
            float th = 7 * s;
            Snap sx, sy;
            float l = target.x, r = target.x + sz.x, cx = target.x + sz.x * 0.5f;
            float tp = target.y, bt = target.y + sz.y, cy = target.y + sz.y * 0.5f;
            for (float e : {0.f, ds.x * 0.5f, ds.x}) {
                consider(sx, th, l, e);
                consider(sx, th, cx, e);
                consider(sx, th, r, e);
            }
            for (float e : {0.f, ds.y * 0.5f, ds.y}) {
                consider(sy, th, tp, e);
                consider(sy, th, cy, e);
                consider(sy, th, bt, e);
            }
            for (auto* h : shown) {
                if (h == dragging) continue;
                Rect o = rectOf(h);
                for (float e : {o.l, (o.l + o.r) * 0.5f, o.r}) {
                    consider(sx, th, l, e);
                    consider(sx, th, cx, e);
                    consider(sx, th, r, e);
                }
                for (float e : {o.t, (o.t + o.b) * 0.5f, o.b}) {
                    consider(sy, th, tp, e);
                    consider(sy, th, cy, e);
                    consider(sy, th, bt, e);
                }
            }
            ImU32 guide = theme::col(t.accent, 0.85f);
            if (sx.hit) {
                target.x += sx.delta;
                fg->AddLine({sx.line, 0}, {sx.line, ds.y}, guide, 1.f * s);
            }
            if (sy.hit) {
                target.y += sy.delta;
                fg->AddLine({0, sy.line}, {ds.x, sy.line}, guide, 1.f * s);
            }
        }
        dragging->setPosition(target);
    }
}

}
