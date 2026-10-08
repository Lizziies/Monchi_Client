#pragma once

#include "gui/Gui.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "sdk/Game.hpp"

#include <cmath>

class PitchDisplay : public GameText {
public:
    PitchDisplay()
        : GameText("Pitch Display", "Shows how far you look up or down, for example to hold the best elytra angle.", need::player,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.005f, 0.402f}) {
        sub("Info displays");
        glideOnly_.visible = [] { return need::have("MoveState"); };
        target_.visible = band_.visible = [this] { return colored_.b; };
        bandColor_.visible = [this] { return colored_.b; };
    }

    void onRender(ImDrawList* dl) override {
        if (!gui::editingHud() && !shown()) return;
        GameText::onRender(dl);
    }

protected:
    std::string label() const override { return showLabel_.b ? i18n::tr("Pitch") : ""; }

    std::string value() override {
        float p = game::state().player.pitch;
        if (invert_.b) p = -p;
        return text::num(p, decimals_.i) + (degree_.b ? "°" : "");
    }

    ImU32 valueColor() const override {
        if (!colored_.b || std::abs(game::state().player.pitch - target_.f) > band_.f) return textColor();
        return ImGui::GetColorU32(bandColor_.color);
    }

private:
    bool shown() const {
        auto& p = game::state().player;
        if (elytraOnly_.b && p.armor[1].name != "elytra") return false;
        return !glideOnly_.b || !need::have("MoveState") || p.gliding;
    }

    Setting& decimals_ = intSlider("decimals", "Decimals", 1, 0, 3);
    Setting& degree_ = toggleSetting("degree", "Degree sign", true);
    Setting& invert_ = toggleSetting("invert", "Up is positive", false);
    Setting& elytraOnly_ = toggleSetting("onlyRenderWhenElytraEquipped", "Only with an elytra on", false);
    Setting& glideOnly_ = toggleSetting("glideOnly", "Only while gliding", false);
    Setting& colored_ = toggleSetting("colored", "Color near a target angle", false);
    Setting& target_ = slider("target", "Target angle", -2.f, -90.f, 90.f, "%.1f°");
    Setting& band_ = slider("band", "Tolerance (°)", 3.f, 0.5f, 20.f, "%.1f°");
    Setting& bandColor_ = colorSetting("bandColor", "Color at the target", {0.55f, 0.91f, 0.69f, 1.f});
};
