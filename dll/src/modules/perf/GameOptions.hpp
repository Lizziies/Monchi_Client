#pragma once

#include "modules/Module.hpp"

#include <format>

// Holds some of Minecraft's own video settings at their cheapest while the module is on. Nothing the settings menu
// could not do, but it takes one key, can follow a server profile, and gives the saved settings back untouched.
class PerformanceMode : public Module {
public:
    PerformanceMode()
        : Module("Performance Mode", "Switches Minecraft's own costly video settings off while it is on and gives them back afterwards: smooth lighting, see-through leaves, clouds, bubbles, far particles. Lighting and leaves change as chunks are loaded anew.",
                 Category::Performance, {"performance"}) {
        sub("Frame timing");
        require(0, {"LocalPlayer"});
    }

    void onFrame() override {
        if (lighting_.b) fx::hold("gfx_smoothlighting", fx::Hold::Off);
        if (leaves_.b) fx::hold("gfx_transparentleaves", fx::Hold::Off);
        if (clouds_.b) fx::hold("gfx_toggleclouds", fx::Hold::Off);
        if (bubbles_.b) fx::hold("gfx_bubble_particles", fx::Hold::Off);
        if (particles_.b) fx::hold("gfx_particleviewdistance", fx::Hold::Lowest);
        if (screens_.b) fx::hold("screen_animations", fx::Hold::Off);
        if (tips_.b) fx::hold("game_tips_animation_enabled", fx::Hold::Off);
        if (darkness_.b) fx::hold("darkness_effect_modifier", fx::Hold::Lowest);
    }

    void drawSettings() override {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", i18n::fmt("Settings held right now: {}", fx::held()).c_str());
        if (fx::heldMissing()) ImGui::TextDisabled("%s", i18n::fmt("Not known on this game version: {}", fx::heldMissing()).c_str());
    }

    std::string proof() const override {
        return std::format("{} of Minecraft's video settings held, {} not known on this version{}", fx::held(), fx::heldMissing(), fx::held() ? "" : ", 0 times");
    }

private:
    Setting& lighting_ = toggleSetting("lighting", "Smooth lighting off", true);
    Setting& leaves_ = toggleSetting("leaves", "See-through leaves off", true);
    Setting& clouds_ = toggleSetting("clouds", "Clouds off", true);
    Setting& bubbles_ = toggleSetting("bubbles", "Bubble particles off", true);
    Setting& particles_ = toggleSetting("particles", "Particles only close by", true);
    Setting& screens_ = toggleSetting("screens", "Screen animations off (menus open at once)", true);
    Setting& tips_ = toggleSetting("tips", "Game tip animations off", true);
    Setting& darkness_ = toggleSetting("darkness", "Darkness effect at its weakest", false);
};
