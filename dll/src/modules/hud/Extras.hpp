#pragma once

#include "modules/common/ItemIcons.hpp"

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/HudModule.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Inventory.hpp"
#include "modules/common/Layout.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>
#include <format>

class FallPredictor : public GameText {
public:
    FallPredictor()
        : GameText("Fall Predictor", "Shows how far you have fallen and how much damage you would take if you landed now.", need::player, need::sigs({"LocalPlayer", "MoveState", "PlayerStats"}),
                   {"hud-self"}, {0.005f, 0.402f}) {
        sub("Info displays");
        warn_.visible = [this] { return colored_.b; };
    }

    void onFrame() override {
        auto& p = game::state().player;
        if (p.onGround || p.inWater || p.gliding || p.flying || p.mode == game::Mode::Creative) {
            top_ = p.pos.y;
            fall_ = 0.f;
            return;
        }
        top_ = std::max(top_, p.pos.y);
        fall_ = std::max(0.f, top_ - p.pos.y);
    }

    void onRender(ImDrawList* dl) override {
        if (fall_ < minimum_.f && !gui::editingHud()) return;
        GameText::onRender(dl);
    }

protected:
    std::string label() const override { return showLabel_.b ? i18n::tr("Fall") : ""; }

    std::string value() override {
        auto& p = game::state().player;
        float damage = std::max(0.f, std::ceil(fall_ - 3.f));
        float life = p.health + p.absorption;
        lethal_ = damage >= life && damage > 0.f;
        std::string out = i18n::fmt("{:.1f} blocks", fall_);
        if (damage > 0.f) out += i18n::fmt("  ·  {:.0f} damage", damage);
        if (lethal_ && warnText_.b) out += std::string("  ·  ") + i18n::tr("lethal");
        return out;
    }

    ImU32 valueColor() const override {
        if (!colored_.b) return textColor();
        float damage = std::max(0.f, std::ceil(fall_ - 3.f));
        if (lethal_) return ImGui::GetColorU32(lethalColor_.color);
        return ImGui::GetColorU32(damage > 0.f ? warn_.color : textColor_.color);
    }

private:
    Setting& minimum_ = slider("minimum", "Show from (blocks)", 2.f, 0.f, 10.f, "%.1f");
    Setting& colored_ = toggleSetting("colored", "Color by danger", true);
    Setting& warnText_ = toggleSetting("warnText", "Say when it would be lethal", true);
    Setting& warn_ = colorSetting("warn", "Color when it hurts", {1.f, 0.82f, 0.49f, 1.f});
    Setting& lethalColor_ = colorSetting("lethal", "Color when lethal", {1.f, 0.3f, 0.35f, 1.f});
    float top_ = 0.f;
    float fall_ = 0.f;
    mutable bool lethal_ = false;
};

class InventoryView : public GameList {
public:
    InventoryView()
        : GameList("Inventory Viewer", "Shows the contents of your inventory as a grid on the screen, with counts and durability.", need::inventory,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.4f, 0.3f}) {
        sub("Inventory info");
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& p = game::state().player;
        float cell = cell_.f * s, gap = gap_.f * s, y = 0.f, round = slotRound_.f * s;
        float w = 9.f * cell + 8.f * gap;
        ImVec4 slot = slotColor_.color;
        if (title_.b) y += drawText(dl, o, s, titleText_.text.empty() ? std::string(i18n::tr("Inventory")) : titleText_.text, accentColor()).y + 3 * s;
        auto row = [&](const game::Item* items, int count, bool hotbar) {
            for (int i = 0; i < count; i++) {
                ImVec2 a = o + ImVec2(float(i) * (cell + gap), y);
                ImVec2 b = a + ImVec2(cell, cell);
                const game::Item& it = items[i];
                bool selected = hotbar && highlightHeld_.b && i == p.slot;
                if (!it.empty() || emptySlots_.b) dl->AddRectFilled(a, b, ImGui::GetColorU32(withAlpha(slot, slot.w * (it.empty() ? 0.64f : 1.f))), round);
                if (selected) dl->AddRect(a, b, ImGui::GetColorU32(theme::current().accent), round, 0, 1.5f * s);
                if (it.empty()) continue;
                itemicon::draw(dl, a + ImVec2(2 * s, 2 * s), cell - 4 * s, it.name);
                if (it.enchanted && glint_.b) dl->AddRect(a, b, ImGui::GetColorU32(withAlpha(theme::current().accent, 0.8f)), round, 0, 1.2f * s);
                if (it.count > 1 && counts_.b) {
                    std::string t = std::to_string(it.count);
                    ImVec2 ts = textSize(s * countSize_.f, t);
                    drawText(dl, b - ts - ImVec2(2 * s, 1 * s), s * countSize_.f, t, textColor());
                }
                if (it.maxDamage > 0 && durability_.b && it.damage > 0) {
                    float f = it.fraction();
                    ImVec2 b0{a.x + 2 * s, b.y - 4 * s};
                    dl->AddRectFilled(b0, b0 + ImVec2(cell - 4 * s, 2 * s), IM_COL32(0, 0, 0, 140));
                    dl->AddRectFilled(b0, b0 + ImVec2((cell - 4 * s) * f, 2 * s), ImGui::GetColorU32(rampColor(1.f - f, 0.f, 1.f, ok_.color, mid_.color, bad_.color)));
                }
            }
            y += cell + gap;
        };
        if (main_.b) {
            int rows = std::min(3, int(p.main.size() + 8) / 9);
            for (int r = 0; r < rows; r++) row(p.main.data() + r * 9, std::min(9, int(p.main.size()) - r * 9), false);
        }
        if (hotbar_.b) {
            if (main_.b) y += 3 * s;
            row(p.hotbar.data(), 9, true);
        }
        return {w, std::max(0.f, y - gap)};
    }

private:
    Setting& title_ = toggleSetting("title", "Title", false);
    Setting& cell_ = slider("cell", "Slot size", 26.f, 16.f, 48.f, "%.0f");
    Setting& main_ = toggleSetting("main", "Inventory rows", true);
    Setting& hotbar_ = toggleSetting("hotbar", "Hotbar row", true);
    Setting& highlightHeld_ = toggleSetting("held", "Mark the selected slot", true);
    Setting& counts_ = toggleSetting("counts", "Item counts", true);
    Setting& durability_ = toggleSetting("durability", "Durability bars", true);
    Setting& glint_ = toggleSetting("glint", "Outline enchanted items", true);
    Setting& titleText_ = textSetting("titleText", "Custom title (empty uses default)", "");
    Setting& gap_ = slider("gap", "Gap between slots", 2.f, 0.f, 10.f, "%.0f");
    Setting& slotRound_ = slider("slotRound", "Slot corner radius", 3.f, 0.f, 12.f, "%.0f");
    Setting& slotColor_ = colorSetting("slotColor", "Slot color", {0.f, 0.f, 0.f, 0.43f});
    Setting& emptySlots_ = toggleSetting("emptySlots", "Draw empty slots", true);
    Setting& countSize_ = slider("countSize", "Size of the item counts", 0.8f, 0.5f, 1.3f, "%.2fx");
    Setting& ok_ = colorSetting("ok", "Color full", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Color half", {1.f, 0.82f, 0.49f, 1.f});
    Setting& bad_ = colorSetting("bad", "Color almost broken", {1.f, 0.4f, 0.45f, 1.f});
};
