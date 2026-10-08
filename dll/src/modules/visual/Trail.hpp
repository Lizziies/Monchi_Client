#pragma once

#include "gui/Gui.hpp"
#include "modules/Module.hpp"
#include "modules/common/Bars.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/Needs.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"

#include <cmath>
#include <deque>
#include <map>

class ArrowTrail : public Module {
public:
    ArrowTrail()
        : Module("Arrow Trail", "Draws a fading line behind arrows, ender pearls and tridents in flight, so you can read where they fly.", Category::Visual, {"info-others"}) {
        sub("World");
        require(need::shots | need::player | game::Domain::Camera, need::sigs({"LocalPlayer", "ProjectileList"}));
        rainbowSpeed_.visible = [this] { return colorMode_.i == 1; };
        color_.visible = [this] { return colorMode_.i == 0; };
    }

    void onFrame() override {
        double now = ui::time();
        auto& shots = game::state().shots;
        for (auto& p : shots) {
            if (!wanted(p)) continue;
            auto& line = trails_[p.id];
            line.kind = p.kind;
            if (line.points.empty() || game::distance(line.points.back().pos, p.pos) > 0.15f) line.points.push_back({p.pos, now});
            line.seen = now;
        }
        for (auto it = trails_.begin(); it != trails_.end();) {
            auto& pts = it->second.points;
            while (!pts.empty() && now - pts.front().at > life_.f) pts.pop_front();
            if (pts.empty() && now - it->second.seen > 0.2) it = trails_.erase(it);
            else ++it;
        }
    }

    void onRender(ImDrawList* dl) override {
        double now = ui::time();
        float s = ui::scale();
        for (auto& [id, line] : trails_) {
            for (size_t i = 1; i < line.points.size(); i++) {
                auto& a = line.points[i - 1];
                auto& b = line.points[i];
                float age = float(now - b.at), k = std::clamp(1.f - age / life_.f, 0.f, 1.f);
                ImVec4 c = pick(line.kind, float(i) / float(line.points.size()), id);
                c.w *= k * opacity_.f;
                ImVec2 p0, p1;
                if (!game::projectLine(a.pos, b.pos, p0, p1)) continue;
                if (glow_.b) dl->AddLine(p0, p1, ImGui::GetColorU32(withAlpha(c, 0.25f)), (thickness_.f + 4.f) * s);
                dl->AddLine(p0, p1, ImGui::GetColorU32(c), thickness_.f * s);
            }
        }
    }

    void onDisable() override { trails_.clear(); }

private:
    struct Point {
        game::Vec3 pos;
        double at;
    };
    struct Line {
        std::deque<Point> points;
        int kind = 0;
        double seen = 0.0;
    };

    bool wanted(const game::Projectile& p) const {
        if (ownOnly_.b && !p.mine) return false;
        return p.kind == 0 ? arrows_.b : p.kind == 1 ? pearls_.b : tridents_.b;
    }

    ImVec4 pick(int kind, float t, uintptr_t id) const {
        if (colorMode_.i == 1) {
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(std::fmod(t * 0.5f + float(ui::time()) * rainbowSpeed_.f * 0.2f + float(id % 7) * 0.1f, 1.f), 0.6f, 1.f, r, g, b);
            return {r, g, b, 1.f};
        }
        if (colorMode_.i == 2) return kind == 0 ? arrowColor_.color : kind == 1 ? pearlColor_.color : tridentColor_.color;
        return color_.color;
    }

    Setting& arrows_ = toggleSetting("arrows", "Arrows", true);
    Setting& pearls_ = toggleSetting("pearls", "Ender pearls", true);
    Setting& tridents_ = toggleSetting("tridents", "Tridents", true);
    Setting& ownOnly_ = toggleSetting("ownOnly", "Only my own", true);
    Setting& life_ = slider("life", "Trail length (s)", 1.5f, 0.3f, 6.f, "%.1f s");
    Setting& thickness_ = slider("thickness", "Thickness", 2.f, 1.f, 8.f, "%.1f");
    Setting& opacity_ = slider("opacity", "Opacity", 0.9f, 0.1f, 1.f, "%.2f");
    Setting& glow_ = toggleSetting("glow", "Glow", true);
    Setting& colorMode_ = choice("colorMode", "Colors", {"One color", "Rainbow", "By kind"}, 2);
    Setting& rainbowSpeed_ = slider("rainbowSpeed", "Rainbow speed", 1.f, 0.2f, 4.f, "%.1f");
    Setting& color_ = colorSetting("color", "Color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& arrowColor_ = colorSetting("arrowColor", "Arrow", {1.f, 0.55f, 0.65f, 1.f});
    Setting& pearlColor_ = colorSetting("pearlColor", "Ender pearl", {0.55f, 0.45f, 1.f, 1.f});
    Setting& tridentColor_ = colorSetting("tridentColor", "Trident", {0.4f, 0.95f, 0.85f, 1.f});
    std::map<uintptr_t, Line> trails_;
};

class BlackBars : public Module {
public:
    BlackBars()
        : Module("Black Bars", "Black bars at the top and bottom of the screen for a film look. They slide in and out and can hide in menus.", Category::Visual, {"cosmetic"}) {
        sub("Camera");
    }

    void onRender(ImDrawList* dl) override {
        bool menu = hideMenus_.b && (gui::open() || game::state().screen != game::Screen::None);
        shown_ += ((menu ? 0.f : 1.f) - shown_) * std::min(1.f, ui::dt() * (animate_.b ? speed_.f : 100.f));
        ImVec4 c = color_.color;
        bars::draw(dl, amount_.f * shown_, ImGui::GetColorU32(c));
    }

    void onDisable() override { shown_ = 0.f; }

private:
    Setting& amount_ = slider("amount", "Bar height", 0.1f, 0.02f, 0.3f, "%.2f");
    Setting& animate_ = toggleSetting("animate", "Slide in and out", true);
    Setting& speed_ = slider("speed", "Slide speed", 6.f, 1.f, 20.f, "%.0f");
    Setting& hideMenus_ = toggleSetting("hideMenus", "Hide in menus", true);
    Setting& color_ = colorSetting("color", "Color", {0.f, 0.f, 0.f, 1.f});
    float shown_ = 0.f;
};
