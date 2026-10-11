#pragma once

#include "core/Bg.hpp"
#include "hook/Input.hpp"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace mcopt {

inline bool cursorFree() { return input::cursorShown(); }

inline std::filesystem::path appData() {
    wchar_t buf[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    return n ? std::filesystem::path(buf) : std::filesystem::path();
}

inline std::filesystem::path mojang() { return appData() / L"Minecraft Bedrock" / L"Users"; }

inline std::filesystem::path newest(const wchar_t* tail) {
    std::error_code ec;
    std::filesystem::path best;
    std::filesystem::file_time_type bestTime{};
    for (auto& user : std::filesystem::directory_iterator(mojang(), ec)) {
        auto p = user.path() / L"games" / L"com.mojang" / tail;
        auto t = std::filesystem::last_write_time(p, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        if (best.empty() || t > bestTime) {
            best = p;
            bestTime = t;
        }
    }
    return best;
}

inline std::filesystem::path optionsFile() { return newest(L"minecraftpe\\options.txt"); }

inline std::map<std::string, std::string> readOptions() {
    std::map<std::string, std::string> out;
    std::ifstream in(optionsFile());
    std::string line;
    while (std::getline(in, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string v = line.substr(colon + 1);
        while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
        out[line.substr(0, colon)] = v;
    }
    return out;
}

// Reading the file means walking the user folders and parsing a few hundred lines. Modules ask for a new read when a
// game screen closes, which is the moment a hitch shows most, so it runs in the background and they pick the result
// up when its number changes.
using Options = std::map<std::string, std::string>;

struct Shared {
    std::mutex lock;
    std::shared_ptr<const Options> data;
    std::atomic<unsigned> version{0};
    std::atomic<bool> busy{false};
};

inline Shared& shared() {
    static Shared s;
    return s;
}

inline void refresh() {
    auto& s = shared();
    if (s.busy.exchange(true)) return;
    bg::run([&s] {
        auto fresh = std::make_shared<const Options>(readOptions());
        {
            std::scoped_lock g(s.lock);
            s.data = std::move(fresh);
        }
        s.version++;
        s.busy = false;
    });
}

inline unsigned version() { return shared().version.load(); }

inline std::shared_ptr<const Options> options() {
    auto& s = shared();
    std::scoped_lock g(s.lock);
    return s.data;
}

// The key the game has bound to an action ("drop", "chat"), as a Windows key code, from the options read last; 0 while
// nothing is known. The game stores a keyboard key as its code and a mouse button as its number minus 100: left, right,
// middle, wheel, then the two side buttons.
inline int gameKey(const char* action) {
    auto opts = options();
    if (!opts) return 0;
    auto value = [&](const std::string& key) {
        auto it = opts->find(key);
        return it == opts->end() ? std::string() : it->second;
    };
    std::string prefix = value("ctrl_fullkeyboardgameplay") == "1" ? "keyboard_type_1_key." : "keyboard_type_0_key.";
    int code = std::atoi(value(prefix + action).c_str());
    static const int buttons[] = {0, VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, 0, VK_XBUTTON1, VK_XBUTTON2};
    int button = code + 100;
    return code > 0 ? code : button >= 1 && button <= 6 ? buttons[button] : 0;
}

}
