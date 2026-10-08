#pragma once

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "hook/Input.hpp"
#include "modules/HudModule.hpp"
#include "modules/Module.hpp"
#include "modules/common/Particles.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>
#include <random>

class Pet : public HudModule {
public:
    Pet()
        : HudModule("Pet", "A little Monchi pet in the HUD: bounces, cheers on hits, sleeps when idle.", {"cosmetic"},
                    {0.845f, 0.84f}) {
        sub("Games");
        background_.b = false;
    }

    void onFrame() override {
        joy_ = std::max(0.f, joy_ - ui::dt() * 1.8f);
        double now = ui::time();
        int64_t click = input::lastClickQpc();
        bool clicked = click && click != seenClick_;
        seenClick_ = click;
        bool moved = input::lastMoveQpc() != seenMove_;
        seenMove_ = input::lastMoveQpc();
        if (clicked || moved) lastInput_ = now;
        for (auto& e : game::events()) {
            if (e.kind == game::EventKind::Hit || e.kind == game::EventKind::Kill) {
                joy_ = 1.f;
                if (hearts_.b) burst_ = true;
            }
        }
        if (clicked && !game::has(game::Domain::Combat)) joy_ = std::max(joy_, 0.6f);
    }

    void onRender(ImDrawList* dl) override {
        HudModule::onRender(dl);
        if (!particles_.empty()) particles_.draw(dl);
    }
    void onDisable() override { particles_.clear(); burst_ = false; joy_ = 0.f; }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& t = theme::current();
        double now = ui::time();
        float size = size_.f * s;
        bool asleep = sleepy_.b && now - lastInput_ > sleepAfter_.f;

        float bob = std::sin(float(now) * (asleep ? 1.2f : 3.f)) * (asleep ? 0.015f : 0.04f);
        float jump = joy_ > 0.f ? std::sin(joy_ * 3.1416f) * 0.35f : 0.f;
        float squish = 1.f + bob - jump * 0.2f;
        ImVec2 base = o + ImVec2(size * 0.6f, size * 1.15f);
        ImVec2 c = base - ImVec2(0, size * (0.45f + jump));
        ImVec2 r{size * 0.5f / squish, size * 0.42f * squish};
        ImU32 body = ImGui::GetColorU32(color_.color);
        dl->AddEllipseFilled(base + ImVec2(0, 2 * s), {size * 0.42f, size * 0.07f}, IM_COL32(0, 0, 0, 40), 0.f, 24);
        dl->AddEllipseFilled(c, r, body, 0.f, 36);
        dl->AddEllipse(c, r, theme::col(t.accent, 0.6f), 0.f, 36, 1.5f * s);

        float blink = std::fmod(float(now), 4.f) < 0.15f ? 0.1f : 1.f;
        ImVec2 eyeL = c + ImVec2(-r.x * 0.38f, -r.y * 0.1f), eyeR = c + ImVec2(r.x * 0.38f, -r.y * 0.1f);
        ImU32 ink = IM_COL32(60, 40, 60, 255);
        if (asleep) {
            dl->AddLine(eyeL - ImVec2(4 * s, 0), eyeL + ImVec2(4 * s, 0), ink, 2.f * s);
            dl->AddLine(eyeR - ImVec2(4 * s, 0), eyeR + ImVec2(4 * s, 0), ink, 2.f * s);
            dl->AddText(fonts::bold(), 14.f * s, c + ImVec2(r.x * 0.7f, -r.y * 1.1f - std::fmod(float(now), 2.f) * 6 * s), theme::col(t.textDim), "z");
        } else if (joy_ > 0.1f) {
            dl->AddLine(eyeL + ImVec2(-4 * s, 2 * s), eyeL + ImVec2(0, -3 * s), ink, 2.f * s);
            dl->AddLine(eyeL + ImVec2(0, -3 * s), eyeL + ImVec2(4 * s, 2 * s), ink, 2.f * s);
            dl->AddLine(eyeR + ImVec2(-4 * s, 2 * s), eyeR + ImVec2(0, -3 * s), ink, 2.f * s);
            dl->AddLine(eyeR + ImVec2(0, -3 * s), eyeR + ImVec2(4 * s, 2 * s), ink, 2.f * s);
        } else {
            dl->AddEllipseFilled(eyeL, {3.f * s, 4.f * s * blink}, ink, 0.f, 12);
            dl->AddEllipseFilled(eyeR, {3.f * s, 4.f * s * blink}, ink, 0.f, 12);
        }
        dl->AddCircleFilled(c + ImVec2(-r.x * 0.62f, r.y * 0.2f), 4.5f * s, IM_COL32(255, 140, 170, 120));
        dl->AddCircleFilled(c + ImVec2(r.x * 0.62f, r.y * 0.2f), 4.5f * s, IM_COL32(255, 140, 170, 120));
        if (joy_ > 0.1f) dl->AddCircleFilled(c + ImVec2(0, r.y * 0.28f), 3.f * s, ink);
        else {
            dl->AddLine(c + ImVec2(-3 * s, r.y * 0.25f), c + ImVec2(0, r.y * 0.32f), ink, 1.5f * s);
            dl->AddLine(c + ImVec2(0, r.y * 0.32f), c + ImVec2(3 * s, r.y * 0.25f), ink, 1.5f * s);
        }
        if (burst_) {
            particles_.burst(c - ImVec2(0, r.y), 5, Particles::Shape::Heart, theme::col(t.accent), 14.f * s, 0.9f, 90.f * s, -40.f);
            burst_ = false;
        }
        return {size * 1.2f, size * 1.2f};
    }

private:
    Setting& size_ = slider("size", "Size", 60.f, 30.f, 140.f, "%.0f");
    Setting& color_ = colorSetting("color", "Color", {1.f, 0.93f, 0.95f, 1.f});
    Setting& sleepy_ = toggleSetting("sleepy", "Sleeps when idle", true);
    Setting& sleepAfter_ = slider("sleepAfter", "Falls asleep after (s)", 20.f, 5.f, 120.f, "%.0f s");
    Setting& hearts_ = toggleSetting("hearts", "Hearts on hits", true);
    Particles particles_;
    float joy_ = 0.f;
    bool burst_ = false;
    int64_t seenClick_ = 0;
    int64_t seenMove_ = 0;
    double lastInput_ = 0.0;
};

class Petals : public Module {
public:
    Petals()
        : Module("Petals", "Sakura petals fall gently across the screen.", Category::Fun, {"cosmetic"}) {
        sub("Games");
    }

    void onRender(ImDrawList* dl) override {
        if (menuOnly_.b && !gui::open()) return;
        auto ds = ImGui::GetIO().DisplaySize;
        float dt = ui::dt(), s = ui::scale();
        while ((int)list_.size() < count_.i) list_.push_back(make(ds, true));
        while ((int)list_.size() > count_.i) list_.pop_back();
        for (auto& p : list_) {
            p.t += dt;
            p.y += p.fall * speed_.f * s * dt;
            p.x += (std::sin(p.t * p.sway + p.phase) * 18.f + wind_.f) * s * dt;
            p.rot += p.spin * dt;
            if (p.y > ds.y + 20 || p.x > ds.x + 40 || p.x < -40) p = make(ds, false);
            float size = p.size * size_.f * s;
            ImVec4 c = color_.color;
            c.w *= opacity_.f * p.alpha;
            ImU32 col = ImGui::GetColorU32(c);
            float ca = std::cos(p.rot), sa = std::sin(p.rot);
            auto rot = [&](float x, float y) { return ImVec2(p.x + x * ca - y * sa, p.y + x * sa + y * ca); };
            ImVec2 pts[6] = {rot(0, -size), rot(size * 0.55f, -size * 0.35f), rot(size * 0.4f, size * 0.55f), rot(0, size * 0.35f), rot(-size * 0.4f, size * 0.55f), rot(-size * 0.55f, -size * 0.35f)};
            dl->AddConcavePolyFilled(pts, 6, col);
        }
    }

private:
    struct P {
        float x, y, fall, sway, phase, rot, spin, size, alpha, t;
    };

    P make(ImVec2 ds, bool anywhere) {
        std::uniform_real_distribution<float> u(0.f, 1.f);
        P p{};
        p.x = u(rng_) * ds.x;
        p.y = anywhere ? u(rng_) * ds.y : -20.f;
        p.fall = 30.f + u(rng_) * 50.f;
        p.sway = 0.8f + u(rng_) * 1.4f;
        p.phase = u(rng_) * 6.28f;
        p.rot = u(rng_) * 6.28f;
        p.spin = (u(rng_) - 0.5f) * 3.f;
        p.size = 5.f + u(rng_) * 6.f;
        p.alpha = 0.55f + u(rng_) * 0.45f;
        return p;
    }

    Setting& count_ = intSlider("count", "Count", 40, 5, 200);
    Setting& speed_ = slider("speed", "Fall speed", 1.f, 0.2f, 4.f, "%.1fx");
    Setting& wind_ = slider("wind", "Wind", 10.f, -80.f, 80.f, "%.0f");
    Setting& size_ = slider("size", "Size", 1.f, 0.5f, 3.f, "%.1fx");
    Setting& opacity_ = slider("opacity", "Opacity", 0.8f, 0.1f, 1.f, "%.2f");
    Setting& color_ = colorSetting("color", "Color", {1.f, 0.72f, 0.82f, 1.f});
    Setting& menuOnly_ = toggleSetting("menuOnly", "Only while the menu is open", false);
    std::vector<P> list_;
    std::mt19937 rng_{std::random_device{}()};
};
