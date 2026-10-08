#pragma once

#include "Online.hpp"
#include "core/Config.hpp"
#include "core/Log.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "gui/Notify.hpp"
#include "modules/Manager.hpp"
#include "modules/client/ClientSettings.hpp"
#include "cosmetics/Cosmetics.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "sdk/Game.hpp"
#include "cosmetics/World.hpp"
#include <json.hpp>

#include <string>
#include <vector>

class MonchiOnline : public Module {
public:
    MonchiOnline()
        : Module("Monchi Online",
                 "Shows other Monchi users with a heart and a colored name in the Tab List and chat, and lets them see yours. Sends only your gamertag, the server name, your style and your cosmetics to the Monchi service.",
                 Category::Client, {"cosmetic"}) {
        sub("Online");
        informed_.hidden = true;
        world_.hidden = true;
        worldDefault_.hidden = true;
        // the game's own chat; with Better Chat in its place that one draws the styles itself
        game::setChatDecor([this](std::string& sender, std::string& body) {
            return enabled() && !game::chatHudHidden() && online::decorate(sender, body, showColors_.b, showHearts_.b);
        });
        colorB_.visible = [this] { return mode_.i == 1 || mode_.i == 3; };
        speed_.visible = [this] { return mode_.i >= 2; };
        heartColor_.visible = [this] { return heart_.b; };
        url_.visible = [this] { return !demo_.b; };
    }

    void onEnable() override {
        if (informed_.b) return;
        informed_.b = true;
        notify::push(i18n::tr("Monchi Online"), i18n::tr("Your gamertag, the server name and your style are now sent to the Monchi service. Turn this module off to stop."), notify::Kind::Info, 8.f);
    }

    void onDisable() override {
        push(false);
        cosmetics::world::stop();
        flarialModules::nameStyles({});
    }

    void onFrame() override {
        if (worldSavePending_) { config::markDirty(); worldSavePending_ = false; }
        sync();
        // drawn by the game itself through the core, so walls cover them; without the core nothing is drawn
        flarialModules::worldMeshPlace(std::clamp(worldPlace_.i, 0, 2));
        cosmetics::world::flipLegs(worldLegs_.b);
        if (world_.b) cosmetics::world::update(worldSelf_.b, true, worn_);
        else cosmetics::world::stop();
        if (world_.b && cosmetics::world::state() == 1 && !worldSaid_) {
            worldSaid_ = true;
            logger::info("online: cosmetics are drawn on players in the world");
        }
    }

    void drawSettings() override {
        auto& t = theme::current();
        ImGui::Spacing();
        auto st = online::state();
        ImVec4 c = st == online::State::Online || st == online::State::Demo ? t.ok : st == online::State::Off ? t.textDim : t.warn;
        ImGui::TextColored(c, "%s", online::stateText().c_str());
        if (!world_.b && !worn_.empty()) {
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextColored(t.warn, "%s", i18n::tr("World cosmetics are switched off. Enable the switch at the top of the Cosmetics tab to see equipped cosmetics on players."));
            ImGui::PopTextWrapPos();
        }
        if (world_.b && cosmetics::world::state() == 2)
            ImGui::TextColored(t.warn, "%s", i18n::tr("Cosmetics in the world are not available on this game version."));
        if (st == online::State::Online || st == online::State::Demo) ImGui::TextDisabled("%s", i18n::fmt("{} Monchi users in this list", online::count()).c_str());
        auto list = online::users();
        int shown = 0;
        for (auto& u : list) {
            if (shown++ >= 12) break;
            int idx = 0, total = 0;
            for (unsigned char ch : u.name)
                if ((ch & 0xC0) != 0x80) total++;
            for (size_t i = 0; i < u.name.size();) {
                size_t n = 1;
                unsigned char ch = (unsigned char)u.name[i];
                if (ch >= 0xF0) n = 4;
                else if (ch >= 0xE0) n = 3;
                else if (ch >= 0xC0) n = 2;
                std::string piece = u.name.substr(i, n);
                ImGui::PushStyleColor(ImGuiCol_Text, online::color(u.style, ImGui::GetTime(), idx++, total));
                ImGui::TextUnformatted(piece.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 0);
                i += n;
            }
            if (u.style.heart) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                float h = ImGui::GetTextLineHeight();
                online::heartIcon(ImGui::GetWindowDrawList(), {p.x + h * 0.7f, p.y + h * 0.5f}, h * 0.8f, online::rgb(u.style.heartColor));
                ImGui::Dummy({h * 1.2f, h});
            }
            ImGui::NewLine();
        }
        ImGui::Spacing();
        if (ImGui::Button(i18n::tr("Delete my data from the service"))) {
            online::forget();
            notify::push(i18n::tr("Monchi Online"), i18n::tr("Your data will be deleted from the service."), notify::Kind::Ok);
        }
        ImGui::TextDisabled("%s", i18n::tr("The service stores your gamertag, style, cosmetics and the time you were last seen. Nothing from chat, no location, no worlds."));
    }

    bool& worldCosmetics() { return world_.b; }

    void load(const nlohmann::json& j) override {
        Module::load(j);
        if (!j.contains("settings") || !j["settings"].is_object() || !j["settings"].contains("worldDefaultV1")) {
            world_.b = true;
            worldSavePending_ = true;
        }
    }

    bool hearts() const { return enabled() && showHearts_.b; }
    bool colors() const { return enabled() && showColors_.b; }

private:
    void sync() {
        const auto now = GetTickCount64();
        if (now < nextSync_) return;
        nextSync_ = now + 100;
        online::Style s;
        s.mode = online::Mode(mode_.i);
        s.a = rgb(colorA_.color);
        s.b = rgb(colorB_.color);
        s.speed = speed_.f;
        s.heartColor = rgb(heartColor_.color);
        s.heart = heart_.b;
        if (auto* cs = modules::get<ClientSettings>()) {
            s.tag = cs->tabTag();
            s.tagColor = rgb(cs->tagColor());
        }
        online::setStyle(s);
        worn_ = equipped();
        online::setWorn(worn_);
        push(true);
    }

    ULONGLONG nextSync_ = 0;
    std::vector<online::Worn> worn_;
    static uint32_t rgb(ImVec4 c) {
        return (uint32_t(c.x * 255.f + 0.5f) << 16) | (uint32_t(c.y * 255.f + 0.5f) << 8) | uint32_t(c.z * 255.f + 0.5f);
    }

    const std::vector<online::Worn>& equipped() {
        auto* cs = modules::get<ClientSettings>();
        if (!cs) { cachedWorn_.clear(); cacheReady_ = false; return cachedWorn_; }
        const auto generation = cosmetics::generation();
        const std::string& list = cs->equipped().text;
        const std::string& tints = cs->tints().text;
        if (cacheReady_ && cachedList_ == list && cachedTints_ == tints && cachedGeneration_ == generation) return cachedWorn_;
        std::vector<online::Worn> out;
        auto saved = nlohmann::json::parse(tints, nullptr, false);
        for (size_t from = 0; from <= list.size();) {
            size_t to = list.find(',', from);
            std::string id = list.substr(from, to == std::string::npos ? std::string::npos : to - from);
            if (auto* item = cosmetics::find(id)) {
                online::Worn w{item->id, {}};
                for (size_t i = 0; i < item->tints.size(); i++) {
                    uint32_t color = rgb(item->tints[i].color);
                    if (saved.is_object() && saved.contains(id) && saved[id].is_array() && i < saved[id].size() && saved[id][i].is_string())
                        color = online::parseHex(saved[id][i].get<std::string>(), color);
                    w.tint.push_back(color);
                }
                out.push_back(std::move(w));
            }
            if (to == std::string::npos) break;
            from = to + 1;
        }
        cachedList_ = list;
        cachedTints_ = tints;
        cachedGeneration_ = generation;
        cacheReady_ = true;
        cachedWorn_ = std::move(out);
        return cachedWorn_;
    }

    std::string cachedList_, cachedTints_;
    unsigned cachedGeneration_ = 0;
    bool cacheReady_ = false;
    std::vector<online::Worn> cachedWorn_;

    void push(bool on) {
        online::Config cfg;
        cfg.on = on;
        cfg.visible = visible_.b;
        cfg.demo = on && (demo_.b || game::demo());
        cfg.url = url_.text;
        cfg.server = game::state().server;
        std::vector<std::string> names;
        for (auto& e : game::state().tab) names.push_back(e.name);
        online::tick(cfg, names, game::state().player.name);
    }

    Setting& visible_ = toggleSetting("visible", "Visible to other Monchi users", true);
    Setting& mode_ = choice("mode", "Name style", {"Solid", "Gradient", "Rainbow", "Pulse"});
    Setting& colorA_ = colorSetting("colorA", "Name color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& colorB_ = colorSetting("colorB", "Second color", {1.f, 1.f, 1.f, 1.f});
    Setting& speed_ = slider("speed", "Speed", 1.f, 0.2f, 4.f, "%.1fx");
    Setting& heart_ = toggleSetting("heart", "Heart before my name", true);
    Setting& heartColor_ = colorSetting("heartColor", "Heart color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& showHearts_ = toggleSetting("showHearts", "Show hearts of other users", true);
    Setting& showColors_ = toggleSetting("showColors", "Show name colors of other users", true);
    Setting& world_ = toggleSetting("worldCosmetics", "Show cosmetics on players in the world (experimental)", true);
    Setting& worldDefault_ = toggleSetting("worldDefaultV1", "World cosmetics default applied", true);
    Setting& worldSelf_ = toggleSetting("worldSelf", "Also on myself in third person", true);
    Setting& worldPlace_ = choice("worldPlace", "Cosmetics are drawn (test)", {"Before the world", "With the name tags", "After the world"}, 1);
    Setting& worldLegs_ = toggleSetting("worldLegs", "Shoes swing the other way (test)", false);
    bool worldSaid_ = false;
    bool worldSavePending_ = false;
    Setting& url_ = textSetting("url", "Service address", build::onlineUrl);
    Setting& demo_ = toggleSetting("demo", "Made-up users, no network", false);
    Setting& informed_ = toggleSetting("informed", "Informed", false);
};
