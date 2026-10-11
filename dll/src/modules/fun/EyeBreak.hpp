#pragma once

#include "I18n.hpp"
#include "gui/Notify.hpp"
#include "modules/Module.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>

class EyeBreak : public Module {
public:
    EyeBreak()
        : Module("20-20-20", "Reminds you every 20 minutes to look at something 20 feet (6 m) away for 20 seconds.",
                 Category::Fun, {"cosmetic"}) {
        sub("Games");
        countdown_.visible = [this] { return blackout_.b; };
    }

    void onEnable() override {
        last_ = ui::time();
        breakUntil_ = 0.0;
        inputState_ = 0;
        skipRequested_ = false;
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && ev.vk == VK_ESCAPE && inputState_.load(std::memory_order_relaxed) == 3) skipRequested_ = true;
    }

    void onFrame() override {
        if (skipRequested_.exchange(false) && skippable_.b) breakUntil_ = 0.0;
        if (ui::time() - last_ >= interval_.f * 60.0) {
            last_ = ui::time();
            breakUntil_ = last_ + length_.f;
            if (!blackout_.b) notify::push(i18n::tr("Eye break"), i18n::fmt("Look into the distance for {:.0f} seconds.", length_.f), notify::Kind::Info, length_.f);
        }
        inputState_.store((skippable_.b ? 1u : 0u) | (resting() ? 2u : 0u), std::memory_order_relaxed);
    }

    void onRender(ImDrawList* dl) override {
        if (!blackout_.b || !resting()) return;
        auto ds = ImGui::GetIO().DisplaySize;
        float left = float(breakUntil_ - ui::time());
        float fade = std::clamp(std::min(left, length_.f - left) * 2.f, 0.f, 1.f);
        dl->AddRectFilled({0, 0}, ds, IM_COL32(0, 0, 0, int(255 * fade)));
        if (!countdown_.b) return;
        float s = ui::scale();
        ImU32 col = IM_COL32(255, 255, 255, int(200 * fade));
        draw::textCentered(dl, fonts::bold(), 22.f * s, {ds.x * 0.5f, ds.y * 0.5f - 14 * s}, col, i18n::tr("Look into the distance"));
        draw::textCentered(dl, fonts::bold(), 40.f * s, {ds.x * 0.5f, ds.y * 0.5f + 22 * s}, col, std::to_string(int(std::ceil(left))).c_str());
    }

private:
    bool resting() const { return ui::time() < breakUntil_; }

    Setting& interval_ = slider("interval", "Every (minutes)", 20.f, 5.f, 60.f, "%.0f min");
    Setting& length_ = slider("length", "Break length (s)", 20.f, 5.f, 60.f, "%.0f s");
    Setting& blackout_ = toggleSetting("extreme", "Extreme mode: black screen during the break", false);
    Setting& countdown_ = toggleSetting("countdown", "Countdown on the black screen", true);
    Setting& skippable_ = toggleSetting("skippable", "Esc ends the break early", true);
    std::atomic<unsigned> inputState_{0};
    std::atomic<bool> skipRequested_{false};
    double last_ = 0;
    double breakUntil_ = 0;
};
