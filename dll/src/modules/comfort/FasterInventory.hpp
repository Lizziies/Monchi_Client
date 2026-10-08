#pragma once
#include "modules/Module.hpp"
#include "modules/common/Options.hpp"

class FasterInventory : public Module {
public:
    FasterInventory() : Module("Faster Inventory", "Opens inventory menus without Minecraft's screen transition animation. Server response times stay unchanged.", Category::Comfort, {}, {"fx.screenAnimations"}) {}
    void onEnable() override { mcopt::refresh(); }
    void onFrame() override {
        fx::force(fx::Id::ScreenAnimations, false);
        fx::hold("screen_animations", fx::Hold::Off);
    }
    void drawSettings() override {
        if (auto opts = mcopt::options()) {
            auto it = opts->find("screen_animations");
            if (it != opts->end() && it->second == "0")
                ImGui::TextWrapped("%s", i18n::tr("Minecraft screen animations are already off. This module cannot make menus faster by disabling them again."));
        }
        ImGui::TextWrapped("%s", i18n::tr("Container loading and server response times are controlled by Minecraft and the server."));
    }
};
