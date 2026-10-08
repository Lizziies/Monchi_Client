#pragma once

#include "PostFx.hpp"
#include "hook/Input.hpp"
#include "modules/Module.hpp"
#include "modules/common/Context.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>

class Deepfry : public Module {
public:
    Deepfry()
        : Module("Deepfry", "Deep-fries the game image: oversaturated, grainy and harshly stepped. Pure fun.", Category::Fun,
                 {"cosmetic"}) {
        sub("Post effects");
        speed_.visible = [this] { return mode_.i == 1; };
        paintSize_.visible = [this] { return style_.i != 0; };
    }

    void onFrame() override {
        float k = amount_.f;
        if (mode_.i == 1) {
            phase_ = std::fmod(phase_ + ui::dt() * speed_.f * 6.2832f, 6.2832f);
            k *= 0.5f + 0.5f * std::sin(phase_);
        } else if (mode_.i == 2) {
            pulse_ = std::max(input::down(VK_LBUTTON) ? 1.f : 0.f, pulse_ - ui::dt() * 4.f);
            k *= pulse_;
        }
        k = std::clamp(k, 0.f, 1.f);
        auto& p = post::params();
        if (style_.i != 1) p.fry = std::max(p.fry, k);
        if (style_.i != 0) p.paint = std::max(p.paint, paintSize_.f * k * ui::scale());
    }

private:
    Setting& amount_ = slider("amount", "Strength", 0.5f, 0.05f, 1.f, "%.2f");
    Setting& style_ = choice("style", "Style", {"Deep-fried", "Paint", "Both"});
    Setting& paintSize_ = slider("paintSize", "Brush size", 10.f, 2.f, 24.f, "%.0f");
    Setting& mode_ = choice("mode", "Gradient", {"Permanent", "Pulsing", "On click"});
    Setting& speed_ = slider("speed", "Pulse speed (Hz)", 0.6f, 0.1f, 4.f, "%.2f");
    float phase_ = 0.f;
    float pulse_ = 0.f;
};

class UpsideDown : public Module {
public:
    UpsideDown()
        : Module("Upside Down", "Turns the game image upside down. Controls stay normal, on purpose.", Category::Fun,
                 {"cosmetic"}) {
        sub("Post effects");
        hold_.visible = [this] { return holdMode_.b; };
        what_.visible = [] { return fx::available(fx::Id::Fov); };
        axis_.visible = [this] { return !worldOnly(); };
    }

    void onFrame() override {
        if (holdMode_.b && hold_.i && !input::down(hold_.i)) return;
        if (!worldOnly()) {
            post::params().flip = float(axis_.i + 1);
            return;
        }
        // a field of view past 180 degrees turns the projection over, so only the world flips and the HUD stays
        auto& p = game::state().player;
        float base = ctx::fovBase > 0.f ? ctx::fovBase : game::has(game::Domain::Player) ? p.fov : 70.f;
        fx::set(fx::Id::Fov, 360.f - base);
        fx::set(fx::Id::FovEffects, 1.f);
    }

private:
    bool worldOnly() const { return what_.i == 1 && fx::available(fx::Id::Fov); }

    Setting& what_ = choice("what", "What turns", {"Whole picture", "Only the world"});
    Setting& axis_ = choice("axis", "Direction", {"Upside down", "Mirrored", "Both"});

    Setting& holdMode_ = toggleSetting("holdMode", "Only while a key is held", false);
    Setting& hold_ = keySetting("holdKey", "Hold key", VK_F8);
};
