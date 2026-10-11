#pragma once

#include "gui/Notify.hpp"
#include "modules/Module.hpp"
#include "render/Ui.hpp"
#include "sdk/Inject.hpp"

#include <atomic>

class GamemodeHotkeys : public Module {
public:
    GamemodeHotkeys()
        : Module("Gamemode Hotkeys", "Switch the game mode with a key by typing the command for you. Works where you are allowed to use the command.", Category::Comfort,
                 {"chat"}) {
        sub("Chat");
    }

    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat || inject::ours()) return;
        for (int i = 0; i < 4; i++) {
            int key = inputKeys_[i].load();
            if (!key || ev.vk != key) continue;
            int empty = -1;
            pending_.compare_exchange_strong(empty, i);
            return;
        }
    }

    void onDisable() override { pending_.store(-1); }
    void onEnable() override { publishKeys(); }

    void onFrame() override {
        publishKeys();
        int mode = pending_.exchange(-1);
        if (mode < 0) return;
        double now = ui::time();
        if (now - last_ < 1.5) return;
        last_ = now;
        const char* names[] = {"survival", "creative", "adventure", "spectator"};
        const int numbers[] = {0, 1, 2, 6};
        std::string cmd = format_.text.empty() ? "/gamemode {mode}" : format_.text;
        std::string number = std::to_string(numbers[mode]), name = names[mode];
        for (auto [key, val] : {std::pair<const char*, std::string>{"{mode}", name}, {"{id}", number}})
            for (size_t at = cmd.find(key); at != std::string::npos; at = cmd.find(key, at + val.size())) cmd.replace(at, std::strlen(key), val);
        inject::say(cmd, chatKey_.i);
        if (toast_.b) notify::push(i18n::tr("Game mode"), cmd, notify::Kind::Info, 2.f);
    }

private:
    Setting& survival_ = keySetting("survival", "Survival", 0);
    Setting& creative_ = keySetting("creative", "Creative", 0);
    Setting& adventure_ = keySetting("adventure", "Adventure", 0);
    Setting& spectator_ = keySetting("spectator", "Spectator", 0);
    Setting& format_ = textSetting("format", "Command ({mode} or {id})", "/gamemode {mode}");
    Setting& chatKey_ = keySetting("chatKey", "Chat key in game", 'T');
    Setting& toast_ = toggleSetting("toast", "Show the command that is sent", true);
    double last_ = -100.0;
    std::atomic<int> pending_{-1};
    std::atomic<int> inputKeys_[4]{};
    void publishKeys() {
        Setting* keys[] = {&survival_, &creative_, &adventure_, &spectator_};
        for (int i = 0; i < 4; ++i) inputKeys_[i] = keys[i]->i;
    }
};
