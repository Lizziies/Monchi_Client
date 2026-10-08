// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Game/ItemInHandRendererRenderItem.hpp: on 1.26.52 the world matrix stack comes
// from the render context argument instead of the client instance; the local player's items raise the hand event and the
// copies of a dropped item the item event, other actors' items none.
#pragma once

#include "../Hook.hpp"
#include "../../../../SDK/Client/Actor/Actor.hpp"
#include "../../../../SDK/Client/Item/ItemStack.hpp"

/* the name misleads: this renders items held by any actor, and the game calls it from eleven places */
class ItemInHandRendererRenderItem : public Hook {
private:
    static void *callback(void *a1, void *context, Actor *entity, ItemStack *itemStack, bool a5, bool a6, bool a7, bool a8);

public:
    typedef void *(__fastcall *original)(void *a1, void *context, Actor *entity, ItemStack *itemStack, bool a5, bool a6, bool a7, bool a8);

    static inline original funcOriginal = nullptr;

    ItemInHandRendererRenderItem();

    void enableHook() override;
};
