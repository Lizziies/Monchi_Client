#pragma once

#include "sdk/Game.hpp"
#include "render/Ui.hpp"

#include <imgui.h>

#include <random>
#include <string>

namespace nick {

inline bool on = false;
inline std::string name;
inline int color = -1;
inline bool bold = false;
inline bool obfuscated = false;

inline const char* const colorNames[25] = {"White", "Gray", "Black", "Red", "Dark red", "Orange", "Gold", "Yellow", "Lime", "Green", "Dark green", "Aqua", "Cyan",
                                          "Light blue", "Blue", "Dark blue", "Purple", "Violet", "Magenta", "Pink", "Hot pink", "Brown", "Salmon", "Mint", "Silver"};

inline ImU32 colorOf(int index, ImU32 fallback) {
    static const ImU32 table[25] = {IM_COL32(255, 255, 255, 255), IM_COL32(170, 170, 170, 255), IM_COL32(60, 60, 60, 255),   IM_COL32(255, 85, 85, 255),
                                    IM_COL32(170, 0, 0, 255),     IM_COL32(255, 150, 60, 255),  IM_COL32(255, 170, 0, 255),  IM_COL32(255, 255, 85, 255),
                                    IM_COL32(150, 255, 60, 255),  IM_COL32(85, 255, 85, 255),   IM_COL32(0, 170, 0, 255),    IM_COL32(85, 255, 200, 255),
                                    IM_COL32(85, 255, 255, 255),  IM_COL32(120, 180, 255, 255), IM_COL32(85, 85, 255, 255),  IM_COL32(0, 0, 170, 255),
                                    IM_COL32(170, 0, 170, 255),   IM_COL32(150, 110, 255, 255), IM_COL32(255, 85, 255, 255), IM_COL32(255, 160, 200, 255),
                                    IM_COL32(255, 60, 160, 255),  IM_COL32(150, 100, 60, 255),  IM_COL32(255, 140, 120, 255), IM_COL32(150, 255, 200, 255),
                                    IM_COL32(200, 200, 210, 255)};
    return index >= 0 && index < 25 ? table[index] : fallback;
}

inline bool mine(const std::string& who) { return on && !who.empty() && who == game::state().player.name; }

inline std::string scramble(const std::string& text) {
    static std::mt19937 rng{7};
    static double at = 0.0;
    static std::string last;
    double now = ui::time();
    if (now - at < 0.09 && last.size() == text.size()) return last;
    at = now;
    last = text;
    static const char pool[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (auto& c : last)
        if (c != ' ') c = pool[rng() % (sizeof(pool) - 1)];
    return last;
}

inline std::string show(const std::string& who) {
    if (!mine(who)) return who;
    return obfuscated ? scramble(name.empty() ? who : name) : (name.empty() ? who : name);
}

inline std::string replaceIn(std::string text) {
    if (!on || name.empty()) return text;
    const std::string& real = game::state().player.name;
    if (real.empty()) return text;
    for (size_t at = text.find(real); at != std::string::npos; at = text.find(real, at + name.size())) text.replace(at, real.size(), obfuscated ? scramble(name) : name);
    return text;
}

}
