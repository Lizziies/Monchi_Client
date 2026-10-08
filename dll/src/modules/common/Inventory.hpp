#pragma once

#include "sdk/Game.hpp"

#include <string>

inline int countItems(const std::string& name, int aux = -1, bool hotbarOnly = false, bool offhand = true) {
    auto& p = game::state().player;
    int n = 0;
    auto add = [&](const game::Item& it) {
        if (!it.empty() && it.name == name && (aux < 0 || it.aux == aux)) n += it.count;
    };
    for (auto& it : p.hotbar) add(it);
    if (offhand) add(p.offhand);
    if (!hotbarOnly)
        for (auto& it : p.main) add(it);
    return n;
}

inline ImU32 materialColor(const std::string& name) {
    struct M {
        const char* key;
        ImU32 color;
    };
    static const M table[] = {
        {"netherite", IM_COL32(88, 72, 86, 255)}, {"diamond", IM_COL32(98, 226, 214, 255)}, {"golden", IM_COL32(250, 214, 80, 255)},
        {"gold", IM_COL32(250, 214, 80, 255)},    {"iron", IM_COL32(205, 205, 210, 255)},   {"chainmail", IM_COL32(150, 150, 160, 255)},
        {"leather", IM_COL32(160, 100, 64, 255)}, {"turtle", IM_COL32(80, 190, 110, 255)},  {"stone", IM_COL32(140, 140, 140, 255)},
        {"wooden", IM_COL32(170, 130, 80, 255)},
    };
    for (auto& m : table)
        if (name.find(m.key) != std::string::npos) return m.color;
    return IM_COL32(220, 190, 210, 255);
}
