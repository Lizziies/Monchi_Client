// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/RenderLevelHook.cpp: Monchi's cosmetics are drawn here as well.
#include "RenderLevelHook.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"

#include "Bridge/WorldMesh.hpp"

RenderLevelHook::RenderLevelHook() : Hook("RenderLevelHook", GET_SIG_ADDRESS("LevelRenderer::renderLevel")) {}

void RenderLevelHook::enableHook() {
    this->autoHook((void *) RenderLevelCallback, (void **) &funcOriginal);
}

void RenderLevelHook::RenderLevelCallback(LevelRender* level, ScreenContext* scn, void* a3) {
    auto event = nes::make_holder<Render3DEvent>(level, scn);
    eventMgr.trigger(event);
    monchiWorld::draw(level, scn, false);
    funcOriginal(level, scn, a3);
    // after the level: the material tests depth but writes none, so drawn first the cosmetics lose to everything the
    // level draws afterwards, the wearer's own body included
    monchiWorld::draw(level, scn, true);
}
