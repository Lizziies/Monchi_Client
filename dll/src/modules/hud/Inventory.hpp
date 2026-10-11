#pragma once

#include "gui/Gui.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/common/ArmorIcons.hpp"
#include "modules/common/Sounds.hpp"
#include "modules/common/ItemIcons.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/VanillaHud.hpp"
#include "modules/common/Inventory.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <array>
#include <map>

class ArmorHud : public GameList {
public:
    ArmorHud()
        : GameList("Armor HUD", "Shows your armor and the item in your hand with durability as a number, percent or bar.", need::inventory,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.135f, 0.7f}) {
        sub("Inventory info");
        flash_.visible = [this] { return warn_.f > 0.f; };
    }

    void load(const nlohmann::json& j) override {
        Module::load(j);
        if (!j.contains("armorIconsVersion")) {
            held_.b = true;
            hideEmpty_.b = true;
        }
    }

    nlohmann::json save() const override {
        auto j = Module::save();
        j["armorIconsVersion"] = 1;
        return j;
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& p = game::state().player;
        std::array<const game::Item*, 6> items{};
        int itemCount = 4;
        for (int i = 0; i < 4; i++) items[i] = &p.armor[size_t(i)];
        int armorSlot = 0;
        if (held_.b) items[itemCount++] = &p.held();
        if (offhand_.b) items[itemCount++] = &p.offhand;

        bool horizontal = layout_.i == 1;
        float size = iconSize_.f * s, gap = slotGap_.f * s, x = 0.f, y = 0.f, w = 0.f, h = 0.f;
        double now = ui::time();
        for (int index = 0; index < itemCount; ++index) {
            auto* it = items[index];
            int slot = armorSlot++;
            if (it->empty() && hideEmpty_.b) continue;
            ImVec2 a = o + ImVec2(x, y);
            if (iconBackground_.b) dl->AddRectFilled(a, a + ImVec2(size, size), ImGui::GetColorU32(iconBgColor_.color), iconRound_.f * s);
            ImVec2 pad{size * 0.08f, size * 0.08f};
            static const char* emptyIcons[] = {"empty_armor_slot_helmet", "empty_armor_slot_chestplate", "empty_armor_slot_leggings", "empty_armor_slot_boots"};
            std::string icon = it->empty() && slot < 4 ? emptyIcons[slot] : it->name;
            if (!itemicon::draw(dl, a + pad, size * 0.84f, icon) && slot < 4)
                armoricon::draw(dl, a + pad, size * 0.84f, slot, it->empty() ? IM_COL32(150, 150, 160, 80) : materialColor(it->name));
            if (it->enchanted && glint_.b) dl->AddRect(a, a + ImVec2(size, size), ImGui::GetColorU32(theme::current().accent), iconRound_.f * s, 0, 1.5f * s);

            float textGap = textGap_.f * s;
            float tx = a.x + size + textGap, rowW = size;
            if (it->maxDamage > 0 && durability_.b) {
                float f = it->fraction();
                bool low = warn_.f > 0.f && f * 100.f < warn_.f;
                ImVec4 c = rampColor(1.f - f, 0.f, 1.f, ok_.color, mid_.color, bad_.color);
                if (low && flash_.b && std::fmod(now, 0.8) < 0.4) c = bad_.color;
                std::string t;
                if (mode_.i == 0 || mode_.i == 3) t = std::to_string(it->left());
                if (mode_.i == 1 || mode_.i == 3) t += std::format("{}{:.0f}%", t.empty() ? "" : valueSep_.text, f * 100.f);
                float tw = 0.f;
                if (!t.empty()) tw = drawText(dl, {tx, a.y + (size - fonts::hudSize() * s) * 0.5f - (mode_.i == 2 ? 4 * s : 0)}, s, t, ImGui::GetColorU32(c)).x;
                if (mode_.i == 2 || bar_.b) {
                    float bw = std::max(barWidth_.f * s, tw), bh = barHeight_.f * s;
                    ImVec2 b0{tx, a.y + size - bh - 2 * s};
                    dl->AddRectFilled(b0, b0 + ImVec2(bw, bh), IM_COL32(0, 0, 0, 90), bh * 0.5f);
                    dl->AddRectFilled(b0, b0 + ImVec2(bw * f, bh), ImGui::GetColorU32(c), bh * 0.5f);
                    tw = std::max(tw, bw);
                }
                rowW = size + textGap + tw;
            } else if (counts_.b && !it->empty() && it->maxDamage <= 0 && it->count > 1) {
                rowW = size + textGap + drawText(dl, {tx, a.y + (size - fonts::hudSize() * s) * 0.5f}, s, std::format("{}{}", countText_.text, it->count), textColor()).x;
            }
            if (horizontal) {
                x += rowW + gap * 2;
                w = x;
                h = size;
            } else {
                y += size + gap;
                w = std::max(w, rowW);
                h = y;
            }
        }
        return {std::max(w, size), std::max(h, size)};
    }

private:
    Setting& slotGap_ = slider("slotGap", "Slot spacing", 4.f, 0.f, 24.f, "%.0f");
    Setting& textGap_ = slider("textGap", "Text spacing", 6.f, 0.f, 24.f, "%.0f");
    Setting& durability_ = toggleSetting("showDurability", "Show durability", true);
    Setting& counts_ = toggleSetting("showCount", "Show item count", true);
    Setting& iconBackground_ = toggleSetting("iconBackground", "Icon background", true);
    Setting& iconBgColor_ = colorSetting("iconBgColor", "Icon background color", {0.f, 0.f, 0.f, 100.f / 255.f});
    Setting& iconRound_ = slider("iconRound", "Icon corner radius", 5.f, 0.f, 16.f, "%.0f");
    Setting& barWidth_ = slider("barWidth", "Bar width", 48.f, 16.f, 160.f, "%.0f");
    Setting& barHeight_ = slider("barHeight", "Bar height", 4.f, 2.f, 12.f, "%.0f");
    Setting& valueSep_ = textSetting("valueSep", "Separator", "  ");
    Setting& countText_ = textSetting("countText", "Text in front of the amount", "x");
    Setting& layout_ = choice("layout", "Layout", {"Stacked", "Side by side"});
    Setting& mode_ = choice("mode", "Durability", {"Number", "Percent", "Bar", "Number and percent"}, 1);
    Setting& bar_ = toggleSetting("bar", "Also show a bar", false);
    Setting& iconSize_ = slider("icon", "Icon size", 20.f, 12.f, 40.f, "%.0f");
    Setting& held_ = toggleSetting("held", "Item in hand", true);
    Setting& offhand_ = toggleSetting("offhand", "Offhand", false);
    Setting& hideEmpty_ = toggleSetting("hideEmpty", "Hide empty slots", true);
    Setting& glint_ = toggleSetting("glint", "Outline enchanted", true);
    Setting& warn_ = slider("warn", "Warn below (%)", 15.f, 0.f, 50.f, "%.0f%%");
    Setting& flash_ = toggleSetting("flash", "Flash on warning", true);
    Setting& ok_ = colorSetting("ok", "Color full", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Color half", {1.f, 0.82f, 0.49f, 1.f});
    Setting& bad_ = colorSetting("bad", "Color almost broken", {1.f, 0.4f, 0.45f, 1.f});
};

class PotionHud : public GameList {
public:
    PotionHud()
        : GameList("Potion HUD", "Shows your active effects with time left, sorted and colored. Turns red when an effect is about to run out.", need::effects,
                   need::sigs({"LocalPlayer", "Effects"}), {"hud-self"}, {0.845f, 0.045f}) {
        sub("Inventory info");
        flash_.visible = [this] { return lowAt_.f > 0.f; };
        low_.visible = [this] { return lowAt_.f > 0.f; };
        roman_.visible = [this] { return showName_.b; };
        romanAll_.visible = [this] { return showName_.b && roman_.b; };
        twoLines_.visible = [this] { return showName_.b && showTime_.b; };
        timeSize_.visible = [this] { return showTime_.b; };
    }

protected:
    ImVec2 pivot() const override { return {0.f, bottomUp_.b ? 1.f : 0.f}; }

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        std::vector<game::Effect> list = game::state().player.effects;
        std::erase_if(list, [&](const game::Effect& e) { return (e.good && !showGood_.b) || (!e.good && !showBad_.b); });
        auto key = [&](const game::Effect& e) {
            float seconds = e.infinite ? std::numeric_limits<float>::infinity() : e.seconds;
            return sort_.i == 0 ? -seconds : sort_.i == 1 ? seconds : 0.f;
        };
        std::sort(list.begin(), list.end(), [&](auto& a, auto& b) { return sort_.i == 2 ? a.id < b.id : key(a) < key(b); });
        if ((int)list.size() > max_.i) list.resize(size_t(max_.i));
        if (list.empty()) return emptyNote_.b || gui::editingHud() ? drawText(dl, o, s, i18n::tr("No effects"), ImGui::GetColorU32(theme::current().textDim)) : ImVec2{};
        if (bottomUp_.b) std::reverse(list.begin(), list.end());

        struct Line {
            std::string name, time;
            ImU32 color;
            ImU32 swatch;
            float fraction;
        };
        std::vector<Line> lines;
        double now = ui::time();
        float ts = s * timeSize_.f;
        bool stacked = twoLines_.b && showName_.b && showTime_.b;
        float sw = swatchSize_.f * s;
        float w = rowWidth_.f * s, indent = swatch_.b ? sw + 4.f * s : 0.f, gap = timeGap_.f * s;
        for (auto& e : list) {
            Line l;
            if (showName_.b) l.name = text::effect(e.id);
            if (e.amplifier > 0) {
                int lv = e.amplifier + 1;
                std::string level = roman_.b && showName_.b && (romanAll_.b || lv <= 5) ? text::roman(lv) : std::to_string(lv);
                l.name += (l.name.empty() ? "" : " ") + level;
            }
            if (showTime_.b) l.time = e.infinite ? i18n::tr("Infinite") : text::clock(e.seconds);
            l.swatch = IM_COL32((e.color >> 16) & 255, (e.color >> 8) & 255, e.color & 255, 255);
            l.color = colored_.b ? l.swatch : textColor();
            if (!e.infinite && lowAt_.f > 0.f && e.seconds <= lowAt_.f && !(flash_.b && std::fmod(now, 0.6) < 0.3)) l.color = ImGui::GetColorU32(low_.color);
            l.fraction = !e.infinite && e.total > 0.f ? std::clamp(e.seconds / e.total, 0.f, 1.f) : -1.f;
            float nameW = textSize(s, l.name).x, timeW = textSize(ts, l.time).x;
            float rowW = indent + (stacked ? std::max(nameW, timeW) : nameW + (l.name.empty() || l.time.empty() ? 0.f : gap) + timeW);
            w = std::max(w, rowW);
            lines.push_back(std::move(l));
        }

        auto at = [&](float x, float width) { return mirror_.b ? w - x - width : x; };
        float y = 0.f, lineH = textSize(s, "Ag").y, timeH = textSize(ts, "Ag").y;
        for (auto& l : lines) {
            float nameW = textSize(s, l.name).x, timeW = textSize(ts, l.time).x;
            if (swatch_.b) {
                float sx = at(0.f, sw);
                dl->AddRectFilled(o + ImVec2(sx, y + 2 * s), o + ImVec2(sx + sw, y + 2 * s + sw), l.swatch, swatchRound_.b ? sw * 0.5f : 2 * s);
            }
            if (!l.name.empty()) drawText(dl, o + ImVec2(at(indent, nameW), y), s, l.name, l.color);
            float rowH = l.name.empty() ? timeH : lineH;
            if (!l.time.empty() && stacked) {
                drawText(dl, o + ImVec2(at(indent, timeW), y + lineH), ts, l.time, l.color);
                rowH = lineH + timeH;
            } else if (!l.time.empty()) {
                float tx = l.name.empty() ? indent : w - timeW;
                drawText(dl, o + ImVec2(at(tx, timeW), y + (l.name.empty() ? 0.f : (lineH - timeH) * 0.5f)), ts, l.time, l.color);
                rowH = std::max(rowH, timeH);
            }
            y += rowH;
            if (bar_.b && l.fraction >= 0.f) {
                float bw = w - indent, bx = mirror_.b ? 0.f : indent;
                ImVec2 b0 = o + ImVec2(bx, y);
                float bh = barHeight_.f * s;
                dl->AddRectFilled(b0, b0 + ImVec2(bw, bh), IM_COL32(0, 0, 0, 80), bh * 0.5f);
                float from = mirror_.b ? bw * (1.f - l.fraction) : 0.f;
                dl->AddRectFilled(b0 + ImVec2(from, 0.f), b0 + ImVec2(from + bw * l.fraction, bh), l.color, bh * 0.5f);
                y += bh + 2 * s;
            }
            y += spacing_.f * s;
        }
        return {w, std::max(0.f, y - spacing_.f * s)};
    }

private:
    Setting& sort_ = choice("sort", "Sorting", {"Longest first", "Shortest first", "By name"});
    Setting& max_ = intSlider("max", "Show at most", 8, 1, 16);
    Setting& bottomUp_ = toggleSetting("bottomUp", "Bottom up", false);
    Setting& spacing_ = slider("spacing", "Spacing", 0.f, 0.f, 12.f, "%.0f");
    Setting& showName_ = toggleSetting("name", "Effect name", true);
    Setting& roman_ = toggleSetting("roman", "Roman numerals", true);
    Setting& romanAll_ = toggleSetting("romanAll", "Roman numerals above V", true);
    Setting& showTime_ = toggleSetting("time", "Time left", true);
    Setting& twoLines_ = toggleSetting("twoLines", "Time under the name", false);
    Setting& timeSize_ = slider("timeSize", "Time text size", 1.f, 0.6f, 1.4f, "%.2fx");
    Setting& mirror_ = toggleSetting("mirror", "Text to the left (right-aligned)", false);
    Setting& showGood_ = toggleSetting("good", "Positive effects", true);
    Setting& showBad_ = toggleSetting("bad", "Negative effects", true);
    Setting& colored_ = toggleSetting("colored", "Color by effect", true);
    Setting& swatch_ = toggleSetting("swatch", "Color swatch", true);
    Setting& bar_ = toggleSetting("bar", "Time-left bar", true);
    Setting& lowAt_ = slider("blinkAt", "Red below (s, 0 = off)", 5.f, 0.f, 60.f, "%.0f s");
    Setting& low_ = colorSetting("blink", "Color when low", {1.f, 0.4f, 0.45f, 1.f});
    Setting& flash_ = toggleSetting("flash", "Flash when low", false);
    Setting& barHeight_ = slider("barHeight", "Bar height", 3.f, 1.f, 10.f, "%.0f");
    Setting& swatchSize_ = slider("swatchSize", "Swatch size", 8.f, 4.f, 20.f, "%.0f");
    Setting& swatchRound_ = toggleSetting("swatchRound", "Round swatch", false);
    Setting& timeGap_ = slider("timeGap", "Gap in front of the time", 14.f, 2.f, 60.f, "%.0f");
    Setting& rowWidth_ = slider("rowWidth", "Minimum width", 100.f, 0.f, 320.f, "%.0f");
    Setting& emptyNote_ = toggleSetting("emptyNote", "Show a note without effects", true);
};

class CountHud : public GameText {
public:
    CountHud(std::string name, std::string desc, std::string item, int aux, ImVec2 pos)
        : GameText(std::move(name), std::move(desc), need::inventory, need::sigs({"LocalPlayer"}), {"hud-self"}, pos), item_(std::move(item)),
          aux_(aux) {
        sub("Inventory info");
        inHand_.visible = [this] { return handRule(); };
    }

    void onRender(ImDrawList* dl) override {
        if (handRule() && inHand_.b && !gui::editingHud() && !holding()) return;
        if (hideZero_.b && !gui::editingHud() && countItems(itemName(), itemAux(), hotbarOnly_.b, countOffhand()) == 0) return;
        GameText::onRender(dl);
    }

protected:
    virtual bool handRule() const { return false; }
    virtual bool holding() const { return true; }
    virtual bool countOffhand() const { return true; }
    virtual std::string itemName() const { return item_; }
    virtual int itemAux() const { return aux_; }

    std::string label() const override { return showLabel_.b ? title() : ""; }

    std::string value() override {
        int n = countItems(itemName(), itemAux(), hotbarOnly_.b, countOffhand());
        shown_ = n;
        return std::to_string(n);
    }

    ImU32 valueColor() const override {
        if (lowAt_.i > 0 && shown_ <= lowAt_.i) return ImGui::GetColorU32(lowColor_.color);
        return textColor();
    }

    virtual std::string title() const { return text::pretty(itemName()); }

private:
    Setting& hotbarOnly_ = toggleSetting("hotbarOnly", "Count hotbar only", false);
    Setting& inHand_ = toggleSetting("inHand", "Only when in hand", false);
    Setting& hideZero_ = toggleSetting("hideZero", "Hide when you have none", false);
    Setting& lowAt_ = intSlider("lowAt", "Warning color at most (0 = off)", 0, 0, 64);
    Setting& lowColor_ = colorSetting("lowColor", "Warning color", {1.f, 0.4f, 0.45f, 1.f});
    std::string item_;
    int aux_;
    mutable int shown_ = 0;
};

class PotCounter : public CountHud {
public:
    PotCounter() : CountHud("Pot Counter", "Counts the splash potions in your inventory.", "splash_potion", -1, {0.135f, 0.338f}) {}

protected:
    bool countOffhand() const override { return false; }
    std::string itemName() const override { return name_.text; }
    int itemAux() const override { return aux_.i; }
    std::string title() const override { return "Pots"; }

private:
    Setting& name_ = textSetting("item", "Item name", "splash_potion");
    Setting& aux_ = intSlider("aux", "Potion value (-1 = any)", -1, -1, 120);
};

class ArrowCounter : public CountHud {
public:
    ArrowCounter() : CountHud("Arrow Counter", "Counts your arrows. Can show only while you hold a bow or crossbow.", "arrow", -1, {0.135f, 0.37f}) {}

protected:
    bool handRule() const override { return true; }

    bool holding() const override {
        auto& p = game::state().player;
        auto ranged = [](const game::Item& it) { return !it.empty() && (it.name == "bow" || it.name == "crossbow"); };
        return ranged(p.held()) || ranged(p.offhand);
    }

    std::string title() const override { return i18n::tr("Arrows"); }
};

class TotemCounter : public CountHud {
public:
    TotemCounter() : CountHud("Totem Counter", "Counts your totems of undying. Can show only while you hold one.", "totem_of_undying", -1, {0.135f, 0.402f}) {}

protected:
    bool handRule() const override { return true; }

    bool holding() const override {
        auto& p = game::state().player;
        return (!p.held().empty() && p.held().name == "totem_of_undying") || (!p.offhand.empty() && p.offhand.name == "totem_of_undying");
    }

    std::string title() const override { return "Totems"; }
};

class ItemCounter : public GameList {
public:
    ItemCounter()
        : GameList("Item Counter", "Counts any items you choose. Several items at once, sorting, format, hide at 0 or 1 and colored icons.", need::inventory,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.135f, 0.57f}) {
        sub("Inventory info");
        lowColor_.visible = [this] { return lowAt_.i > 0; };
        for (Setting* st : {&countPos_, &iconOnly_, &iconSize_}) st->visible = [this] { return icons_.i == 1; };
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        struct Row {
            std::string id;
            std::string name;
            int count;
            bool dim;
        };
        bool editing = gui::editingHud();
        std::vector<Row> rows;
        for (auto& entry : text::split(items_.text, ',')) {
            std::string id = entry, label;
            if (auto eq = id.find('='); eq != std::string::npos) {
                auto parts = text::split(id, '=');
                id = parts.empty() ? "" : parts[0];
                label = parts.size() > 1 ? parts[1] : "";
            }
            id = text::lower(id);
            std::replace(id.begin(), id.end(), ' ', '_');
            int aux = -1;
            if (auto colon = id.rfind(':'); colon != std::string::npos && colon + 1 < id.size() && std::isdigit((unsigned char)id[colon + 1])) {
                aux = std::atoi(id.c_str() + colon + 1);
                id = id.substr(0, colon);
            }
            if (id.rfind("minecraft:", 0) == 0) id = id.substr(10);
            if (id.empty()) continue;
            int n = countItems(id, aux, hotbarOnly_.b);
            if (n == 0 && hideZero_.b && !editing) continue;
            rows.push_back({id, label.empty() ? text::pretty(id) : label, n, n == 0 && (dimZero_.b || hideZero_.b)});
        }
        if (sort_.i == 1) std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.count > b.count; });
        else if (sort_.i == 2) std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.name < b.name; });
        if (rows.empty()) return editing ? drawText(dl, o, s, i18n::tr("No items"), ImGui::GetColorU32(theme::current().textDim)) : ImVec2(0.f, 0.f);

        bool badge = icons_.i == 1;
        int place = badge ? countPos_.i : 0;
        float lineH = textSize(s, "Ag").y, icon = (lineH - 2 * s) * iconSize_.f, rowH = std::max(lineH, icon + 2 * s);
        float gap = spacing_.f * s, rowGap = 10 * s + gap, x = 0.f, y = 0.f, w = 0.f;
        auto faded = [](ImU32 c) { return (c & ~IM_COL32_A_MASK) | (ImU32(float((c >> IM_COL32_A_SHIFT) & 0xFF) * 0.45f) << IM_COL32_A_SHIFT); };
        for (auto& r : rows) {
            std::string value = r.count == 1 && hideOne_.b ? "" : std::to_string(r.count);
            std::string line = format_.text.empty() ? "{name} {value}" : format_.text;
            std::string name = badge && iconOnly_.b ? "" : r.name;
            for (auto& [key, val] : {std::pair<const char*, std::string>{"{name}", name}, {"{id}", r.id}, {"{value}", place == 0 ? value : std::string()}})
                for (size_t at = line.find(key); at != std::string::npos; at = line.find(key, at + val.size())) line.replace(at, std::strlen(key), val);
            while (!line.empty() && line.back() == ' ') line.pop_back();
            while (!line.empty() && line.front() == ' ') line.erase(line.begin());
            ImU32 col = lowAt_.i > 0 && r.count <= lowAt_.i ? ImGui::GetColorU32(lowColor_.color) : textColor();
            if (r.dim) col = faded(col);
            float cx = x, ty = y + (rowH - lineH) * 0.5f;
            if (place == 1 && !value.empty()) cx += drawText(dl, o + ImVec2(cx, ty), s, value, col).x + 4 * s;
            if (badge) {
                ImVec2 a = o + ImVec2(cx, y + (rowH - icon) * 0.5f), b = a + ImVec2(icon, icon);
                ImU32 fill = materialColor(r.id), edge = IM_COL32(0, 0, 0, 90);
                dl->AddRectFilled(a, b, r.dim ? faded(fill) : fill, icon * 0.22f);
                dl->AddRect(a, b, r.dim ? faded(edge) : edge, icon * 0.22f);
                if (place == 2 && !value.empty()) {
                    float cs = s * 0.75f;
                    ImVec2 vs = textSize(cs, value);
                    drawText(dl, b - vs + ImVec2(2 * s, 2 * s), cs, value, col);
                }
                cx += icon + (line.empty() ? 0.f : 4 * s);
            }
            if (!line.empty()) cx += drawText(dl, o + ImVec2(cx, ty), s, line, col).x;
            if (layout_.i == 0) {
                w = std::max(w, cx);
                y += rowH + gap;
            } else {
                x = cx + rowGap;
            }
        }
        return {layout_.i == 0 ? w : x - rowGap, layout_.i == 0 ? y - gap : rowH};
    }

private:
    Setting& items_ = textSetting("items", "Items (comma, name or name:variant, =Label renames)", "golden_apple, ender_pearl, splash_potion");
    Setting& format_ = textSetting("format", "Format ({name} {value} {id})", "{name} {value}");
    Setting& sort_ = choice("sort", "Order", {"As listed", "Most first", "By name"});
    Setting& layout_ = choice("layout", "Layout", {"Stacked", "In a row"});
    Setting& icons_ = choice("icons", "Icons", {"None", "Colored badge"}, 1);
    Setting& countPos_ = choice("countPos", "Count position", {"In the text", "Left of the icon", "On the icon"});
    Setting& iconOnly_ = toggleSetting("iconOnly", "Icon replaces the name", false);
    Setting& iconSize_ = slider("iconSize", "Icon size", 1.f, 0.6f, 2.5f, "%.2fx");
    Setting& spacing_ = slider("spacing", "Spacing", 2.f, 0.f, 20.f, "%.0f");
    Setting& hideZero_ = toggleSetting("hideZero", "Hide items you do not have", false);
    Setting& dimZero_ = toggleSetting("dimZero", "Dim items you do not have", false);
    Setting& hideOne_ = toggleSetting("hideOne", "Hide the number when it is 1", false);
    Setting& hotbarOnly_ = toggleSetting("hotbarOnly", "Count hotbar only", false);
    Setting& lowAt_ = intSlider("lowAt", "Warning color at most (0 = off)", 0, 0, 64);
    Setting& lowColor_ = colorSetting("lowColor", "Warning color", {1.f, 0.4f, 0.45f, 1.f});
};

class DurabilityWarning : public Module {
public:
    DurabilityWarning()
        : Module("Durability Warning",
                 "Warns once when armor or a tool runs low and again when it is about to break: a card with the item, its bar and the percent, "
                 "a short red flash at the screen edges and a sound that gets louder. Afterwards only a small indicator stays.",
                 Category::Hud, {"hud-self"}) {
        sub("Inventory info");
        require(need::inventory, need::sigs({"LocalPlayer"}));
        threshold_.visible = critPercent_.visible = [this] { return unit_.i == 0; };
        points_.visible = critPoints_.visible = [this] { return unit_.i == 1; };
        for (Setting* st : {&showTime_, &size_, &y_, &warnColor_, &critColor_, &indicator_}) st->visible = [this] { return card_.b; };
    }

    void onFrame() override {
        auto& p = game::state().player;
        double now = ui::time();
        const game::Item* items[6] = {&p.armor[0], &p.armor[1], &p.armor[2], &p.armor[3], &p.held(), &p.offhand};
        bool on[6] = {armor_.b, armor_.b, armor_.b, armor_.b, held_.b, offhand_.b};
        for (int i = 0; i < 6; i++) {
            Watch& w = watch_[size_t(i)];
            const game::Item& it = *items[i];
            if (!on[i] || it.empty() || it.maxDamage <= 0) {
                w = {};
                continue;
            }
            if (it.name != w.name) w = {it.name};
            w.fraction = it.fraction();
            w.left = it.left();
            Level level = levelOf(it);
            if (level > w.level) fire(w, level, now);
            w.level = level;
        }
    }

    void onRender(ImDrawList* dl) override {
        double now = ui::time();
        auto ds = ImGui::GetIO().DisplaySize;
        float k = size_.f * ui::scale();
        if (flash_.b) edgeFlash(dl, ds, now);
        if (!card_.b) return;
        float y = ds.y * y_.f;
        for (int i = 0; i < 6; i++) {
            Watch& w = watch_[size_t(i)];
            if (w.level == Level::Ok) continue;
            double age = now - w.shownAt;
            if (age < showTime_.f + 0.4) {
                y += card(dl, ds.x * 0.5f, y, k, w, i, age, now) + 6.f * k;
            } else if (indicator_.b && w.level == Level::Critical) {
                y += pill(dl, ds.x * 0.5f, y, k, w, i, now) + 4.f * k;
            }
        }
    }

private:
    enum class Level { Ok, Low, Critical };

    struct Watch {
        std::string name;
        Level level = Level::Ok;
        double shownAt = -100.0;
        float fraction = 1.f;
        int left = 0;
    };

    Level levelOf(const game::Item& it) const {
        bool critical = unit_.i == 1 ? it.left() <= critPoints_.i : it.fraction() * 100.f < critPercent_.f;
        if (critical) return Level::Critical;
        bool low = unit_.i == 1 ? it.left() <= points_.i : it.fraction() * 100.f < threshold_.f;
        return low ? Level::Low : Level::Ok;
    }

    // the same piece is only announced again after half a minute, so swapping between tools does not nag
    void fire(Watch& w, Level level, double now) {
        std::string key = w.name + (level == Level::Critical ? "!" : "");
        auto seen = lastWarn_.find(key);
        if (seen != lastWarn_.end() && now - seen->second < 30.0) return;
        lastWarn_[key] = now;
        w.shownAt = now;
        if (level == Level::Critical) flashAt_ = now;
        if (sound_.i == 2 || (sound_.i == 1 && level == Level::Critical)) sounds::beep(level == Level::Critical ? MB_ICONHAND : MB_ICONASTERISK);
        if (notify_.b)
            notify::push(i18n::tr("Durability Warning"), i18n::fmt("{} has {} durability left", text::pretty(w.name), w.left), notify::Kind::Warn, 4.f);
    }

    ImVec4 colorOf(Level level) const { return level == Level::Critical ? critColor_.color : warnColor_.color; }

    void icon(ImDrawList* dl, ImVec2 at, float size, const Watch& w, int slot, float alpha) const {
        ImU32 base = materialColor(w.name);
        if (slot < 4) armoricon::draw(dl, at, size, slot, base, alpha);
        else armoricon::drawTool(dl, at, size, armoricon::toolKind(w.name), base, alpha);
    }

    void edgeFlash(ImDrawList* dl, ImVec2 ds, double now) const {
        float age = float(now - flashAt_);
        if (age < 0.f || age > 1.4f) return;
        float a = (1.f - age / 1.4f) * (0.55f + 0.45f * std::cos(age * 11.f));
        if (a <= 0.f) return;
        ImU32 hot = theme::col(critColor_.color, a * 0.55f), none = theme::col(critColor_.color, 0.f);
        float e = std::min(ds.x, ds.y) * 0.16f;
        dl->AddRectFilledMultiColor({0, 0}, {ds.x, e}, hot, hot, none, none);
        dl->AddRectFilledMultiColor({0, ds.y - e}, {ds.x, ds.y}, none, none, hot, hot);
        dl->AddRectFilledMultiColor({0, 0}, {e, ds.y}, hot, none, none, hot);
        dl->AddRectFilledMultiColor({ds.x - e, 0}, {ds.x, ds.y}, none, hot, hot, none);
    }

    float card(ImDrawList* dl, float cx, float y, float k, const Watch& w, int slot, double age, double now) const {
        float t = std::clamp(float(age) / 0.3f, 0.f, 1.f);
        float in = 1.f - (1.f - t) * (1.f - t);
        float out = std::clamp(float(showTime_.f + 0.4 - age) / 0.4f, 0.f, 1.f);
        float alpha = in * out;
        float width = 236.f * k, height = 48.f * k;
        ImVec2 a{cx - width * 0.5f, y - (1.f - in) * 22.f * k};
        ImVec2 b = a + ImVec2(width, height);
        bool critical = w.level == Level::Critical;
        ImVec4 c = colorOf(w.level);
        float pulse = critical ? 0.65f + 0.35f * std::sin(float(now) * 7.f) : 1.f;
        dl->AddRectFilled(a, b, IM_COL32(12, 12, 16, int(205 * alpha)), 7.f * k);
        dl->AddRect(a, b, theme::col(c, 0.55f * alpha * pulse), 7.f * k, 0, (critical ? 1.8f : 1.2f) * k);
        dl->AddRectFilled(a + ImVec2(0, 6.f * k), a + ImVec2(3.f * k, height - 6.f * k), theme::col(c, alpha));
        float is = 30.f * k;
        ImVec2 ia = a + ImVec2(12.f * k, (height - is) * 0.5f);
        icon(dl, ia, is, w, slot, alpha);
        float tx = ia.x + is + 10.f * k, tr = b.x - 10.f * k;
        std::string title = text::pretty(w.name);
        std::string tag = critical ? i18n::tr("Critical") : i18n::tr("Low");
        ImFont* bold = fonts::bold();
        float ts = 14.f * k, ss = 11.f * k;
        dl->AddText(bold, ts, {tx, a.y + 6.f * k}, theme::col({1.f, 1.f, 1.f, 1.f}, alpha), title.c_str());
        ImVec2 tg = bold->CalcTextSizeA(ss, FLT_MAX, 0.f, tag.c_str());
        dl->AddText(bold, ss, {tr - tg.x, a.y + 8.f * k}, theme::col(c, alpha * pulse), tag.c_str());
        float by = a.y + height - 14.f * k, bh = 5.f * k, bw = tr - tx - 46.f * k;
        dl->AddRectFilled({tx, by}, {tx + bw, by + bh}, IM_COL32(255, 255, 255, int(40 * alpha)), 2.f * k);
        dl->AddRectFilled({tx, by}, {tx + bw * std::clamp(w.fraction, 0.02f, 1.f), by + bh}, theme::col(c, alpha), 2.f * k);
        std::string amount = unit_.i == 1 ? i18n::fmt("{} left", w.left) : std::format("{:.0f}%", w.fraction * 100.f);
        ImVec2 as = fonts::hud()->CalcTextSizeA(ss, FLT_MAX, 0.f, amount.c_str());
        dl->AddText(fonts::hud(), ss, {tr - as.x, by + bh * 0.5f - as.y * 0.5f}, theme::col(c, alpha), amount.c_str());
        return height;
    }

    float pill(ImDrawList* dl, float cx, float y, float k, const Watch& w, int slot, double now) const {
        float pulse = 0.6f + 0.4f * std::sin(float(now) * 5.f);
        std::string amount = unit_.i == 1 ? std::format("{}", w.left) : std::format("{:.0f}%", w.fraction * 100.f);
        float is = 16.f * k, height = 22.f * k, ss = 11.f * k;
        ImVec2 as = fonts::bold()->CalcTextSizeA(ss, FLT_MAX, 0.f, amount.c_str());
        float width = is + as.x + 22.f * k;
        ImVec2 a{cx - width * 0.5f, y}, b = a + ImVec2(width, height);
        ImVec4 c = colorOf(w.level);
        dl->AddRectFilled(a, b, IM_COL32(12, 12, 16, 170), height * 0.5f);
        dl->AddRect(a, b, theme::col(c, 0.7f * pulse), height * 0.5f, 0, 1.2f * k);
        icon(dl, a + ImVec2(8.f * k, (height - is) * 0.5f), is, w, slot, 1.f);
        dl->AddText(fonts::bold(), ss, {a.x + 12.f * k + is, a.y + (height - as.y) * 0.5f}, theme::col(c, pulse), amount.c_str());
        return height;
    }

    Setting& unit_ = choice("unit", "Warn by", {"Percent", "Durability points"});
    Setting& threshold_ = slider("threshold", "Warn below (%)", 15.f, 1.f, 50.f, "%.0f%%");
    Setting& points_ = intSlider("points", "Warn at points left or less", 30, 1, 500);
    Setting& critPercent_ = slider("critPercent", "Critical below (%)", 5.f, 1.f, 25.f, "%.0f%%");
    Setting& critPoints_ = intSlider("critPoints", "Critical at points left or less", 8, 1, 200);
    Setting& armor_ = toggleSetting("armor", "Check armor", true);
    Setting& held_ = toggleSetting("held", "Check item in hand", true);
    Setting& offhand_ = toggleSetting("offhand", "Check offhand", false);
    Setting& card_ = toggleSetting("card", "Warning card", true);
    Setting& flash_ = toggleSetting("flash", "Flash the screen edges when critical", true);
    Setting& indicator_ = toggleSetting("indicator", "Keep a small indicator while critical", true);
    Setting& sound_ = choice("soundMode", "Sound", {"Off", "Critical only", "Always"}, 1);
    Setting& notify_ = toggleSetting("notify", "Notification", false);
    Setting& showTime_ = slider("showTime", "Display time (s)", 4.f, 1.f, 15.f, "%.0f s");
    Setting& size_ = slider("cardSize", "Size", 1.f, 0.6f, 2.f, "%.2fx");
    Setting& y_ = slider("cardY", "Height (fraction)", 0.12f, 0.03f, 0.9f, "%.2f");
    Setting& warnColor_ = colorSetting("warnColor", "Color low", {1.f, 0.78f, 0.3f, 1.f});
    Setting& critColor_ = colorSetting("critColor", "Color critical", {1.f, 0.32f, 0.36f, 1.f});
    std::array<Watch, 6> watch_;
    std::map<std::string, double> lastWarn_;
    double flashAt_ = -100.0;
};

class LowHealth : public Module {
public:
    LowHealth()
        : Module("Low Health Indicator", "A red, pulsing screen edge when your health is low.", Category::Hud, {"hud-self"}) {
        sub("Info displays");
        require(need::player, need::sigs({"LocalPlayer", "PlayerStats"}));
    }

    void onRender(ImDrawList* dl) override {
        if (hudOnly_.b && game::state().screen != game::Screen::None && !gui::editingHud()) return;
        auto& p = game::state().player;
        float low = threshold_.f * 2.f;
        float target = p.health > 0.f && p.health <= low ? 1.f - p.health / low : 0.f;
        level_ = draw::approach(level_, target, 6.f);
        if (level_ < 0.01f) return;
        double t = ui::time();
        float beat = pulse_.b ? 0.75f + 0.25f * std::sin(float(t) * (4.f + 6.f * level_)) : 1.f;
        float a = std::clamp(intensity_.f * level_ * beat, 0.f, 1.f);
        auto ds = ImGui::GetIO().DisplaySize;
        float e = std::min(ds.x, ds.y) * size_.f * (grow_.b ? 0.35f + 0.65f * level_ : 1.f);
        ImU32 solid = ImGui::GetColorU32(withAlpha(color_.color, a));
        ImU32 clear = ImGui::GetColorU32(withAlpha(color_.color, 0.f));
        dl->AddRectFilledMultiColor({0, 0}, {ds.x, e}, solid, solid, clear, clear);
        dl->AddRectFilledMultiColor({0, ds.y - e}, {ds.x, ds.y}, clear, clear, solid, solid);
        dl->AddRectFilledMultiColor({0, 0}, {e, ds.y}, solid, clear, clear, solid);
        dl->AddRectFilledMultiColor({ds.x - e, 0}, {ds.x, ds.y}, clear, solid, solid, clear);
    }

private:
    Setting& threshold_ = slider("threshold", "From (hearts)", 4.f, 1.f, 10.f, "%.1f ♥");
    Setting& intensity_ = slider("intensity", "Strength", 0.55f, 0.1f, 1.f, "%.2f");
    Setting& size_ = slider("size", "Edge width", 0.18f, 0.05f, 0.4f, "%.2f");
    Setting& pulse_ = toggleSetting("pulse", "Heartbeat pulse", true);
    Setting& grow_ = toggleSetting("grow", "Edge grows with lost health", false);
    Setting& hudOnly_ = toggleSetting("hudOnly", "Hide in menus", true);
    Setting& color_ = colorSetting("color", "Color", {1.f, 0.1f, 0.2f, 1.f});
    float level_ = 0.f;
};

