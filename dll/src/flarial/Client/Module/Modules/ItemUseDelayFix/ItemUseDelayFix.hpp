// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/ItemUseDelayFix/ItemUseDelayFix.hpp: the patched six bytes are the
// dispatch call of the vtable slot that stores "now + 200 ms" as the next allowed block break after an attack
// (handleBuildAction, 1.26.52 0x5d95d4e); it is only patched when the bytes are FF 15.
#pragma once

#include "../Module.hpp"
#include "Utils/Memory/PatchSite.hpp"


class ItemUseDelayFix : public Module {
private:
    static inline codepatch::Site site;
public:
    ItemUseDelayFix() : Module("Item Use Delay Fix", "Removes 200ms delay after attack on using items (e.g projectiles).",
                           IDR_NAMETAG_PNG, "") {
        uintptr_t address = GET_SIG_ADDRESS("ClientInputCallbacks::handleBuildAction_onAttack_setNoBlockBreakUntil_CallPatch");
        uint8_t call[6]{};
        constexpr uint8_t nops[6]{0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        if (address && codepatch::readBytes(address, call, sizeof call) && call[0] == 0xFF && call[1] == 0x15)
            codepatch::armBytes(site, address, call, sizeof call, nops);
        if (!site.armed()) Logger::warn("Item Use Delay Fix: call site not found, module stays inactive");
    };

    void onEnable() override;

    void onDisable() override;

    void defaultConfig() override;

    static void patch();

    static void unpatch();
};
