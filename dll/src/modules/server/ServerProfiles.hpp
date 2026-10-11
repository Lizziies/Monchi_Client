#pragma once

#include "core/Config.hpp"
#include "core/Paths.hpp"
#include "sdk/Game.hpp"
#include "gui/Notify.hpp"
#include "modules/Module.hpp"
#include "modules/common/Text.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

class ServerProfiles : public Module {
public:
    ServerProfiles()
        : Module("Server Profiles", "Automatically switches your settings profile when you join a server and back afterwards.", Category::Server,
                 {"cosmetic"}) {
        sub("Profiles");
    }

    void onServer(const ServerEvent& ev) override {
        if (!ev.joined) {
            back();
            return;
        }
        std::stringstream ss(rules_.text);
        std::string line;
        while (std::getline(ss, line, ';')) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string server = trim(line.substr(0, eq)), profile = trim(line.substr(eq + 1));
            if (server.empty() || profile.empty()) continue;
            if (text::lower(ev.name).find(text::lower(server)) == std::string::npos && text::lower(ev.host).find(text::lower(server)) == std::string::npos) continue;
            auto all = config::profiles();
            if (std::find(all.begin(), all.end(), profile) == all.end() || profile == config::profile()) return;
            // kept on disk in a file of its own: the profile it belongs to is the one being left
            previous_ = config::profile();
            std::ofstream(paths::root() / L"profile_before_server.txt", std::ios::trunc) << previous_;
            config::switchProfile(profile);
            if (toast_.b) notify::push(i18n::tr("Profile switched"), profile + i18n::tr(" for ") + ev.name, notify::Kind::Ok);
            return;
        }
    }

    // the game was closed while a server's profile was on: the next start goes back to the one from before
    void onEnable() override {
        std::ifstream in(paths::root() / L"profile_before_server.txt");
        leftOver_ = in && std::getline(in, previous_);
    }

    // not while the modules are still being switched on: a profile change loads every module's settings
    void onFrame() override {
        if (!leftOver_) return;
        leftOver_ = false;
        if (game::state().server.empty()) back();
    }

private:
    void back() {
        if (restore_.b && !previous_.empty()) {
            auto all = config::profiles();
            if (std::find(all.begin(), all.end(), previous_) != all.end() && previous_ != config::profile()) config::switchProfile(previous_);
        }
        previous_.clear();
        std::error_code ec;
        std::filesystem::remove(paths::root() / L"profile_before_server.txt", ec);
    }

    static std::string trim(const std::string& s) {
        auto a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }

    Setting& rules_ = textSetting("rules", "Server=profile, separate with semicolons", "The Hive=pvp; Zeqa=pvp");
    Setting& restore_ = toggleSetting("restore", "Switch back when leaving", true);
    Setting& toast_ = toggleSetting("toast", "Show notice", true);
    std::string previous_;
    bool leftOver_ = false;
};
