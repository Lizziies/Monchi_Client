// SPDX-License-Identifier: AGPL-3.0-only
// ItemRenderer::render, the renderer of dropped items: remembers which item is being drawn and where it lies before the
// game bobs and spins it, for the group and item hooks further down the same call.
#pragma once

#include "../Hook.hpp"
#include "../../../Module/Modules/ItemPhysics/ItemPhysics.hpp"

class ItemRendererRenderHook : public Hook {
private:
    static void callback(void *self, void *context, ActorRenderData *data);

public:
    typedef void (__fastcall *original)(void *self, void *context, ActorRenderData *data);

    static inline original funcOriginal = nullptr;

    static inline thread_local DroppedItem item;

    ItemRendererRenderHook();

    void enableHook() override;
};
