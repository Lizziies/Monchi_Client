#pragma once

#include "gui/Gui.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "hook/Input.hpp"
#include "modules/Module.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Particles.hpp"
#include "modules/common/Sounds.hpp"
#include "modules/common/Text.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <cstring>
#include <format>
#include <random>

inline ImVec2 screenCenter() {
    auto ds = ImGui::GetIO().DisplaySize;
    return {std::floor(ds.x * 0.5f) + 0.5f, std::floor(ds.y * 0.5f) + 0.5f};
}

class HitMarker : public Module {
public:
    HitMarker()
        : Module("Hit Marker", "A short marker at the crosshair when you hit. Display only, changes nothing in the game.", Category::Pvp,
                 {"cosmetic"}) {
        sub("Hit visuals");
        critColor_.visible = [] { return need::have("MoveState"); };
    }

    void onFrame() override {
        if (source_.i == 1) {
            int64_t click = input::lastClickQpc();
            if (click && click != seen_) {
                seen_ = click;
                spawn(false);
            }
            return;
        }
        for (auto& e : game::events())
            if (e.kind == game::EventKind::Hit) spawn(e.crit);
    }

    void onRender(ImDrawList* dl) override {
        ImVec2 c = screenCenter();
        double now = ui::time();
        for (auto& m : marks_) {
            float k = float((now - m.at) / duration_.f);
            if (k >= 1.f) continue;
            float grow = grow_.b ? 1.f + k * 0.5f : 1.f;
            float size = size_.f * grow;
            ImVec4 col = m.crit ? critColor_.color : color_.color;
            col.w *= (1.f - k) * (1.f - k);
            ImU32 u = ImGui::GetColorU32(col);
            ImU32 shadow = IM_COL32(0, 0, 0, int(160 * col.w));
            float g = gap_.f + (k * 3.f);
            auto stroke = [&](ImVec2 a, ImVec2 b) {
                if (outline_.b) dl->AddLine(a, b, shadow, thickness_.f + 2.f);
                dl->AddLine(a, b, u, thickness_.f);
            };
            switch (style_.i) {
            case 0:
                for (int sx : {-1, 1})
                    for (int sy : {-1, 1}) stroke(c + ImVec2(sx * g, sy * g), c + ImVec2(sx * (g + size), sy * (g + size)));
                break;
            case 1:
                for (int a = 0; a < 4; a++) {
                    ImVec2 d = a == 0 ? ImVec2(1, 0) : a == 1 ? ImVec2(-1, 0) : a == 2 ? ImVec2(0, 1) : ImVec2(0, -1);
                    stroke(c + d * g, c + d * (g + size));
                }
                break;
            case 2:
                dl->AddCircle(c, g + size * 0.5f, u, 24, thickness_.f);
                break;
            case 3:
                for (int a = 0; a < 4; a++) {
                    float ang = 0.7854f + a * 1.5708f;
                    dl->AddCircleFilled(c + ImVec2(std::cos(ang), std::sin(ang)) * (g + size * 0.5f), thickness_.f * 1.2f, u);
                }
                break;
            }
        }
        std::erase_if(marks_, [&](const Mark& m) { return now - m.at > duration_.f; });
    }

    void drawSettings() override {
        ImGui::Spacing();
        if (source_.i == 0 && !game::ready(need::combat)) ImGui::TextDisabled(i18n::tr("Hit detection needs the AttackEntity signature. \"Every click\" always works."));
    }

private:
    struct Mark {
        double at;
        bool crit;
    };

    void spawn(bool crit) {
        marks_.push_back({ui::time(), crit});
        if (marks_.size() > 8) marks_.pop_front();
    }

    Setting& source_ = choice("source", "Trigger", {"Hits", "Every click"});
    Setting& style_ = choice("style", "Shape", {"X", "Plus", "Ring", "Dots"});
    Setting& size_ = slider("size", "Length", 7.f, 2.f, 24.f, "%.0f");
    Setting& gap_ = slider("gap", "Gap", 5.f, 0.f, 24.f, "%.0f");
    Setting& thickness_ = slider("thickness", "Thickness", 2.f, 1.f, 5.f, "%.1f");
    Setting& duration_ = slider("duration", "Duration (s)", 0.25f, 0.08f, 1.f, "%.2f s");
    Setting& grow_ = toggleSetting("grow", "Grows while fading", true);
    Setting& outline_ = toggleSetting("outline", "Outline", true);
    Setting& color_ = colorSetting("color", "Color", {1.f, 1.f, 1.f, 1.f});
    Setting& critColor_ = colorSetting("critColor", "Color on crit", {0.23f, 0.65f, 0.93f, 1.f});
    std::deque<Mark> marks_;
    int64_t seen_ = 0;
};

class HitEffects : public Module {
public:
    HitEffects()
        : Module("Hit Effects", "Particles and sparks at the crosshair on hits. Local only, visible only to you.", Category::Pvp,
                 {"cosmetic"}) {
        sub("Hit visuals");
        require(need::combat, need::sigs({"LocalPlayer"}));
        critColor_.visible = [this] { return !onlyCrit_.b && need::have("MoveState"); };
        onlyCrit_.visible = [] { return need::have("MoveState"); };
    }

    void onFrame() override {
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Hit) continue;
            if (onlyCrit_.b && need::have("MoveState") && !e.crit) continue;
            ImVec4 c = e.crit ? critColor_.color : color_.color;
            if (rainbow_.b) {
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(std::fmod(float(ui::time()), 1.f), 0.5f, 1.f, r, g, b);
                c = {r, g, b, 1.f};
            }
            particles_.burst(screenCenter() + ImVec2(offsetX_.f, offsetY_.f), count_.i + (e.crit ? count_.i / 2 : 0),
                             Particles::Shape(style_.i), ImGui::GetColorU32(c), size_.f, life_.f, speed_.f, gravity_.f);
        }
    }

    void onRender(ImDrawList* dl) override { particles_.draw(dl); }
    void onDisable() override { particles_.clear(); }

private:
    Setting& style_ = choice("style", "Particles", {"Dots", "Hearts", "Sparks", "Stars"}, 1);
    Setting& count_ = intSlider("count", "Amount", 8, 2, 40);
    Setting& size_ = slider("size", "Size", 14.f, 4.f, 40.f, "%.0f");
    Setting& life_ = slider("life", "Lifetime (s)", 0.6f, 0.2f, 2.f, "%.1f s");
    Setting& speed_ = slider("speed", "Speed", 220.f, 40.f, 600.f, "%.0f");
    Setting& gravity_ = slider("gravity", "Gravity", 180.f, -200.f, 600.f, "%.0f");
    Setting& onlyCrit_ = toggleSetting("onlyCrit", "Only on crit", false);
    Setting& rainbow_ = toggleSetting("rainbow", "Rainbow", false);
    Setting& color_ = colorSetting("color", "Color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& critColor_ = colorSetting("critColor", "Color on crit", {1.f, 0.85f, 0.4f, 1.f});
    Setting& offsetX_ = slider("offsetX", "Offset X", 0.f, -200.f, 200.f, "%.0f");
    Setting& offsetY_ = slider("offsetY", "Offset Y", 0.f, -200.f, 200.f, "%.0f");
    Particles particles_;
};

class KillEffects : public Module {
public:
    KillEffects()
        : Module("Kill Effects", "A rain of hearts and text on screen when you get a kill. Local only.", Category::Pvp,
                 {"cosmetic"}) {
        sub("Hit visuals");
        require(need::combat, need::sigs({"LocalPlayer", "KillEvents"}));
    }

    void onFrame() override {
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Kill) continue;
            auto ds = ImGui::GetIO().DisplaySize;
            particles_.burst({ds.x * 0.5f, ds.y * 0.4f}, count_.i, Particles::Shape(style_.i), ImGui::GetColorU32(color_.color), 22.f, 1.2f, 320.f, 120.f);
            if (title_.b) {
                title_text_ = e.text.empty() ? i18n::tr("Kill!") : i18n::tr("Kill: ") + e.text;
                title_at_ = ui::time();
            }
        }
    }

    void onRender(ImDrawList* dl) override {
        particles_.draw(dl);
        double age = ui::time() - title_at_;
        if (!title_.b || age > 1.4) return;
        auto ds = ImGui::GetIO().DisplaySize;
        float k = float(age / 1.4);
        ImVec4 c = color_.color;
        c.w *= 1.f - k * k;
        draw::textCentered(dl, fonts::bold(), 30.f * ui::scale() * (1.f + 0.2f * (1.f - k)), {ds.x * 0.5f, ds.y * 0.32f - k * 24.f}, ImGui::GetColorU32(c),
                           title_text_.c_str());
    }

    void onDisable() override { particles_.clear(); title_text_.clear(); title_at_ = -10.0; }

private:
    Setting& style_ = choice("style", "Particles", {"Dots", "Hearts", "Sparks", "Stars"}, 1);
    Setting& count_ = intSlider("count", "Amount", 28, 5, 120);
    Setting& title_ = toggleSetting("title", "Show text", true);
    Setting& color_ = colorSetting("color", "Color", {0.23f, 0.65f, 0.93f, 1.f});
    std::string title_text_;
    double title_at_ = -10.0;
    Particles particles_;
};

class DamageIndicator : public Module {
public:
    DamageIndicator()
        : Module("Damage Indicator", "Damage numbers rise from the crosshair when you hit.",
                 Category::Pvp, {"info-others"}) {
        sub("Combat displays");
        require(need::combat, need::sigs({"LocalPlayer", "HitConfirm"}));
    }

    void onFrame() override {
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Confirm) continue;
            std::uniform_real_distribution<float> j(-spread_.f, spread_.f);
            numbers_.push_back({ui::time(), j(rng_), e.damage, e.crit});
        }
        while (numbers_.size() > 12) numbers_.pop_front();
    }

    void onRender(ImDrawList* dl) override {
        double now = ui::time();
        ImVec2 c = screenCenter() + ImVec2(offsetX_.f, offsetY_.f);
        for (auto& n : numbers_) {
            float k = float((now - n.at) / life_.f);
            if (k >= 1.f) continue;
            std::string s = n.value > 0.f ? (hearts_.b ? text::num(n.value / 2.f, 1) + " ♥" : text::num(n.value, 1)) : i18n::tr("Hit");
            ImVec4 col = n.crit ? critColor_.color : color_.color;
            col.w *= 1.f - k * k;
            float size = size_.f * ui::scale() * (n.crit ? 1.25f : 1.f) * (1.f + 0.3f * std::max(0.f, 0.25f - k) * 4.f);
            ImVec2 p = c + ImVec2(n.x, -rise_.f * k);
            ImFont* f = fonts::bold();
            if (shadow_.b) dl->AddText(f, size, p + ImVec2(1.5f, 1.5f), IM_COL32(0, 0, 0, int(200 * col.w)), s.c_str());
            dl->AddText(f, size, p, ImGui::GetColorU32(col), s.c_str());
        }
        std::erase_if(numbers_, [&](const Num& n) { return now - n.at > life_.f; });
    }

private:
    struct Num {
        double at;
        float x;
        float value;
        bool crit;
    };

    Setting& size_ = slider("size", "Size", 22.f, 10.f, 48.f, "%.0f");
    Setting& life_ = slider("life", "Duration (s)", 0.9f, 0.3f, 3.f, "%.1f s");
    Setting& rise_ = slider("rise", "Rise", 70.f, 10.f, 200.f, "%.0f");
    Setting& spread_ = slider("spread", "Spread", 40.f, 0.f, 120.f, "%.0f");
    Setting& hearts_ = toggleSetting("hearts", "Show in hearts", false);
    Setting& shadow_ = toggleSetting("shadow", "Shadow", true);
    Setting& color_ = colorSetting("color", "Color", {1.f, 1.f, 1.f, 1.f});
    Setting& critColor_ = colorSetting("critColor", "Color on crit", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& offsetX_ = slider("offsetX", "Offset X", 40.f, -200.f, 200.f, "%.0f");
    Setting& offsetY_ = slider("offsetY", "Offset Y", -20.f, -200.f, 200.f, "%.0f");
    std::deque<Num> numbers_;
    std::mt19937 rng_{std::random_device{}()};
};

class HitSound : public Module {
public:
    HitSound()
        : Module("Hit Sound", "Plays your own sound on every hit. Local only, creates no network packet.", Category::Pvp,
                 {"cosmetic"}) {
        sub("Hit visuals");
        require(need::combat, need::sigs({"LocalPlayer"}));
        critTone_.visible = [] { return need::have("MoveState"); };
    }

    void onFrame() override {
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Hit) continue;
            play(e.crit ? critTone_.i : tone_.i, e.crit ? 1.25f : 1.f);
        }
    }

    void onDisable() override {
        sounds::post([] { PlaySoundW(nullptr, nullptr, 0); });
    }

    void drawSettings() override {
        ImGui::Spacing();
        if (ImGui::SmallButton(i18n::tr("Play sound"))) play(tone_.i, 1.f);
        if (!need::have("MoveState")) return;
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Play crit sound"))) play(critTone_.i, 1.25f);
    }

private:
    void play(int tone, float pitchBoost) {
        static const float base[] = {880.f, 440.f, 1320.f, 660.f, 220.f};
        float hz = base[std::clamp(tone, 0, 4)] * pitch_.f * pitchBoost;
        int which = pitchBoost == 1.f ? 0 : 1;
        float volume = volume_.f;
        // the clip is built and started on the sound thread; the frame only hands over three numbers
        sounds::post([which, hz, volume, tone] { start(which, hz, volume, tone); });
    }

    struct Clip {
        std::vector<uint8_t> data;
        float hz = 0.f, volume = 0.f;
        int tone = -1;
    };

    static void start(int which, float hz, float volume, int tone) {
        static std::array<Clip, 2> clips;
        auto& clip = clips[size_t(which)];
        if (clip.data.empty() || clip.hz != hz || clip.volume != volume || clip.tone != tone) {
            // SND_MEMORY keeps reading the buffer after PlaySound returns.
            PlaySoundW(nullptr, nullptr, 0);
            clip.hz = hz;
            clip.volume = volume;
            clip.tone = tone;
            auto& wav_ = clip.data;
            int rate = 22050, n = int(rate * 0.12f);
            wav_.assign(44 + n * 2, 0);
            auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) wav_[at + i] = uint8_t(v >> (8 * i)); };
            auto put16 = [&](size_t at, uint16_t v) { for (int i = 0; i < 2; i++) wav_[at + i] = uint8_t(v >> (8 * i)); };
            std::memcpy(&wav_[0], "RIFF", 4);
            put32(4, 36 + n * 2);
            std::memcpy(&wav_[8], "WAVEfmt ", 8);
            put32(16, 16);
            put16(20, 1);
            put16(22, 1);
            put32(24, rate);
            put32(28, rate * 2);
            put16(32, 2);
            put16(34, 16);
            std::memcpy(&wav_[36], "data", 4);
            put32(40, n * 2);
            for (int i = 0; i < n; i++) {
                float t = float(i) / rate, env = std::exp(-t * 38.f);
                float wave = tone == 1 ? (std::fmod(t * hz, 1.f) < 0.5f ? 1.f : -1.f) * 0.5f : std::sin(6.2832f * hz * t);
                put16(44 + size_t(i) * 2, uint16_t(int16_t(wave * env * volume * 24000.f)));
            }
        }
        PlaySoundW(reinterpret_cast<LPCWSTR>(clip.data.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }

    Setting& tone_ = choice("tone", "Sound", {"Ping", "Click", "High", "Medium", "Low"});
    Setting& critTone_ = choice("critTone", "Sound on crit", {"Ping", "Click", "High", "Medium", "Low"}, 2);
    Setting& volume_ = slider("volume", "Volume", 0.5f, 0.05f, 1.f, "%.2f");
    Setting& pitch_ = slider("pitch", "Pitch", 1.f, 0.5f, 2.f, "%.2fx");
};

class TotemPop : public Module {
public:
    TotemPop()
        : Module("Totem Pop", "Shows a message and plays a sound when one of your totems is used up.", Category::Pvp, {"hud-self"}) {
        sub("Hit visuals");
        require(need::combat, need::sigs({"LocalPlayer", "TotemEvents"}));
    }

    void onFrame() override {
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::TotemPop) continue;
            at_ = ui::time();
            if (sound_.b) sounds::beep(MB_ICONEXCLAMATION);
        }
    }

    void onRender(ImDrawList* dl) override {
        double age = ui::time() - at_;
        if (age > duration_.f) return;
        auto ds = ImGui::GetIO().DisplaySize;
        float k = float(age / duration_.f);
        ImVec4 c = color_.color;
        c.w *= 1.f - k * k * k;
        int left = game::state().player.offhand.name == "totem_of_undying" ? game::state().player.offhand.count : 0;
        std::string s = count_.b ? i18n::fmt("Totem used ({} left)", left) : i18n::tr("Totem used");
        draw::textCentered(dl, fonts::bold(), size_.f * ui::scale(), {ds.x * 0.5f, ds.y * 0.28f - k * 20.f}, ImGui::GetColorU32(c), s.c_str());
        if (flash_.b) dl->AddRectFilled({0, 0}, ds, ImGui::GetColorU32(withAlpha(c, 0.12f * (1.f - k))));
    }

private:
    Setting& duration_ = slider("duration", "Duration (s)", 1.8f, 0.5f, 5.f, "%.1f s");
    Setting& size_ = slider("size", "Size", 28.f, 14.f, 60.f, "%.0f");
    Setting& count_ = toggleSetting("count", "Remaining totems", true);
    Setting& flash_ = toggleSetting("flash", "Flash the screen", true);
    Setting& sound_ = toggleSetting("sound", "Sound", false);
    Setting& color_ = colorSetting("color", "Color", {1.f, 0.85f, 0.4f, 1.f});
    double at_ = -10.0;
};
