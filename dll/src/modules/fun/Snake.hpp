#pragma once

#include "I18n.hpp"
#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <algorithm>
#include <deque>
#include <format>
#include <random>

class Snake : public Module {
public:
    Snake() : Module("Snake", "Snake for the queue. Arrow keys/WASD, ESC quits.", Category::Fun, {"cosmetic"}) {
        sub("Games");
    }

    bool persistent() const override { return false; }
    void onEnable() override { reset(); }

    void onFrame() override {
        if (gui::open()) return;
        gui::claimKeyboard();

        auto pressed = [](ImGuiKey a, ImGuiKey b) { return ImGui::IsKeyPressed(a, false) || ImGui::IsKeyPressed(b, false); };
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            setEnabled(false);
            return;
        }
        if (pressed(ImGuiKey_UpArrow, ImGuiKey_W) && dir_.y == 0) next_ = {0, -1};
        if (pressed(ImGuiKey_DownArrow, ImGuiKey_S) && dir_.y == 0) next_ = {0, 1};
        if (pressed(ImGuiKey_LeftArrow, ImGuiKey_A) && dir_.x == 0) next_ = {-1, 0};
        if (pressed(ImGuiKey_RightArrow, ImGuiKey_D) && dir_.x == 0) next_ = {1, 0};
        if (dead_ && ImGui::IsKeyPressed(ImGuiKey_Space, false)) reset();

        step_ += ui::dt();
        float interval = std::max(0.05f, 0.14f - body_.size() * 0.002f);
        while (!dead_ && step_ >= interval) {
            step_ -= interval;
            tick();
        }
        draw();
    }

private:
    static constexpr int W = 24, H = 16;

    void reset() {
        body_ = {{W / 2, H / 2}, {W / 2 - 1, H / 2}, {W / 2 - 2, H / 2}};
        dir_ = next_ = {1, 0};
        dead_ = false;
        step_ = 0;
        place();
    }

    void place() {
        std::uniform_int_distribution<int> x(0, W - 1), y(0, H - 1);
        do food_ = {float(x(rng_)), float(y(rng_))};
        while (std::any_of(body_.begin(), body_.end(), [&](auto& p) { return p.x == food_.x && p.y == food_.y; }));
    }

    void tick() {
        dir_ = next_;
        ImVec2 head = body_.front() + dir_;
        if (head.x < 0 || head.y < 0 || head.x >= W || head.y >= H ||
            std::any_of(body_.begin(), body_.end(), [&](auto& p) { return p.x == head.x && p.y == head.y; })) {
            dead_ = true;
            best_ = std::max(best_, (int)body_.size() - 3);
            return;
        }
        body_.push_front(head);
        if (head.x == food_.x && head.y == food_.y) place();
        else body_.pop_back();
    }

    void draw() {
        auto& t = theme::current();
        float s = ui::scale();
        auto* dl = ImGui::GetForegroundDrawList();
        auto ds = ImGui::GetIO().DisplaySize;
        float cell = 22 * s;
        ImVec2 size{W * cell, H * cell};
        ImVec2 o = (ds - size) * 0.5f;

        dl->AddRectFilled(o - ImVec2(14 * s, 48 * s), o + size + ImVec2(14 * s, 14 * s), theme::col(t.bg, 0.95f), t.rounding * s);
        std::string title = i18n::fmt("Snake  ·  {}  ·  Best {}", (int)body_.size() - 3, best_);
        dl->AddText(fonts::bold(), 20 * s, o - ImVec2(0, 36 * s), theme::col(t.text), title.c_str());
        dl->AddRectFilled(o, o + size, theme::col(t.surface), 6 * s);

        draw::heart(dl, o + (food_ + ImVec2(0.5f, 0.55f)) * cell, cell * 0.9f, theme::col(t.accent));
        for (size_t i = 0; i < body_.size(); i++) {
            float k = float(i) / body_.size();
            ImVec2 p = o + body_[i] * cell;
            dl->AddRectFilled(p + ImVec2(2, 2), p + ImVec2(cell - 2, cell - 2), theme::col(theme::mix(t.accent2, t.accent, k)), 5 * s);
        }
        if (dead_)
            draw::textCentered(dl, fonts::bold(), 22 * s, o + size * 0.5f, theme::col(t.text), i18n::tr("Space = again, ESC = quit"));
    }

    std::deque<ImVec2> body_;
    ImVec2 dir_{1, 0}, next_{1, 0}, food_{0, 0};
    bool dead_ = false;
    float step_ = 0;
    int best_ = 0;
    std::mt19937 rng_{std::random_device{}()};
};
