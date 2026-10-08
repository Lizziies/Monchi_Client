#pragma once

#include "gui/Notify.hpp"
#include "modules/Manager.hpp"
#include "modules/Module.hpp"
#include "modules/flarial/FlarialModules.hpp"

#include "core/Config.hpp"

#include <atomic>
#include <sstream>

class StreamerMode : public Module {
public:
    StreamerMode()
        : Module("Streamer Mode", "Hides IP, coordinates, server and chat on a key. A second press brings everything back.",
                 Category::Comfort, {"cosmetic"}) {
        sub("Profiles");
    }

    void onEnable() override {
        inputKey_ = hotkey_.i;
        // what was hidden when the game closed is brought back first, or those modules stayed off for good
        hidden_ = names(kept_.text);
        if (!hidden_.empty()) bring();
        hide();
    }

    // the key arrives on the window thread; modules are switched where the frame is drawn
    void onKey(KeyEvent& ev) override {
        int key = inputKey_.load();
        if (!ev.down || ev.repeat || !key || ev.vk != key) return;
        switches_.fetch_add(1, std::memory_order_relaxed);
    }

    void onFrame() override {
        inputKey_ = hotkey_.i;
        if (!(switches_.exchange(0, std::memory_order_relaxed) & 1)) return;
        if (active_) restore();
        else hide();
    }

    void onDisable() override {
        switches_ = 0;
        if (active_) restore();
    }

private:
    static std::vector<std::string> names(const std::string& list) {
        std::vector<std::string> out;
        std::stringstream ss(list);
        std::string name;
        while (std::getline(ss, name, ',')) {
            auto a = name.find_first_not_of(' '), b = name.find_last_not_of(' ');
            if (a != std::string::npos) out.push_back(name.substr(a, b - a + 1));
        }
        return out;
    }

    void remember() {
        std::string all;
        for (auto& n : hidden_) all += (all.empty() ? "" : ",") + n;
        if (kept_.text == all) return;
        kept_.text = all;
        config::markDirty();
    }

    void bring() {
        for (auto& name : hidden_)
            if (auto* m = modules::find(name)) m->setEnabled(true);
        hidden_.clear();
        remember();
    }

    void hide() {
        for (auto& name : names(list_.text)) {
            auto* m = modules::find(name);
            if (!m || m == this || !m->userEnabled()) continue;
            m->setEnabled(false);
            hidden_.push_back(name);
        }
        remember();
        active_ = true;
        flarialModules::privateChat(true);
        if (toast_.b) notify::push("Streamer Mode", i18n::tr("Sensitive displays are off."), notify::Kind::Info);
    }

    void restore() {
        bring();
        active_ = false;
        flarialModules::privateChat(false);
        if (toast_.b) notify::push("Streamer Mode", i18n::tr("Displays are back on."), notify::Kind::Info);
    }

    Setting& hotkey_ = keySetting("hotkey", "Toggle", 0);
    Setting& list_ = textSetting("list", "Modules that get hidden (comma)", "IP Display, Coordinates, Server Display, Waypoints, Better Chat, Death Logger");
    Setting& toast_ = toggleSetting("toast", "Show notice", true);
    Setting& kept_ = hiddenText("hiddenNow");
    std::vector<std::string> hidden_;
    bool active_ = false;
    std::atomic<unsigned> switches_{0};
    std::atomic<int> inputKey_{0};

    Setting& hiddenText(const char* id) {
        Setting& s = textSetting(id, id, "");
        s.hidden = true;
        return s;
    }
};
