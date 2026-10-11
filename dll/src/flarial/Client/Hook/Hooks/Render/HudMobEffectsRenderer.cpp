// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/HudMobEffectsRenderer.cpp: forwards the four arguments 1.26.52 passes.
#include "HudMobEffectsRenderer.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"

HudMobEffectsRendererHook::HudMobEffectsRendererHook() : Hook("HudMobEffectsRenderer_render", GET_SIG_ADDRESS("HudMobEffectsRenderer::render")) {}

void HudMobEffectsRendererHook::enableHook() {
    this->autoHook((void *) HudMobEffectsRenderer_renderCallback, (void **) &funcOriginal);
}

void *HudMobEffectsRendererHook::HudMobEffectsRenderer_renderCallback(struct HudMobEffectsRenderer *_this,
                                                                      struct IClientInstance *client,
                                                                      struct MinecraftUIRenderContext *renderContext,
                                                                      struct UIControl *owner) {
    auto event = nes::make_holder<RenderPotionHUDEvent>();
    eventMgr.trigger(event);

    if(event->isCancelled()) return nullptr;

    return funcOriginal(_this, client, renderContext, owner);
}
