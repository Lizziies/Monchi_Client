#pragma once

#include "core/Config.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/Manager.hpp"
#include "modules/Module.hpp"

#include <json.hpp>

#include <string>

class ConfigSharing : public Module {
public:
    ConfigSharing()
        : Module("Config Sharing", "Turns your settings into a short code that you can send to friends, and loads codes from others.", Category::Client, {"cosmetic"}) {
        sub("Platform");
    }

    bool alwaysOn() const override { return true; }

    void drawSettings() override {
        auto& t = theme::current();
        ImGui::Spacing();
        if (ImGui::Button(i18n::tr("Copy my settings as a code"))) {
            std::string code = exportCode();
            ImGui::SetClipboardText(code.c_str());
            notify::push(i18n::tr("Config Sharing"), i18n::fmt("Code copied ({} characters)", code.size()), notify::Kind::Ok);
        }
        ImGui::SameLine();
        if (ImGui::Button(i18n::tr("Load a code from the clipboard"))) preview(ImGui::GetClipboardText() ? ImGui::GetClipboardText() : "");
        if (!status_.empty()) ImGui::TextColored(pending_.is_null() ? t.warn : t.ok, "%s", status_.c_str());
        if (!pending_.is_null()) {
            if (ImGui::Button(i18n::tr("Apply the code"))) apply();
            ImGui::SameLine();
            if (ImGui::Button(i18n::tr("Cancel"))) {
                pending_ = nullptr;
                status_.clear();
            }
        }
        ImGui::TextDisabled("%s", i18n::tr("Only load codes from people you trust."));
    }

private:
    static constexpr const char* prefix = "MCS1-";

    static std::string encode(const std::string& in) {
        static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        std::string out;
        for (size_t i = 0; i < in.size(); i += 3) {
            uint32_t v = uint8_t(in[i]) << 16 | (i + 1 < in.size() ? uint8_t(in[i + 1]) << 8 : 0) | (i + 2 < in.size() ? uint8_t(in[i + 2]) : 0);
            out += table[(v >> 18) & 63];
            out += table[(v >> 12) & 63];
            if (i + 1 < in.size()) out += table[(v >> 6) & 63];
            if (i + 2 < in.size()) out += table[v & 63];
        }
        return out;
    }

    static bool decode(const std::string& in, std::string& out) {
        auto value = [](char c) {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            return c == '-' ? 62 : c == '_' ? 63 : -1;
        };
        out.clear();
        uint32_t acc = 0;
        int bits = 0;
        for (char c : in) {
            int v = value(c);
            if (v < 0) return false;
            acc = (acc << 6) | uint32_t(v);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out += char((acc >> bits) & 255);
            }
        }
        return true;
    }

    std::string exportCode() const {
        nlohmann::json mods = nlohmann::json::object();
        for (auto& m : modules::all()) {
            if (m.get() == this || !m->userEnabled()) continue;
            auto j = m->save();
            if (!personal_.b) {
                auto& s = j["settings"];
                for (auto& set : m->settings())
                    if (set.type == SettingType::Key || set.type == SettingType::Text) s.erase(set.id);
            }
            mods[m->name()] = j;
        }
        nlohmann::json root = {{"v", 1}, {"theme", theme::save()}, {"modules", mods}, {"personal", personal_.b}};
        return prefix + encode(root.dump());
    }

    void preview(const std::string& code) {
        pending_ = nullptr;
        std::string trimmed = code;
        trimmed.erase(0, trimmed.find_first_not_of(" \r\n\t"));
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.back())) trimmed.pop_back();
        std::string raw;
        if (trimmed.rfind(prefix, 0) != 0 || !decode(trimmed.substr(5), raw)) {
            status_ = i18n::tr("This is not a Monchi settings code.");
            return;
        }
        auto j = nlohmann::json::parse(raw, nullptr, false);
        if (j.is_discarded() || !j.contains("modules") || !j["modules"].is_object()) {
            status_ = i18n::tr("The code is damaged.");
            return;
        }
        pending_ = j;
        status_ = i18n::fmt("The code has settings for {} modules. Apply it?", j["modules"].size());
    }

    void apply() {
        if (pending_.is_null()) return;
        int count = 0;
        if (pending_.contains("theme")) {
            theme::load(pending_["theme"]);
            theme::applyStyle();
        }
        for (auto& m : modules::all()) {
            if (m.get() == this) continue;
            auto it = pending_["modules"].find(m->name());
            if (it == pending_["modules"].end() || !it->is_object()) continue;
            nlohmann::json j = *it;
            if (j.contains("settings") && j["settings"].is_object())
                for (auto& set : m->settings())
                    if (set.type == SettingType::Key || set.type == SettingType::Text) {
                        if (!pending_.value("personal", false) || m->name() == "Lua Scripts") j["settings"].erase(set.id);
                    }
            m->load(j);
            count++;
        }
        config::markDirty();
        notify::push(i18n::tr("Config Sharing"), i18n::fmt("Applied settings for {} modules.", count), notify::Kind::Ok);
        pending_ = nullptr;
        status_.clear();
    }

    Setting& personal_ = toggleSetting("personal", "Include key binds and texts in the code", false);
    nlohmann::json pending_;
    std::string status_;
};
