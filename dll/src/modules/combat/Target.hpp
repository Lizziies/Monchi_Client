#pragma once

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include <sstream>

class TargetHud : public GameList {
public:
    TargetHud()
        : GameList("Target HUD", "Name and distance of the opponent you aim at, health only on request.",
                   need::target, need::sigs({"LocalPlayer", "Target"}), {"info-others"}, {0.40f, 0.62f}) {
        sub("Combat displays");
        background_.b = true;
    }

    void onFrame() override {
        auto& t = game::state().target;
        if (t.kind == game::Target::Kind::Entity && (!playersOnly_.b || t.isPlayer)) {
            last_ = t;
            seen_ = ui::time();
        }
    }

    void onRender(ImDrawList* dl) override {
        bool editing = gui::editingHud();
        if (!editing && ui::time() - seen_ > linger_.f) return;
        if (editing && last_.name.empty()) {
            last_.name = i18n::tr("Opponent");
            last_.health = 14.f;
            last_.distance = 2.8f;
        }
        GameList::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        float w = width_.f * s;
        float y = 0.f;
        if (name_.b) y += drawText(dl, o, s, last_.name, nameAccent_.b ? accentColor() : textColor()).y;
        float frac = last_.maxHealth > 0.f ? std::clamp(last_.health / last_.maxHealth, 0.f, 1.f) : 0.f;
        smooth_ = draw::approach(smooth_, frac, 10.f);
        bool health = healthMode_.i == 2 || (healthMode_.i == 1 && !last_.isPlayer);
        auto bar = [&] {
            if (!bar_.b || !health) return;
            ImVec2 b0 = o + ImVec2(0, y + 3 * s);
            float h = barHeight_.f * s;
            dl->AddRectFilled(b0, b0 + ImVec2(w, h), IM_COL32(0, 0, 0, 80), h * 0.5f);
            ImVec4 c = rampColor(1.f - smooth_, 0.f, 1.f, good_.color, mid_.color, low_.color);
            dl->AddRectFilled(b0, b0 + ImVec2(w * smooth_, h), ImGui::GetColorU32(c), h * 0.5f);
            y += h + 6 * s;
        };
        if (barPlace_.i == 0) bar();
        std::string info;
        if (text_.b && health)
            info += healthAs_.i == 1 || hearts_.b ? std::format("{:.1f} ♥", last_.health / 2.f)
                    : healthAs_.i == 2 ? std::format("{:.0f}%", frac * 100.f) : std::format("{:.0f} / {:.0f}", last_.health, last_.maxHealth);
        if (dist_.b) info += std::format("{}{:.{}f}{}", info.empty() ? "" : sep_.text, last_.distance, decimals_.i, unit_.text);
        if (!info.empty()) y += drawText(dl, o + ImVec2(0, y), s, info, textColor()).y;
        if (barPlace_.i == 1) bar();
        return {w, std::max(y, 14.f * s)};
    }

private:
    Setting& width_ = slider("width", "Width", 150.f, 90.f, 320.f, "%.0f");
    Setting& name_ = toggleSetting("name", "Name", true);
    Setting& healthMode_ = choice("healthMode", "Show health", {"Never", "Only for mobs", "Always"}, 1);
    Setting& bar_ = toggleSetting("bar", "Health bar", true);
    Setting& text_ = toggleSetting("text", "Health value", true);
    Setting& hearts_ = toggleSetting("hearts", "In hearts", false);
    Setting& dist_ = toggleSetting("dist", "Distance", true);
    Setting& playersOnly_ = toggleSetting("players", "Players only", true);
    Setting& linger_ = slider("linger", "Display time after looking away (s)", 2.f, 0.f, 10.f, "%.1f s");
    Setting& nameAccent_ = toggleSetting("nameAccent", "Name in accent color", true);
    Setting& healthAs_ = choice("healthAs", "Health value as", {"Points of max", "Hearts", "Percent"});
    Setting& barHeight_ = slider("barHeight", "Bar height", 8.f, 3.f, 20.f, "%.0f");
    Setting& barPlace_ = choice("barPlace", "Health bar position", {"Under the name", "Under the text"});
    Setting& decimals_ = intSlider("decimals", "Distance decimals", 1, 0, 2);
    Setting& unit_ = textSetting("unit", "Distance unit", " m");
    Setting& sep_ = textSetting("sep", "Separator", "  ·  ");
    Setting& good_ = colorSetting("good", "Color full health", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Color half health", {1.f, 0.82f, 0.49f, 1.f});
    Setting& low_ = colorSetting("low", "Color low health", {1.f, 0.4f, 0.45f, 1.f});
    game::Target last_;
    double seen_ = -100.0;
    float smooth_ = 1.f;
};

class Waila : public GameList {
public:
    Waila()
        : GameList("Waila", "Shows what you are looking at: block or entity with name, distance and break progress.", need::target,
                   need::sigs({"LocalPlayer", "Target"}), {"hud-self"}, {0.40f, 0.02f}) {
        sub("Combat displays");
    }

    void onRender(ImDrawList* dl) override {
        if (game::state().target.kind == game::Target::Kind::None && !air_.b && !gui::editingHud()) return;
        GameList::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& t = game::state().target;
        std::string name = title(t);
        // in one line the details follow the name, otherwise each gets a line of its own
        std::vector<std::string> details;
        if (t.kind == game::Target::Kind::Entity) {
            if (healthMode_.i == 2 || (healthMode_.i == 1 && !t.isPlayer)) details.push_back(i18n::fmt("{:.0f} / {:.0f} health", t.health, t.maxHealth));
        } else if (t.kind == game::Target::Kind::Block && coords_.b) {
            details.push_back(std::format("{} {} {}", t.blockX, t.blockY, t.blockZ));
        }
        if (dist_.b) details.push_back(distText_.text.empty() ? i18n::fmt("{:.1f} blocks away", t.distance) : std::format("{:.{}f}{}", t.distance, decimals_.i, distText_.text));
        float y = 0.f, w = 0.f;
        if (oneLine_.b) {
            float x = drawText(dl, o, s, name, nameAccent_.b ? accentColor() : textColor()).x;
            for (auto& d : details) x += drawText(dl, o + ImVec2(x, 0), s, sep_.text + d, textColor()).x;
            w = x;
            y = textSize(s, name).y;
        } else {
            y = drawText(dl, o, s, name, nameAccent_.b ? accentColor() : textColor()).y;
            w = textSize(s, name).x;
            for (auto& d : details) {
                y += drawText(dl, o + ImVec2(0, y), s, d, textColor()).y;
                w = std::max(w, textSize(s, d).x);
            }
        }
        if (progress_.b && t.kind == game::Target::Kind::Block && t.breakProgress > 0.f) {
            ImVec2 b0 = o + ImVec2(0, y + 2 * s);
            float bw = std::max(w, barWidth_.f * s), bh = barHeight_.f * s;
            dl->AddRectFilled(b0, b0 + ImVec2(bw, bh), IM_COL32(0, 0, 0, 80), bh * 0.5f);
            dl->AddRectFilled(b0, b0 + ImVec2(bw * t.breakProgress, bh), ImGui::GetColorU32(barColor_.color), bh * 0.5f);
            y += bh + 4 * s;
            w = bw;
        }
        return {w, y};
    }

private:
    std::string title(const game::Target& t) const {
        if (t.kind == game::Target::Kind::None) return gui::editingHud() && !air_.b ? i18n::tr("Block") : advanced_.b ? "minecraft:air" : i18n::tr("Air");
        if (t.name.empty()) return i18n::tr(t.kind == game::Target::Kind::Block ? "Block" : "Entity");
        if (t.isPlayer) return t.name;
        if (!advanced_.b) return text::pretty(t.name);
        return t.name.find(':') == std::string::npos ? "minecraft:" + t.name : t.name;
    }

    Setting& healthMode_ = choice("healthMode", "Show health", {"Never", "Only for mobs", "Always"}, 1);
    Setting& dist_ = toggleSetting("dist", "Distance", true);
    Setting& coords_ = toggleSetting("coords", "Block coordinates", false);
    Setting& progress_ = toggleSetting("progress", "Break progress", true);
    Setting& advanced_ = toggleSetting("advanced", "Advanced mode (full ids like minecraft:stone)", false);
    Setting& air_ = toggleSetting("showAir", "Show air when you look at nothing", false);
    Setting& oneLine_ = toggleSetting("oneLine", "Everything in one line", false);
    Setting& sep_ = textSetting("sep", "Separator", "  ·  ");
    Setting& nameAccent_ = toggleSetting("nameAccent", "Name in accent color", true);
    Setting& distText_ = textSetting("distText", "Text after the distance (empty uses default)", "");
    Setting& decimals_ = intSlider("decimals", "Distance decimals", 1, 0, 2);
    Setting& barWidth_ = slider("barWidth", "Bar width", 110.f, 40.f, 300.f, "%.0f");
    Setting& barHeight_ = slider("barHeight", "Bar height", 5.f, 2.f, 16.f, "%.0f");
    Setting& barColor_ = colorSetting("barColor", "Bar color", {0.23f, 0.65f, 0.93f, 1.f});
};

class BowCharge : public Module {
public:
    BowCharge()
        : Module("Bow Charge", "Shows how far your bow is drawn, right at the crosshair.", Category::Pvp, {"hud-self"}) {
        sub("Combat displays");
        require(need::player | need::inventory, need::sigs({"LocalPlayer", "UseState"}));
    }

    void onRender(ImDrawList* dl) override {
        auto& p = game::state().player;
        bool active = p.usingItem && (!onlyBow_.b || p.held().name == "bow");
        shown_ = draw::approach(shown_, active ? 1.f : 0.f, 18.f);
        if (shown_ < 0.02f) return;
        float f = std::clamp(p.useProgress, 0.f, 1.f);
        bool full = f >= 0.995f;
        ImVec4 col = full ? fullColor_.color : theme::mix(color_.color, fullColor_.color, f * f);
        col.w *= shown_;
        ImVec2 c = screenCenter() + ImVec2(offsetX_.f, offsetY_.f);
        float s = ui::scale();
        if (style_.i == 0) {
            float w = width_.f * s, h = 6.f * s;
            ImVec2 a = c - ImVec2(w * 0.5f, 0), b = a + ImVec2(w, h);
            dl->AddRectFilled(a - ImVec2(1, 1), b + ImVec2(1, 1), IM_COL32(0, 0, 0, int(120 * shown_)), h * 0.5f);
            dl->AddRectFilled(a, a + ImVec2(w * f, h), ImGui::GetColorU32(col), h * 0.5f);
        } else {
            float r = width_.f * 0.25f * s;
            dl->AddCircle(c, r, IM_COL32(0, 0, 0, int(120 * shown_)), 32, 4 * s);
            dl->PathArcTo(c, r, -1.5708f, -1.5708f + 6.2832f * f, 32);
            dl->PathStroke(ImGui::GetColorU32(col), 0, 3 * s);
        }
        if (percent_.b) {
            std::string t = std::format("{:.0f}%", f * 100.f);
            draw::textCentered(dl, fonts::bold(), 13.f * s, c + ImVec2(0, 16 * s), ImGui::GetColorU32(col), t.c_str());
        }
    }

private:
    static ImVec2 screenCenter() {
        auto ds = ImGui::GetIO().DisplaySize;
        return {std::floor(ds.x * 0.5f) + 0.5f, std::floor(ds.y * 0.5f) + 0.5f};
    }

    Setting& style_ = choice("style", "Shape", {"Bar", "Ring"});
    Setting& width_ = slider("width", "Size", 80.f, 30.f, 200.f, "%.0f");
    Setting& offsetX_ = slider("offsetX", "Offset X", 0.f, -300.f, 300.f, "%.0f");
    Setting& offsetY_ = slider("offsetY", "Offset Y", 36.f, -300.f, 300.f, "%.0f");
    Setting& percent_ = toggleSetting("percent", "Show percent", true);
    Setting& onlyBow_ = toggleSetting("onlyBow", "Bow only", true);
    Setting& color_ = colorSetting("color", "Color", {1.f, 1.f, 1.f, 0.9f});
    Setting& fullColor_ = colorSetting("full", "Color at full draw", {0.55f, 0.91f, 0.69f, 1.f});
    float shown_ = 0.f;
};

class CooldownIndicator : public GameList {
public:
    CooldownIndicator()
        : GameList("Cooldown Indicator", "Shows cooldowns, for example for ender pearls. You choose which items count.",
                   need::combat | need::inventory, need::sigs({"LocalPlayer", "ItemUseEvents"}), {"hud-self"}, {0.135f, 0.306f}) {
        sub("Combat displays");
    }

    void onFrame() override {
        parse();
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::ItemUse) continue;
            for (auto& r : rules_)
                if (r.item == e.item) active_[r.item] = {ui::time(), r.seconds};
        }
    }

    void onRender(ImDrawList* dl) override {
        std::erase_if(active_, [](auto& kv) { return ui::time() - kv.second.first > kv.second.second; });
        if (active_.empty() && !gui::editingHud()) return;
        GameList::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        float y = 0.f, w = width_.f * s;
        auto show = [&](const std::string& item, double start, double len) {
            float left = float(len - (ui::time() - start));
            float frac = len > 0 ? std::clamp(left / float(len), 0.f, 1.f) : 0.f;
            if (drains_.i == 1) frac = 1.f - frac;
            std::string time = std::format("{:.{}f}{}", std::max(left, 0.f), decimals_.i, unit_.text);
            std::string t = show_.i == 1 ? time : show_.i == 2 ? text::pretty(item) : text::pretty(item) + sep_.text + time;
            auto sz = drawText(dl, o + ImVec2(0, y), s, t, textColor());
            w = std::max(w, sz.x);
            y += sz.y;
            if (bar_.b) {
                ImVec2 b0 = o + ImVec2(0, y + 1 * s);
                float bh = barHeight_.f * s;
                dl->AddRectFilled(b0, b0 + ImVec2(w, bh), IM_COL32(0, 0, 0, 80), bh * 0.5f);
                dl->AddRectFilled(b0, b0 + ImVec2(w * frac, bh), ImGui::GetColorU32(color_.color), bh * 0.5f);
                y += bh + 3 * s;
            }
        };
        if (active_.empty()) show("ender_pearl", ui::time(), 1.0);
        for (auto& [item, v] : active_) show(item, v.first, v.second);
        return {w, y};
    }

private:
    struct Rule {
        std::string item;
        float seconds;
    };

    void parse() {
        if (parsed_ == spec_.text) return;
        parsed_ = spec_.text;
        rules_.clear();
        std::stringstream ss(parsed_);
        std::string part;
        while (std::getline(ss, part, ',')) {
            auto eq = part.find('=');
            if (eq == std::string::npos) continue;
            std::string name = part.substr(0, eq);
            name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
            try {
                rules_.push_back({name, std::stof(part.substr(eq + 1))});
            } catch (...) {
            }
        }
    }

    Setting& spec_ = textSetting("spec", "Items (name=seconds, comma separated)", "ender_pearl=1.0,chorus_fruit=1.0");
    Setting& bar_ = toggleSetting("bar", "Bar", true);
    Setting& color_ = colorSetting("color", "Bar color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& show_ = choice("show", "Text", {"Name and time", "Time only", "Name only"});
    Setting& sep_ = textSetting("sep", "Separator", "  ");
    Setting& unit_ = textSetting("unit", "Time unit", "s");
    Setting& decimals_ = intSlider("decimals", "Time decimals", 1, 0, 2);
    Setting& width_ = slider("width", "Minimum width", 120.f, 60.f, 300.f, "%.0f");
    Setting& barHeight_ = slider("barHeight", "Bar height", 4.f, 2.f, 14.f, "%.0f");
    Setting& drains_ = choice("drains", "Bar", {"Empties", "Fills"});
    std::string parsed_;
    std::vector<Rule> rules_;
    std::map<std::string, std::pair<double, float>> active_;
};
