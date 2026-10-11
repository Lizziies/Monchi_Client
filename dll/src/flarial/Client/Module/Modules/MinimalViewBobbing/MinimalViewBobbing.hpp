// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/MinimalViewBobbing/MinimalViewBobbing.hpp: on 1.26.52 the camera bob
// system (0x6730a00) starts with a six byte jne that skips the whole bob when its parameter block is not flagged;
// making that jump unconditional removes the camera shake and leaves the hand bobbing to the game.
#pragma once

#include "../Module.hpp"
#include "Utils/Memory/PatchSite.hpp"


class MinimalViewBobbing : public Module {
private:
    static inline codepatch::Site site;
public:
    MinimalViewBobbing() : Module("Minimal View Bobbing", "Prevent camera shake when view bobbing is on.",
                           IDR_EYE_PNG, "", false) {
        if (!codepatch::armJneToJmp(site, GET_SIG_ADDRESS("MinimalViewBobbingTiltBranch")))
            Logger::warn("Minimal View Bobbing: MinimalViewBobbingTiltBranch is missing or not a jne, module stays inactive");
    };

    void onEnable() override {
        patch();
        Module::onEnable();
    }

    void onDisable() override {
        unpatch();
        Module::onDisable();
    }

    void defaultConfig() override {
        Module::defaultConfig("core");
    }

    static void patch() {
        if (!site.armed()) return;
        bool done = codepatch::engage(site);
        Logger::info("minimal view bobbing: camera bob branch at {:#x} {}", site.address, done ? "patched" : "could not be patched");
    }

    static void unpatch() {
        codepatch::release(site);
    }
};
