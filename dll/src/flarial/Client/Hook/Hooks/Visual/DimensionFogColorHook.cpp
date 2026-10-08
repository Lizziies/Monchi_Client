// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/DimensionFogColorHook.cpp: return the game-owned output, never the temporary event color.
#include "DimensionFogColorHook.hpp"

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

MCCColor& DimensionFogColorHook::DimensionFogColorCallback(Dimension* _this, MCCColor& result,
    MCCColor const& baseColor, float brightness)
{

    auto& output = funcOriginal(_this, result, baseColor, brightness);
    applyFog(output);
    return output;
}

DimensionFogColorHook::DimensionFogColorHook(): Hook("Fog Color Hook", GET_SIG_ADDRESS("Dimension::getBrightnessDependentFogColor"))
{}

void DimensionFogColorHook::enableHook()
{

    this->autoHook( (void *) DimensionFogColorCallback, (void **) &funcOriginal);

}
