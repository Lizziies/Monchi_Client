#pragma once

#include "hook/Input.hpp"
#include "modules/HudModule.hpp"

#include <format>

class Cps : public TextHud {
public:
    bool defaultEnabled() const override { return true; }

    Cps() : TextHud("CPS", "Counts your clicks per second. Format placeholders: {lmb} {rmb} {value}.", {"hud-self"}, {0.005f, 0.082f}) {
        sub("Info displays");
    }

protected:
    std::string label() const override { return "CPS"; }

    void tokens(std::vector<std::pair<std::string, std::string>>& map) override {
        map.push_back({"{lmb}", std::to_string(input::cps(MouseButton::Left))});
        map.push_back({"{rmb}", std::to_string(input::cps(MouseButton::Right))});
    }

    std::string value() override {
        int l = input::cps(MouseButton::Left), r = input::cps(MouseButton::Right);
        switch (mode_.i) {
        case 1: return std::to_string(l);
        case 2: return std::to_string(r);
        default: return std::format("{}{}{}", l, separator_.text, r);
        }
    }

private:
    Setting& separator_ = textSetting("separator", "Click separator", " | ");
    Setting& mode_ = choice("mode", "Display", {"Left | Right", "Left only", "Right only"});
};
