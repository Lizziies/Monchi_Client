// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/HudCursorRenderer.cpp: see the header for the 1.26.52 argument order.
#include "HudCursorRenderer.hpp"
#include "Bridge/CameraRequests.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"

HudCursorRendererHook::HudCursorRendererHook() : Hook("HudCursorRenderer_render", GET_SIG_ADDRESS("HudCursorRenderer::render")) {}

void HudCursorRendererHook::enableHook() {
    this->autoHook((void *) HudCursorRenderer_renderCallback, (void **) &funcOriginal);
}

void* HudCursorRendererHook::HudCursorRenderer_renderCallback(struct HudCursorRenderer *_this,
                                                              struct IClientInstance *client,
                                                              struct MinecraftUIRenderContext *renderContext,
                                                              struct UIControl *owner) {
    if (monchiCamera::hideCrosshair.load()) return nullptr;
    // the crosshair control's cached position (+0x10) and size (+0x48); the game no longer hands over an area
    RectangleArea area{0.f, 0.f, 0.f, 0.f};
    if (owner) {
        auto *f = reinterpret_cast<float *>(owner);
        area = RectangleArea{f[4], f[4] + f[18], f[5], f[5] + f[19]};
    }
    auto event = nes::make_holder<HudCursorRendererRenderEvent>(&area, renderContext);
    eventMgr.trigger(event);

    if(event->isCancelled()) return nullptr;

    return funcOriginal(_this, client, renderContext, owner);
}
