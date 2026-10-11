#pragma once

#include "I18n.hpp"
#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <algorithm>
#include <format>
#include <random>
#include <vector>

class Flappy : public Module {
public:
    Flappy() : Module("Flappy Heart", "Flappy Bird with a heart. Space flies, ESC quits.", Category::Fun, {"cosmetic"}) {
        sub("Games");
    }

    bool persistent() const override { return false; }
    void onEnable() override { reset(); }

    void onFrame() override {
        if (gui::open()) return;
        gui::claimKeyboard();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            setEnabled(false);
            return;
        }
        bool flap = ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (flap) {
            if (dead_) reset();
            started_ = true;
            vel_ = -330.f;
        }

        float dt = std::min(ui::dt(), 0.05f);
        if (started_ && !dead_) {
            vel_ += 900.f * dt;
            y_ += vel_ * dt;
            spawn_ -= dt;
            if (spawn_ <= 0) {
                std::uniform_real_distribution<float> gapY(110.f, H - 110.f);
                pipes_.push_back({W + 40.f, gapY(rng_), false});
                spawn_ = 1.45f;
            }
            for (auto& p : pipes_) {
                p.x -= 160.f * dt;
                if (!p.scored && p.x + 30 < X) {
                    p.scored = true;
                    score_++;
                    best_ = std::max(best_, score_);
                }
                if (X + 12 > p.x - 30 && X - 12 < p.x + 30 && (y_ - 12 < p.gap - 70 || y_ + 12 > p.gap + 70)) dead_ = true;
            }
            std::erase_if(pipes_, [](auto& p) { return p.x < -60; });
            if (y_ > H - 10 || y_ < 0) dead_ = true;
        }
        draw();
    }

private:
    static constexpr float W = 480, H = 360, X = 120;

    struct Pipe {
        float x, gap;
        bool scored;
    };

    void reset() {
        y_ = H * 0.5f;
        vel_ = 0;
        pipes_.clear();
        spawn_ = 0.6f;
        score_ = 0;
        dead_ = false;
        started_ = false;
    }

    void draw() {
        auto& t = theme::current();
        float s = ui::scale();
        auto* dl = ImGui::GetForegroundDrawList();
        auto ds = ImGui::GetIO().DisplaySize;
        ImVec2 size{W * s, H * s};
        ImVec2 o = (ds - size) * 0.5f;

        dl->AddRectFilled(o - ImVec2(12 * s, 44 * s), o + size + ImVec2(12 * s, 12 * s), theme::col(t.bg, 0.95f), t.rounding * s);
        dl->AddText(fonts::bold(), 20 * s, o - ImVec2(0, 34 * s), theme::col(t.text),
                    i18n::fmt("Flappy Heart  ·  {}  ·  Best {}", score_, best_).c_str());
        dl->PushClipRect(o, o + size, true);
        draw::gradientRect(dl, o, o + size, theme::col(t.surface), theme::col(t.surfaceHover), 6 * s);
        for (auto& p : pipes_) {
            ImVec2 a = o + ImVec2((p.x - 30) * s, 0), b = o + ImVec2((p.x + 30) * s, (p.gap - 70) * s);
            dl->AddRectFilled(a, b, theme::col(t.accent2), 6 * s);
            dl->AddRectFilled(o + ImVec2((p.x - 30) * s, (p.gap + 70) * s), o + ImVec2((p.x + 30) * s, H * s), theme::col(t.accent2), 6 * s);
        }
        draw::heart(dl, o + ImVec2(X * s, y_ * s), 30 * s, theme::col(t.accent));
        dl->PopClipRect();
        if (!started_ || dead_)
            draw::textCentered(dl, fonts::bold(), 20 * s, o + size * 0.5f + ImVec2(0, 60 * s), theme::col(t.text),
                               i18n::tr(dead_ ? "Space = again" : "Space to start"));
    }

    float y_ = 0, vel_ = 0, spawn_ = 0;
    std::vector<Pipe> pipes_;
    int score_ = 0, best_ = 0;
    bool dead_ = false, started_ = false;
    std::mt19937 rng_{std::random_device{}()};
};
