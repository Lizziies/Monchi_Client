#pragma once
#include <atomic>
#include <initializer_list>
#include <string_view>

namespace nativeScreen {
class State {
public:
    void observe(std::string_view name, unsigned long long now) {
        int kind = classify(name);
        if (!kind) return;
        if (kind != 1) {
            menu_.store(kind);
            menuAt_.store(now);
        }
        current_.store(kind);
        updated_.store(now);
    }

    int read(unsigned long long now) const {
        auto updated = updated_.load();
        if (!updated || (now >= updated && now - updated > 1000)) return 0;
        auto menuAt = menuAt_.load();
        if (menuAt && (now < menuAt || now - menuAt <= 100)) return menu_.load();
        return current_.load();
    }

private:
    static int classify(std::string_view name) {
        if (name == "hud_screen") return 1;
        if (name == "pause_screen") return 2;
        if (name.find("inventory") != name.npos || name.find("chest") != name.npos ||
            name.find("crafting") != name.npos || name.find("container") != name.npos) return 3;
        if (name == "chat_screen") return 4;
        for (auto part : {"start_screen", "play_screen", "settings", "options", "loading", "disconnect", "death_screen"})
            if (name.find(part) != name.npos) return 5;
        return 0;
    }
    std::atomic<int> current_{0}, menu_{0};
    std::atomic<unsigned long long> updated_{0}, menuAt_{0};
};
inline State state;
}
