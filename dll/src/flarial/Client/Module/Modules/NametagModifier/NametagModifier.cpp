// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/NametagModifier/NametagModifier.cpp: patch and unpatch use the checked site of NametagModifier.hpp.
#include "NametagModifier.hpp"

#include "Client.hpp"
#include "Bridge/LocalNametag.hpp"


void NametagModifier::defaultConfig() {
    Module::defaultConfig("core");
    setDef("overrideNativeColors", false);
    setDef("nameTagText", (std::string)"ffffff", 1.f, false);
    setDef("nameTagBg", (std::string)"000000", 0.25f, false);
}

void NametagModifier::onEnable() {
    patch();
    Listen(this, PerspectiveEvent, &NametagModifier::onGetViewPerspective)
    Listen(this, DrawNameTagEvent, &NametagModifier::onDrawNameTag)
    Module::onEnable();
}

void NametagModifier::onDisable() {
    Deafen(this, PerspectiveEvent, &NametagModifier::onGetViewPerspective)
    Deafen(this, DrawNameTagEvent, &NametagModifier::onDrawNameTag)
    unpatch();
    Module::onDisable();
}

void NametagModifier::patch() {
    if (site.armed()) patched = codepatch::engage(site) || patched;
}

void NametagModifier::unpatch() {
    if (codepatch::release(site)) patched = false;
}

void NametagModifier::onGetViewPerspective(PerspectiveEvent &event) {
    if (auto sl = ModuleManager::getModule("SnapLook"); sl && sl->active) {
        if (!patched) patch();
    } else if (event.getPerspective() == Perspective::FirstPerson && patched) unpatch();
    else if (event.getPerspective() != Perspective::FirstPerson && !patched) patch();
}

void NametagModifier::onDrawNameTag(DrawNameTagEvent &event) {
    if (!getOps<bool>("overrideNativeColors") || !localNametag::matches(event.getTagRenderObject()->nameTag)) return;
    D2D_COLOR_F textCol = getColor("nameTagText");
    D2D_COLOR_F bgCol = getColor("nameTagBg");

    event.getTagRenderObject()->tagColor = MCCColor(bgCol.r, bgCol.g, bgCol.b, bgCol.a);
    event.getTagRenderObject()->textColor = MCCColor(textCol.r, textCol.g, textCol.b, textCol.a);
}


void NametagModifier::settingsRender(float settingsOffset) {
	initSettingsPage();

	addHeader("Nametag");
	addToggle("Customize your nametag colors", "Only your own nametag changes. Other players keep their original colors.", "overrideNativeColors");

    addColorPicker("Text Color", "Enable Customize your nametag colors to apply this color.", "nameTagText");
    addColorPicker("Background Color", "Enable Customize your nametag colors to apply this color.", "nameTagBg");

	FlarialGUI::UnsetScrollView();

	resetPadding();
}
