// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/HurtColor/HurtColor.cpp: separate local-player and other-entity damage colors.
#include "HurtColor.hpp"
#include "Bridge/HurtTargets.hpp"



void HurtColor::onEnable() {
    Listen(this, HurtColorEvent, &HurtColor::onGetHurtColor)
    Module::onEnable();
}

void HurtColor::onDisable() {
    Deafen(this, HurtColorEvent, &HurtColor::onGetHurtColor)
    Module::onDisable();
}

void HurtColor::defaultConfig() {
    settings.renameSetting("color", "colorOpacity", "color_rgb", "hurt");
    Module::defaultConfig("core");
    setDef("hurt", (std::string)"FFFFFF", 0.65f, false);
    setDef("separateTargets", false);
    setDef("colorSelf", true);
    setDef("colorOthers", true);
    setDef("selfHurt", (std::string)"FFFFFF", 0.65f, false);
    
}

void HurtColor::settingsRender(float settingsOffset) {
    initSettingsPage();

    addToggle("Color yourself", "Show your hurt tint. Turning this off hides it.", "colorSelf");
    addColorPicker("Your hurt color", "Apply the custom hurt color to your own player.", "selfHurt");
    addToggle("Color other entities", "Show opponent hurt tint. Turning this off hides it.", "colorOthers");
    addColorPicker("Opponent hurt color", "Apply the custom hurt color to other players and mobs.", "hurt");

    FlarialGUI::UnsetScrollView();
    resetPadding();
}

void HurtColor::onGetHurtColor(HurtColorEvent &event) {
    if (!this->isEnabled()) return;
    auto action = hurtTargets::action(event.hasActor(), event.isSelf(), getOps<bool>("colorSelf"), getOps<bool>("colorOthers"));
    if (action == hurtTargets::Action::Keep) return;
    if (action == hurtTargets::Action::Hide) {
        event.getHurtColor()->a = 0.f;
        return;
    }
    const char* key = hurtTargets::key(event.isSelf());
    D2D1_COLOR_F color = getColor(key);
    event.setHurtColorFromD2DColor(color, color.a);
}
