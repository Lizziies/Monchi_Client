#pragma once

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/HudModule.hpp"
#include "render/Fonts.hpp"
#include "sdk/Game.hpp"

#include <functional>
#include <string>
#include <vector>

inline bool gameVisible() { return game::state().inWorld || gui::editingHud(); }

class GameText : public TextHud {
public:
    GameText(std::string name, std::string description, unsigned domains, std::vector<std::string> sigs, std::vector<std::string> tags,
             ImVec2 pos)
        : TextHud(std::move(name), std::move(description), std::move(tags), pos) {
        require(domains, std::move(sigs));
    }

    void onRender(ImDrawList* dl) override {
        if (!gameVisible()) return;
        TextHud::onRender(dl);
    }
};

struct Row {
    std::string text;
    ImU32 color = 0;
    float fraction = -1.f;
    ImU32 barColor = 0;
    std::string suffix;
};

class GameList : public HudModule {
public:
    GameList(std::string name, std::string description, unsigned domains, std::vector<std::string> sigs, std::vector<std::string> tags,
             ImVec2 pos)
        : HudModule(std::move(name), std::move(description), std::move(tags), pos) {
        require(domains, std::move(sigs));
    }

    void onRender(ImDrawList* dl) override {
        if (!gameVisible()) return;
        HudModule::onRender(dl);
    }
};
