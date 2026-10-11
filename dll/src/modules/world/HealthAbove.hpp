#pragma once

#include "modules/Module.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <unordered_map>

class HealthAbove : public Module {
public:
    HealthAbove()
        : Module("Health Above Head", "Shows the health of players you have aimed at above their heads, as a bar, a number or both. Never for players you have not had in your crosshair.", Category::Visual, {"info-others", "server-rules"}) {
        sub("World");
        require(need::player | need::others | need::camera, need::sigs({"LocalPlayer"}));
        markRisky("Shows information the vanilla client doesn't. Only use it where the server allows it.");
        shown_.visible = [this] { return style_.i != 0; };
        width_.visible = [this] { return style_.i != 1; };
    }

    void onRender(ImDrawList* dl) override {
        auto& st = game::state();
        auto& me = st.player;
        float s = ui::scale();
        std::unordered_map<uintptr_t, float> next;
        // Only who was in the crosshair in the last seconds gets a bar. The crosshair only picks what stands free in
        // front of you, so nothing is shown through a wall and nobody is found by their bar.
        if (st.target.kind == game::Target::Kind::Entity) {
            const game::Other* aimed = nullptr;
            float best = 3.f;
            for (auto& o : st.others) {
                float d = game::distance({o.pos.x, st.target.pos.y, o.pos.z}, st.target.pos);
                if (d < best) {
                    best = d;
                    aimed = &o;
                }
            }
            if (aimed) seen_[aimed->id] = st.time;
        }
        std::erase_if(seen_, [&](auto& e) { return st.time - e.second > keep_.f; });
        for (auto& o : st.others) {
            if (!seen_.count(o.id)) continue;
            if (!o.isPlayer && !mobs_.b) continue;
            if (!teammates_.b && o.team != 0 && o.team == me.team) continue;
            float dist = game::distance(me.pos, o.pos);
            if (dist > range_.f || o.maxHealth <= 0.f) continue;
            auto at = game::project({o.pos.x, o.pos.y + offset_.f, o.pos.z});
            if (!at) continue;
            float frac = std::clamp(o.health / o.maxHealth, 0.f, 1.f);
            float& smooth = next.emplace(o.id, smooth_.count(o.id) ? smooth_[o.id] : frac).first->second;
            smooth = draw::approach(smooth, frac, 12.f);
            float k = std::clamp(9.f / std::max(dist, 1.f), 0.55f, 1.4f) * scale_.f * s;
            drawOne(dl, *at, k, o.health, o.maxHealth, smooth);
        }
        smooth_ = std::move(next);
    }

private:
    static void drawHeart(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
        dl->AddCircleFilled({c.x - r * 0.55f, c.y - r * 0.3f}, r * 0.62f, col, 12);
        dl->AddCircleFilled({c.x + r * 0.55f, c.y - r * 0.3f}, r * 0.62f, col, 12);
        dl->AddTriangleFilled({c.x - r * 1.12f, c.y - r * 0.05f}, {c.x + r * 1.12f, c.y - r * 0.05f}, {c.x, c.y + r * 1.15f}, col);
    }

    void drawOne(ImDrawList* dl, ImVec2 at, float k, float health, float maxHealth, float frac) {
        ImVec4 col = rampColor(1.f - frac, 0.f, 1.f, good_.color, mid_.color, low_.color);
        float w = width_.f * k, h = 7.f * k;
        float y = at.y;
        if (style_.i != 0) {
            std::string t = shown_.i == 0 ? std::format("{:.0f}", health) : shown_.i == 1 ? std::format("{:.1f}", health / 2.f) : std::format("{:.0f} / {:.0f}", health, maxHealth);
            ImFont* f = fonts::bold();
            float size = 14.f * k;
            ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0.f, t.c_str());
            bool heart = shown_.i == 1;
            float hw = heart ? size * 0.85f : 0.f;
            ImVec2 p{at.x - (ts.x + hw) * 0.5f, y - ts.y};
            dl->AddText(f, size, p + ImVec2(1, 1), IM_COL32(0, 0, 0, 170), t.c_str());
            dl->AddText(f, size, p, ImGui::GetColorU32(col), t.c_str());
            if (heart) drawHeart(dl, {p.x + ts.x + size * 0.12f + hw * 0.5f, p.y + ts.y * 0.52f}, size * 0.34f, ImGui::GetColorU32(col));
            y -= ts.y + 2.f * k;
        }
        if (style_.i != 1) {
            ImVec2 a{at.x - w * 0.5f, y - h}, b{at.x + w * 0.5f, y};
            dl->AddRectFilled(a - ImVec2(1.5f * k, 1.5f * k), b + ImVec2(1.5f * k, 1.5f * k), IM_COL32(0, 0, 0, 140), h * 0.6f);
            if (frac > 0.01f) dl->AddRectFilled(a, {a.x + w * frac, b.y}, ImGui::GetColorU32(col), h * 0.5f);
        }
    }

    Setting& style_ = choice("style", "Display", {"Bar", "Number", "Bar and number"}, 2);
    Setting& shown_ = choice("shown", "Number as", {"Points", "Hearts", "Points of max"}, 1);
    Setting& range_ = slider("range", "Range", 30.f, 5.f, 80.f, "%.0f m");
    Setting& keep_ = slider("keep", "Stays after aiming", 4.f, 1.f, 10.f, "%.0f s");
    Setting& offset_ = slider("offset", "Height above feet", 2.45f, 1.8f, 3.5f, "%.2f");
    Setting& scale_ = slider("scale", "Size", 1.f, 0.6f, 2.f, "%.2fx");
    Setting& width_ = slider("width", "Bar width", 56.f, 30.f, 110.f, "%.0f");
    Setting& teammates_ = toggleSetting("teammates", "Also teammates", false);
    Setting& mobs_ = toggleSetting("mobs", "Also mobs", false);
    Setting& good_ = colorSetting("good", "Full", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Half", {1.f, 0.82f, 0.49f, 1.f});
    Setting& low_ = colorSetting("low", "Low", {1.f, 0.35f, 0.4f, 1.f});
    std::unordered_map<uintptr_t, float> smooth_;
    std::unordered_map<uintptr_t, double> seen_;
};
