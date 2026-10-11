#pragma once

#include "Probe.hpp"
#include "modules/HudModule.hpp"
#include "modules/common/Colors.hpp"

#include <algorithm>
#include <format>

class PingCounter : public TextHud {
public:
    PingCounter() : TextHud("Ping Counter", "Shows your ping to the server.", {"hud-self"}, {0.005f, 0.306f}) {
        sub("Network");
    }

    void onEnable() override { probe::use(true); }
    void onDisable() override { probe::use(false); }

protected:
    std::string label() const override { return showLabel_.b ? "Ping" : ""; }

    ImU32 valueColor() const override {
        if (!colors_.b || last_ <= 0.f) return textColor();
        return ImGui::GetColorU32(rampColor(last_, 50.f, 150.f, good_.color, mid_.color, bad_.color));
    }

    std::string value() override {
        auto s = probe::metrics();
        if (!s.running || s.received == 0) return "–";
        float ms = s.avg;
        last_ = ms;
        std::string out = std::format("{:.0f}{}", ms, unit_.b ? " ms" : "");
        if (jitter_.b) out += std::format("  ±{:.0f}", s.jitter);
        if (loss_.b && s.loss > 0.05f) out += std::format("  {:.0f}%", s.loss);
        return out;
    }

private:
    Setting& unit_ = toggleSetting("unit", "Show unit", true);
    Setting& jitter_ = toggleSetting("jitter", "Append jitter", false);
    Setting& loss_ = toggleSetting("loss", "Append loss", false);
    Setting& colors_ = toggleSetting("colors", "Color by ping", true);
    Setting& good_ = colorSetting("good", "Good", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Okay", {1.f, 0.82f, 0.49f, 1.f});
    Setting& bad_ = colorSetting("bad", "Bad", {1.f, 0.4f, 0.45f, 1.f});
    float last_ = 0.f;
};
