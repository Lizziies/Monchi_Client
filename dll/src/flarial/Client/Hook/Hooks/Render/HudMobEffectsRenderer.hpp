// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/HudMobEffectsRenderer.hpp: 1.26.52 passes (this, client, context, owner).
#pragma once

#include <array>
#include "../Hook.hpp"

class HudMobEffectsRendererHook : public Hook {
private:
    static void *HudMobEffectsRenderer_renderCallback(class HudMobEffectsRenderer *_this, class IClientInstance *client, class MinecraftUIRenderContext *renderContext, class UIControl *owner);

public:
    typedef void *(__thiscall *original)(class HudMobEffectsRenderer *_this, class IClientInstance *client, class MinecraftUIRenderContext *renderContext, class UIControl *owner);

    static inline original funcOriginal = nullptr;

    HudMobEffectsRendererHook();

    void enableHook() override;
};
