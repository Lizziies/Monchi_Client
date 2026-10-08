#pragma once

#include "hook/Dx.hpp"
#include "FpsSamples.hpp"
#include "modules/HudModule.hpp"

#include <algorithm>
#include <array>
#include <format>

class Fps : public TextHud {
public:
    bool defaultEnabled() const override { return true; }

    Fps() : TextHud("FPS", "Shows your frames per second.", {"hud-self"}, {0.005f, 0.05f}) {
        sub("Info displays");
    }

    void onFrame() override {
        double ms = dx::frame().frameMs;
        if (ms <= 0) return;
        samples_[head_] = ms;
        head_ = (head_ + 1) % samples_.size();
        count_ = std::min(count_ + 1, samples_.size());

        elapsed_ += ms;
        frames_++;
        if (elapsed_ >= interval_.f * 1000.0) {
            fps_ = frames_ * 1000.0 / elapsed_;
            frames_ = 0;

            if (lowShown_.b) low_ = fpsSamples::low({samples_.data(), count_});
            elapsed_ = 0;
        }
    }

protected:
    std::string label() const override { return show_.i == 1 ? "MS" : "FPS"; }

    std::string value() override {
        double k = double(std::max(1, spoof_.i)), fps = fps_ * k;
        double ms = fps > 0 ? 1000.0 / fps : 0.0;
        std::string out = show_.i == 1 ? std::format("{:.1f}", ms) : std::format("{:.0f}", fps);
        if (show_.i == 2) out += std::format("  ·  {:.1f} ms", ms);
        if (lowShown_.b) out += std::format("  ·  1%: {:.0f}", low_ * k);
        return out;
    }

private:
    Setting& show_ = choice("show", "Show", {"FPS", "Frame time", "Both"});
    Setting& lowShown_ = toggleSetting("low", "Show 1% low", false);
    Setting& interval_ = slider("interval", "Update interval (s)", 0.5f, 0.1f, 2.f, "%.1f s");
    Setting& spoof_ = intSlider("spoof", "Multiply the shown value (spoof)", 1, 1, 10);
    std::array<double, 512> samples_{};
    size_t head_ = 0;
    size_t count_ = 0;
    double elapsed_ = 0;
    int frames_ = 0;
    double fps_ = 0;
    double low_ = 0;
};
