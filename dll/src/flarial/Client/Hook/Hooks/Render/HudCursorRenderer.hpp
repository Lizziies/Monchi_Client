// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/HudCursorRenderer.hpp: 1.26.52 passes (this, client, context, owner);
// there is no pass number and no area argument (checked in the game image: the third argument takes the virtual draw calls,
// the fourth has the UIControl layout at +0x10, +0x18 and +0x48).
#pragma once

#include <array>
#include "../Hook.hpp"

class HudCursorRendererHook : public Hook {
private:
    static void* HudCursorRenderer_renderCallback(class HudCursorRenderer *_this, class IClientInstance *client, class MinecraftUIRenderContext *renderContext, class UIControl *owner);

public:
    typedef void*(__thiscall *original)(class HudCursorRenderer *_this, class IClientInstance *client, class MinecraftUIRenderContext *renderContext, class UIControl *owner);

    static inline original funcOriginal = nullptr;

    HudCursorRendererHook();

    void enableHook() override;
};
