#pragma once

#include "core/Guard.hpp"
#include "core/Config.hpp"
#include "core/Http.hpp"
#include "core/Paths.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Text.hpp"
#include "modules/platform/Script.hpp"
#include "render/Ui.hpp"

#include <json.hpp>

#include <atomic>
#include <fstream>
#include <mutex>
#include <thread>

class LuaScripts : public Module {
public:
    LuaScripts()
        : Module("Lua Scripts", "Runs your own Lua 5.4 scripts from the scripts folder: HUD elements, reactions to hits and chat, small helpers. Safe sandbox, reloads when you save.",
                 Category::Client, {"cosmetic"}) {
        sub("Platform");
        disabled_.hidden = true;
        engine_.setFolder(paths::scripts());
    }

    ~LuaScripts() override { join(); }

    void onEnable() override {
        engine_.setFolder(paths::scripts());
        engine_.setAllowChat(chat_.b);
        engine_.setDisabled(text::split(disabled_.text, ','));
        appliedDisabled_ = disabled_.text;
        engine_.scan(true);
    }

    void onDisable() override {
        engine_.shutdown();
        join();
    }

    void onFrame() override {
        engine_.setAllowChat(chat_.b);
        if (disabled_.text != appliedDisabled_) {
            appliedDisabled_ = disabled_.text;
            engine_.setDisabled(text::split(disabled_.text, ','));
        }
        if (reload_.b) engine_.scan(false);
        engine_.events();
        engine_.tick(float(ui::dt()));
    }

    void onKey(KeyEvent& ev) override { engine_.keys(ev.vk, ev.down); }

    void onServer(const ServerEvent& ev) override { engine_.serverChanged(ev.name, ev.host, ev.joined); }

    void onRender(ImDrawList* dl) override { engine_.draw(dl); }

    void drawSettings() override {
        auto& t = theme::current();
        ImGui::Spacing();
        ImGui::TextDisabled("%s", i18n::fmt("Folder: {}", paths::scripts().string()).c_str());
        if (ImGui::SmallButton(i18n::tr("Copy the folder path"))) ImGui::SetClipboardText(paths::scripts().string().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Create an example script"))) example();
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Scan now"))) engine_.scan(true);

        auto list = engine_.list();
        if (list.empty()) ImGui::TextDisabled("%s", i18n::tr("No scripts yet. Put a .lua file in the folder."));
        std::string toggled;
        bool on = false, changed = false;
        for (auto& s : list) {
            ImGui::PushID(s.name.c_str());
            bool enabled = s.enabled;
            if (ImGui::Checkbox("##on", &enabled)) {
                toggled = s.name;
                on = enabled;
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextColored(!s.enabled ? t.textDim : s.running ? t.ok : t.warn, "%s", s.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", i18n::fmt("{} events  ·  {} HUD  ·  {} KB", s.handlers, s.hud, s.memory / 1024).c_str());
            if (s.enabled) {
                ImGui::SameLine();
                if (ImGui::SmallButton(i18n::tr("Reload"))) engine_.reload(s.name);
            }
            if (!s.error.empty()) ImGui::TextColored(t.warn, "%s", s.error.substr(0, 200).c_str());
            ImGui::PopID();
        }
        if (changed) {
            engine_.enable(toggled, on);
            std::string joined;
            for (auto& s : engine_.list())
                if (!s.enabled) joined += (joined.empty() ? "" : ",") + s.name;
            disabled_.text = joined;
            config::markDirty();
        }

        if (ImGui::CollapsingHeader(i18n::tr("Script list from GitHub"))) market();
    }

private:
    struct Entry {
        std::string name;
        std::string file;
        std::string author;
        std::string description;
    };

    void join() {
        if (fetcher_.joinable()) fetcher_.join();
    }

    void example() {
        auto file = paths::scripts() / "combo_hud.lua";
        if (std::filesystem::exists(file)) {
            notify::push(i18n::tr("Lua Scripts"), i18n::tr("combo_hud.lua already exists."), notify::Kind::Info);
            return;
        }
        std::ofstream(file) << R"(-- Shows your combo as a number with a small bar that empties after 1.5 seconds.
local last = 0

monchi.on("hit", function(e)
  last = monchi.time()
end)

monchi.on("tick", function(dt)
  local c = monchi.combat()
  if c.combo > 0 then
    local left = math.max(0, 1 - (monchi.time() - last) / 1.5)
    monchi.hud.text("combo", "Combo " .. c.combo, 0.5, 0.60, { color = 0xFF7DB5, scale = 1.4, align = "center" })
    monchi.hud.bar("combo_bar", 0.47, 0.645, 90, 5, left, { color = 0xFF7DB5 })
  else
    monchi.hud.remove("combo")
    monchi.hud.remove("combo_bar")
  end
end)
)";
        engine_.scan(true);
        notify::push(i18n::tr("Lua Scripts"), i18n::tr("Created combo_hud.lua."), notify::Kind::Ok);
    }

    void market() {
        bool busy = busy_.load();
        if (ImGui::SmallButton(busy ? i18n::tr("Loading ...") : i18n::tr("Refresh the list")) && !busy) {
            join();
            busy_ = true;
            fetcher_ = std::thread([this] { guard::call("script list", [this] {
                auto body = http::get(L"raw.githubusercontent.com", http::repoRawPath(L"scripts/index.json"), 5000);
                std::vector<Entry> out;
                if (body) {
                    auto j = nlohmann::json::parse(*body, nullptr, false);
                    if (j.is_array())
                        for (auto& e : j)
                            if (e.is_object()) out.push_back({e.value("name", ""), e.value("file", ""), e.value("author", ""), e.value("description", "")});
                }
                std::scoped_lock g(lock_);
                entries_ = std::move(out);
                failed_ = !body;
                busy_ = false;
            }); });
        }
        std::vector<Entry> entries;
        bool failed;
        {
            std::scoped_lock g(lock_);
            entries = entries_;
            failed = failed_;
        }
        if (failed) ImGui::TextDisabled("%s", i18n::tr("Could not load the list."));
        for (auto& e : entries) {
            ImGui::PushID(e.file.c_str());
            ImGui::TextUnformatted(e.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", e.author.c_str());
            ImGui::TextDisabled("%s", e.description.c_str());
            if (ImGui::SmallButton(i18n::tr("Install"))) install(e);
            ImGui::PopID();
        }
        ImGui::TextDisabled("%s", i18n::tr("Scripts run in a sandbox, but only install scripts you trust."));
    }

    void install(const Entry& e) {
        auto ok = [](const std::string& f) {
            if (f.size() < 5 || f.size() > 64 || f.substr(f.size() - 4) != ".lua") return false;
            for (char c : f)
                if (!std::isalnum((unsigned char)c) && c != '_' && c != '-' && c != '.') return false;
            return f.find("..") == std::string::npos;
        };
        if (!ok(e.file)) return;
        join();
        busy_ = true;
        std::string file = e.file;
        fetcher_ = std::thread([this, file] { guard::call("script install", [this, file] {
            auto body = http::get(L"raw.githubusercontent.com", http::repoRawPath(L"scripts/" + std::wstring(file.begin(), file.end())), 5000);
            if (body && body->size() < 200000) std::ofstream(paths::scripts() / file, std::ios::binary | std::ios::trunc) << *body;
            std::scoped_lock g(lock_);
            installed_ = body ? file : std::string();
            busy_ = false;
        }); });
        ImGui::TextDisabled("%s", i18n::tr("Installing ..."));
    }

    Setting& chat_ = toggleSetting("chat", "Let scripts type in chat (monchi.say)", false);
    Setting& reload_ = toggleSetting("reload", "Reload scripts when the file changes", true);
    Setting& disabled_ = textSetting("disabled", "Disabled scripts", "");
    std::string appliedDisabled_;
    script::Engine engine_;
    std::thread fetcher_;
    std::atomic<bool> busy_{false};
    std::mutex lock_;
    std::vector<Entry> entries_;
    std::string installed_;
    bool failed_ = false;
};
