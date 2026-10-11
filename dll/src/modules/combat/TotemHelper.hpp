#pragma once

#include "gui/Gui.hpp"
#include "modules/Module.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Sounds.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <string>

// Helps you notice an empty offhand and find your totems. It only reads what the client already reads every frame
// and draws; it never moves an item or clicks for you.
class TotemHelper : public Module {
public:
    TotemHelper()
        : Module("Totem Helper",
                 "Turns the totem animation off if you like, warns you the moment your offhand has no totem, with text, sound, flash or a screen border, and makes the totems in your hotbar easy to spot. Display only: it never moves items or clicks for you.",
                 Category::Pvp, {"hud-self"}) {
        sub("Hit visuals");
        require(need::player | need::inventory | need::combat, need::sigs({"LocalPlayer"}));
        for (Setting* s : {&fightOnly_, &needTotems_, &text_, &sound_, &flash_, &border_}) s->visible = [this] { return warn_.b; };
        fightWindow_.visible = [this] { return warn_.b && fightOnly_.b; };
        for (Setting* s : {&message_, &count_, &textColor_, &textSize_, &textY_, &pulse_}) s->visible = [this] { return warn_.b && text_.b; };
        for (Setting* s : {&soundKind_, &repeat_}) s->visible = [this] { return warn_.b && sound_.b; };
        flashColor_.visible = [this] { return warn_.b && flash_.b; };
        for (Setting* s : {&borderColor_, &borderSize_}) s->visible = [this] { return warn_.b && border_.b; };
        for (Setting* s : {&style_, &markColor_, &glowSize_, &markPulse_}) s->visible = [this] { return mark_.b || markInventory_.b; };
    }

    void onFrame() override {
        if (!animation_.b) game::clearTotemAnimation();
        bool now = warning();
        if (now && !warned_) {
            since_ = ui::time();
            lastSound_ = -100.0;
        }
        warned_ = now;
        if (!now || !sound_.b) return;
        double t = ui::time();
        bool first = lastSound_ < since_;
        if (!first && (repeat_.f <= 0.f || t - lastSound_ < repeat_.f)) return;
        lastSound_ = t;
        static const UINT kinds[] = {MB_OK, MB_ICONEXCLAMATION, MB_ICONHAND, MB_ICONASTERISK};
        sounds::beep(kinds[std::clamp(soundKind_.i, 0, 3)]);
    }

    void onRender(ImDrawList* dl) override {
        if (gui::editingHud()) return;
        float grids[4][4];
        bool inventory = flarialModules::slotGrids(&grids[0][0]);
        if (mark_.b && !inventory) markHotbar(dl);
        if (markInventory_.b && inventory) markInventory(dl, grids);
        if (!warned_) return;
        auto ds = ImGui::GetIO().DisplaySize;
        float age = float(ui::time() - since_);
        float wave = 0.5f + 0.5f * std::sin(age * 7.f);
        if (flash_.b) {
            float k = std::max(0.f, 1.f - age * 2.5f);
            if (k > 0.f) dl->AddRectFilled({0, 0}, ds, ImGui::GetColorU32(withAlpha(flashColor_.color, flashColor_.color.w * 0.35f * k)));
        }
        if (border_.b) {
            float w = borderSize_.f * ui::scale();
            ImU32 c = ImGui::GetColorU32(withAlpha(borderColor_.color, borderColor_.color.w * (0.55f + 0.45f * wave)));
            dl->AddRect({w * 0.5f, w * 0.5f}, {ds.x - w * 0.5f, ds.y - w * 0.5f}, c, 0.f, 0, w);
        }
        if (text_.b) {
            // the default message is stored in English and shown in the client's language until it is changed
            std::string s = message_.text == defaultMessage ? std::string(i18n::tr(defaultMessage)) : message_.text;
            if (count_.b) s += " (" + std::to_string(totems()) + ")";
            ImVec4 c = textColor_.color;
            if (pulse_.b) c.w *= 0.6f + 0.4f * wave;
            draw::textCentered(dl, fonts::bold(), textSize_.f * ui::scale(), {ds.x * 0.5f, ds.y * textY_.f}, ImGui::GetColorU32(c), s.c_str());
        }
    }

private:
    static bool isTotem(const game::Item& it) { return !it.empty() && it.name == "totem_of_undying"; }

    static int totems() {
        auto& p = game::state().player;
        int n = 0;
        for (auto& it : p.hotbar)
            if (isTotem(it)) n += it.count;
        for (auto& it : p.main)
            if (isTotem(it)) n += it.count;
        return n;
    }

    bool warning() const {
        auto& st = game::state();
        if (!warn_.b || !st.inWorld || st.player.mode == game::Mode::Creative || st.player.mode == game::Mode::Spectator) return false;
        if (isTotem(st.player.offhand)) return false;
        if (needTotems_.b && totems() == 0) return false;
        if (fightOnly_.b) {
            double last = std::max(st.combat.lastHitAt, st.combat.lastHurtAt);
            if (st.time - last > fightWindow_.f) return false;
        }
        return true;
    }

    void markHotbar(ImDrawList* dl) {
        auto& p = game::state().player;
        float rect[4];
        if (!flarialModules::hotbar(rect)) return;
        float k = rect[2] / 24.f;
        // The rectangle is the selection frame, and while scrolling it and the selected slot can be a frame apart.
        // The hotbar itself does not move, so its left edge is taken over only once both have held still.
        int slot = std::clamp(p.slot, 0, 8);
        float edge = rect[0] - float(slot) * 20.f * k;
        bool still = slot == lastSlot_ && rect[0] == lastRectX_;
        lastSlot_ = slot;
        lastRectX_ = rect[0];
        stillFrames_ = still ? stillFrames_ + 1 : 0;
        if (stillFrames_ >= 2 || edge_ == 0.f || std::fabs(k - edgeScale_) > 0.01f) {
            if (stillFrames_ >= 2 || edge_ == 0.f) edge_ = edge;
            edgeScale_ = k;
        }
        float wave = markPulse_.b ? 0.55f + 0.45f * std::sin(float(ui::time()) * 6.f) : 1.f;
        ImVec4 c = withAlpha(markColor_.color, markColor_.color.w * wave);
        for (int i = 0; i < 9; i++) {
            if (!isTotem(p.hotbar[size_t(i)])) continue;
            // the selection frame is 24 GUI pixels wide and overhangs its 20 pixel slot by 2 on each side
            float x = edge_ + float(i) * 20.f * k + 2.f * k;
            drawMark(dl, {x + 1.f * k, rect[1] + 3.f * k}, {x + 19.f * k, rect[1] + 21.f * k}, k, c);
        }
    }

    // grids from the open screen's UI tree: 18 GUI units per slot, the item fills the inner 16
    void markInventory(ImDrawList* dl, const float (&g)[4][4]) {
        auto& p = game::state().player;
        float wave = markPulse_.b ? 0.55f + 0.45f * std::sin(float(ui::time()) * 6.f) : 1.f;
        ImVec4 c = withAlpha(markColor_.color, markColor_.color.w * wave);
        auto cell = [&](const float* r, int cols, int col, int row) {
            float k = r[2] / (18.f * float(cols));
            ImVec2 a{r[0] + (float(col) * 18.f + 1.f) * k, r[1] + (float(row) * 18.f + 1.f) * k};
            drawMark(dl, a, {a.x + 16.f * k, a.y + 16.f * k}, k, c);
        };
        if (g[0][2] > 0.f)
            for (int i = 0; i < 9; i++)
                if (isTotem(p.hotbar[size_t(i)])) cell(g[0], 9, i, 0);
        if (g[1][2] > 0.f)
            for (size_t i = 0; i < p.main.size() && i < 27; i++)
                if (isTotem(p.main[i])) cell(g[1], 9, int(i % 9), int(i / 9));
        if (g[2][2] > 0.f && isTotem(p.offhand)) cell(g[2], 1, 0, 0);
    }

    void drawMark(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, ImVec4 c) {
        ImU32 col = ImGui::GetColorU32(c);
        switch (style_.i) {
        case 0: draw::glow(dl, a, b, 2.f * k, col, glowSize_.f * k); break;
        case 1: dl->AddRect(a, b, col, 1.f * k, 0, std::max(1.f, k)); break;
        case 2: dl->AddRectFilled(a, b, ImGui::GetColorU32(withAlpha(c, c.w * 0.45f)), 1.f * k); break;
        default:
            draw::glow(dl, a, b, 2.f * k, col, glowSize_.f * k);
            dl->AddRect(a, b, col, 1.f * k, 0, std::max(1.f, k));
            break;
        }
    }

    Setting& animation_ = toggleSetting("animation", "Totem animation", false);
    Setting& warn_ = toggleSetting("warn", "Warning when the offhand has no totem", true);
    Setting& fightOnly_ = toggleSetting("fightOnly", "Only during a fight", false);
    Setting& fightWindow_ = slider("fightWindow", "Fight lasts after the last hit (s)", 8.f, 2.f, 30.f, "%.0f s");
    Setting& needTotems_ = toggleSetting("needTotems", "Only when you still have totems", true);
    Setting& text_ = toggleSetting("text", "Text", true);
    static constexpr const char* defaultMessage = "No totem in offhand!";
    Setting& message_ = textSetting("message", "Message", defaultMessage);
    Setting& count_ = toggleSetting("count", "Show totems left", true);
    Setting& textColor_ = colorSetting("textColor", "Text color", {1.f, 0.32f, 0.32f, 1.f});
    Setting& textSize_ = slider("textSize", "Text size", 26.f, 12.f, 60.f, "%.0f");
    Setting& textY_ = slider("textY", "Text height on screen", 0.32f, 0.05f, 0.95f, "%.2f");
    Setting& pulse_ = toggleSetting("pulse", "Pulse", true);
    Setting& sound_ = toggleSetting("sound", "Sound", true);
    Setting& soundKind_ = choice("soundKind", "Sound", {"Beep", "Alert", "Error", "Ding"}, 1);
    Setting& repeat_ = slider("repeat", "Repeat every (s, 0 = once)", 0.f, 0.f, 5.f, "%.1f");
    Setting& flash_ = toggleSetting("flash", "Flash the screen", false);
    Setting& flashColor_ = colorSetting("flashColor", "Flash color", {1.f, 0.2f, 0.2f, 1.f});
    Setting& border_ = toggleSetting("border", "Screen border", false);
    Setting& borderColor_ = colorSetting("borderColor", "Border color", {1.f, 0.25f, 0.25f, 0.85f});
    Setting& borderSize_ = slider("borderSize", "Border thickness", 6.f, 2.f, 20.f, "%.0f");
    Setting& mark_ = toggleSetting("mark", "Highlight totems in the hotbar", true);
    Setting& markInventory_ = toggleSetting("markInventory", "Highlight totems in the inventory", true);
    Setting& style_ = choice("style", "Highlight style", {"Glow", "Frame", "Slot background", "Glow and frame"});
    Setting& markColor_ = colorSetting("markColor", "Highlight color", {1.f, 0.82f, 0.25f, 0.9f});
    Setting& glowSize_ = slider("glowSize", "Glow size", 1.f, 1.f, 12.f, "%.0f");
    Setting& markPulse_ = toggleSetting("markPulse", "Pulse", false);
    float edge_ = 0.f;
    float edgeScale_ = 0.f;
    float lastRectX_ = 0.f;
    int lastSlot_ = -1;
    int stillFrames_ = 0;
    bool warned_ = false;
    double since_ = 0.0;
    double lastSound_ = -100.0;
};
