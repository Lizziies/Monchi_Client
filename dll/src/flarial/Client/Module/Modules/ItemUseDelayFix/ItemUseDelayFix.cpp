// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/ItemUseDelayFix/ItemUseDelayFix.cpp: patch and unpatch use the checked site of ItemUseDelayFix.hpp.
#include "ItemUseDelayFix.hpp"

void ItemUseDelayFix::onEnable() {
    patch();
    Module::onEnable();
}

void ItemUseDelayFix::onDisable() {
    unpatch();
    Module::onDisable();
}

void ItemUseDelayFix::defaultConfig() {
    Module::defaultConfig("core");
    
}

void ItemUseDelayFix::patch() {
    if (site.armed()) codepatch::engage(site);
}

void ItemUseDelayFix::unpatch() {
    codepatch::release(site);
}
