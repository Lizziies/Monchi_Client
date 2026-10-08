#pragma once

#include "gui/Theme.hpp"
#include "hook/Input.hpp"
#include "modules/HudModule.hpp"
#include "modules/Manager.hpp"
#include "modules/common/Colors.hpp"
#include "render/Draw.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>
#include <array>

class MouseStrokes : public HudModule {
public:
    MouseStrokes() : HudModule("Mouse Strokes", "Shows your mouse movement as a dot with a trail.", {"hud-self"}, {0.26f, 0.88f}) {
        sub("Info displays");
        trailLength_.visible = trailColor_.visible = [this] { return trail_.b; };
        frameColor_.visible = [this] { return frame_.b || cross_.b; };
    }

    void onEnable() override {
        target_ = dot_ = ImVec2(0, 0);
        head_ = count_ = 0;
        haveView_ = false;
    }

protected:
    bool hasText() const override { return false; }

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        float size = boxSize_.f * s;
        float dx = 0.f, dy = 0.f;
        auto& p = game::state().player;
        if (source_.i == 1 && game::has(game::Domain::Player)) {
            if (haveView_) {
                dx = (std::fmod(p.yaw - lastYaw_ + 540.f, 360.f) - 180.f) * 8.f;
                dy = (p.pitch - lastPitch_) * 8.f;
            }
            lastYaw_ = p.yaw;
            lastPitch_ = p.pitch;
            haveView_ = true;
        } else {
            auto d = modules::mouseDelta();
            dx = float(d.x);
            dy = float(d.y);
            haveView_ = false;
        }

        float keep = 1.f - returnSpeed_.f;
        target_.x = std::clamp(target_.x * keep + dx * sensitivity_.f * 0.05f, -1.f, 1.f);
        target_.y = std::clamp(target_.y * keep + dy * sensitivity_.f * 0.05f, -1.f, 1.f);
        dot_.x = draw::approach(dot_.x, target_.x, 18.f);
        dot_.y = draw::approach(dot_.y, target_.y, 18.f);

        ImVec2 c = o + ImVec2(size, size) * 0.5f;
        ImVec2 at = c + dot_ * (size * 0.4f);
        size_t limit = size_t(std::clamp(trailLength_.i, 2, int(points_.size())));
        if (trail_.b) {
            points_[head_] = at - o;
            head_ = (head_ + 1) % points_.size();
            count_ = std::min(count_ + 1, limit);
        } else {
            count_ = 0;
        }

        if (frame_.b) dl->AddRect(o, o + ImVec2(size, size), ImGui::GetColorU32(frameColor_.color), 6 * s, 0, 1.f * s);
        if (cross_.b) {
            ImU32 cc = ImGui::GetColorU32(withAlpha(frameColor_.color, 0.8f));
            dl->AddLine(c - ImVec2(size * 0.1f, 0.f), c + ImVec2(size * 0.1f, 0.f), cc, 1.f * s);
            dl->AddLine(c - ImVec2(0.f, size * 0.1f), c + ImVec2(0.f, size * 0.1f), cc, 1.f * s);
        }
        if (trail_.b)
            for (size_t i = 1; i < count_; i++) {
                size_t start = (head_ + points_.size() - count_) % points_.size();
                float a = float(i) / float(count_);
                dl->AddLine(o + points_[(start + i - 1) % points_.size()], o + points_[(start + i) % points_.size()],
                            theme::col(trailColor_.color, a * 0.7f), dotSize_.f * 0.55f * s * a);
            }
        dl->AddCircleFilled(at, dotSize_.f * s, ImGui::GetColorU32(color_.color));
        return {size, size};
    }

private:
    Setting& boxSize_ = slider("box", "Size", 70.f, 40.f, 160.f, "%.0f");
    Setting& sensitivity_ = slider("sens", "Sensitivity", 1.f, 0.2f, 4.f, "%.1fx");
    Setting& color_ = colorSetting("color", "Dot color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& source_ = choice("source", "Movement from", {"Mouse", "View rotation"});
    Setting& returnSpeed_ = slider("returnSpeed", "Return to the center", 0.2f, 0.02f, 0.6f, "%.2f");
    Setting& dotSize_ = slider("dotSize", "Dot size", 4.5f, 1.5f, 12.f, "%.1f");
    Setting& trail_ = toggleSetting("trail", "Trail", true);
    Setting& trailLength_ = intSlider("trailLength", "Trail length", 24, 2, 80);
    Setting& trailColor_ = colorSetting("trailColor", "Trail color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& frame_ = toggleSetting("frame", "Frame", true);
    Setting& cross_ = toggleSetting("cross", "Center cross", false);
    Setting& frameColor_ = colorSetting("frameColor", "Frame color", {0.62f, 0.62f, 0.68f, 0.3f});
    ImVec2 target_{0, 0};
    ImVec2 dot_{0, 0};
    std::array<ImVec2, 80> points_{};
    size_t head_ = 0;
    size_t count_ = 0;
    float lastYaw_ = 0.f;
    float lastPitch_ = 0.f;
    bool haveView_ = false;
};
