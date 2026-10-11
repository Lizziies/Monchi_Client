// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/OverworldFogColorHook.cpp: return the game-owned output, never the temporary event color.
#include "OverworldFogColorHook.hpp"

namespace {
void editFog(MCCColor& output) {
    auto event = nes::make_holder<FogColorEvent>(output);
    eventMgr.trigger(event);
    output = event->getFogColor();
}
void applyFog(MCCColor& output) {
    __try { editFog(output); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
}

MCCColor& OverworldFogColorHook::OverworldFogColorCallback(Dimension* _this, MCCColor& result,
    MCCColor const& baseColor, float brightness)
{

    auto& output = funcOriginal(_this, result, baseColor, brightness);
    applyFog(output);
    return output;
}

OverworldFogColorHook::OverworldFogColorHook(): Hook("Overworld Fog Color Hook", GET_SIG_ADDRESS("OverworldDimension::getBrightnessDependentFogColor"))
{}

void OverworldFogColorHook::enableHook()
{

    this->autoHook((void *) OverworldFogColorCallback, (void **) &funcOriginal);

}
