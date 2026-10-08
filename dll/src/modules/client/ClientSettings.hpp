#pragma once

#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/HudModule.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "core/Build.hpp"
#include "modules/Manager.hpp"
#include "modules/Module.hpp"
#include "sdk/Game.hpp"

#include <cctype>
#include <format>
#include <string>

class ClientSettings : public Module {
public:
    ClientSettings()
        : Module("Client Settings", "Name tag behind your name in chat, notifications and other global options.", Category::Client) {
        sub("Client");
        cosmetics_.hidden = true;
        tints_.hidden = true;
        tagText_.visible = [this] { return tag_.b; };
        tagColor_.visible = [this] { return tag_.b; };
        tagPos_.visible = [this] { return tag_.b; };
        brackets_.visible = [this] { return tag_.b; };
        tabTag_.visible = [this] { return tag_.b; };
    }

    bool alwaysOn() const override { return true; }

    void onFrame() override {
        notify::setMuted(!notifications_.b);
        draw::setMotion(motion_.b);
        hud::setGlobalScale(hudScale_.f);
    }

    void onRender(ImDrawList* dl) override {
        if (!invMark_.b || game::state().screen != game::Screen::Inventory) return;
        auto& t = theme::current();
        float s = ui::scale();
        auto ds = ImGui::GetIO().DisplaySize;
        float size = 22.f * s;
        ImVec2 ts = fonts::bold()->CalcTextSizeA(size, FLT_MAX, 0.f, build::name);
        ImVec2 max{ds.x - 24 * s, ds.y - 24 * s};
        ImVec2 min{max.x - ts.x - 56 * s, max.y - 40 * s};
        dl->AddRectFilled(min, max, theme::col(t.surface, 0.8f), 20 * s);
        draw::heart(dl, {min.x + 22 * s, min.y + 20 * s}, 18 * s, theme::col(t.accent));
        dl->AddText(fonts::bold(), size, {min.x + 40 * s, min.y + (40 * s - size) * 0.5f}, theme::col(t.text), build::name);
    }

    std::string tagged(const std::string& line, bool chat) const {
        if (!tag_.b || (!chat && !tabTag_.b)) return line;
        const std::string& me = game::state().player.name;
        if (me.empty()) return line;
        size_t at = nameEnd(line, me);
        if (at == std::string::npos) return line;
        const auto& c = tagColor_.color;
        std::string body = brackets_.b ? "[" + tagText_.text + "]" : tagText_.text;
        std::string tag = std::format("§#{:02X}{:02X}{:02X};{}§r", int(c.x * 255.f), int(c.y * 255.f), int(c.z * 255.f), body);
        if (chat && tagPos_.i == 1) {
            size_t close = line.find_first_of(">:", at);
            at = close == std::string::npos ? at : close + 1;
        }
        return line.substr(0, at) + " " + tag + line.substr(at);
    }

    std::string tabTag() const {
        if (!tag_.b || !tabTag_.b) return "";
        return brackets_.b ? "[" + tagText_.text + "]" : tagText_.text;
    }

    ImVec4 tagColor() const { return tagColor_.color; }
    Setting& equipped() { return cosmetics_; }
    float menuBlur() const { return menuBlur_.f; }
    Setting& slim() { return slim_; }
    Setting& spin() { return spin_; }
    Setting& animSpeed() { return animSpeed_; }
    Setting& tints() { return tints_; }

private:
    static size_t nameEnd(const std::string& line, const std::string& me) {
        for (size_t p = line.find(me); p != std::string::npos; p = line.find(me, p + 1)) {
            size_t end = p + me.size();
            bool left = p == 0 || !std::isalnum((unsigned char)line[p - 1]);
            bool right = end >= line.size() || !std::isalnum((unsigned char)line[end]);
            if (left && right) return end;
        }
        return std::string::npos;
    }

    Setting& tag_ = toggleSetting("tag", "Client tag behind my name", true);
    Setting& tagText_ = textSetting("tagText", "Tag text", "Monchi <3");
    Setting& tagColor_ = colorSetting("tagColor", "Tag color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& tagPos_ = choice("tagPos", "Tag position", {"Inside the name brackets", "Before the message"});
    Setting& brackets_ = toggleSetting("brackets", "Tag in brackets", true);
    Setting& tabTag_ = toggleSetting("tabTag", "Also in the Tab List", true);
    Setting& notifications_ = toggleSetting("notifications", "Notifications", true);
    Setting& invMark_ = toggleSetting("invMark", "Watermark in the inventory", true);
    Setting& motion_ = toggleSetting("motion", "Animations", true);
    Setting& menuBlur_ = slider("menuBlur", "Menu background blur", 0.7f, 0.f, 1.f, "%.2f");
    Setting& hudScale_ = slider("hudScale", "Default HUD size", 1.f, 0.6f, 1.6f, "%.2fx");
    Setting& cosmetics_ = textSetting("cosmetics", "Equipped cosmetics", "");
    Setting& tints_ = textSetting("cosmeticTints", "Cosmetic colors", "");
    Setting& slim_ = toggleSetting("slimArms", "Slim arms (Alex model)", false);
    Setting& spin_ = slider("previewSpin", "Turn speed", 18.f, 0.f, 120.f, "%.0f°/s");
    Setting& animSpeed_ = slider("cosmeticAnim", "Animation speed", 1.f, 0.2f, 3.f, "%.1fx");
};
