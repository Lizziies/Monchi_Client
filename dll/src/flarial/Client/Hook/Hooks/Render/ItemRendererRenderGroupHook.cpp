// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/ItemRendererRenderGroupHook.cpp: only acts for the dropped item ItemRenderer::render is
// drawing, takes the matrix from the render context instead of the client instance camera, and leaves the matrix stack alone
// because the game pops the changed matrix itself.
#include "ItemRendererRenderGroupHook.hpp"
#include "ItemRendererRenderHook.hpp"
#include "../../../../SDK/Client/Render/MatrixStack.hpp"
#include "../../../Events/EventManager.hpp"
#include "../../../Events/Render/ItemRendererEvent.hpp"

// static (0x55aa4c0): draws the copies of a stack, each under its own pushed matrix, through ItemInHandRenderer::renderItem.
// The flat path of ItemRenderer::render passes 0.3 as the size (0x55ab167), the block path 0.25 or 0.5 (0x55aa349).
void ItemRendererRenderGroupHook::ItemRendererCallback(ItemRenderer* _this, BaseActorRenderContext* ctx, void* itemActor, int amount, float a5, float a6, bool a7) {
    auto &item = ItemRendererRenderHook::item;
    auto *stack = item.actor && item.actor == itemActor ? MatrixStack::ofContext(ctx) : nullptr;
    if (!stack) return funcOriginal(_this, ctx, itemActor, amount, a5, a6, a7);

    item.matrix = &stack->top().matrix;
    item.group = glm::vec3((*item.matrix)[3]);
    item.flat = a5 == 0.3f;
    item.copy = 0;

    ActorRenderData data{};
    data.actor = item.actor;
    data.extraData = &item;
    auto event = nes::make_holder<ItemRendererEvent>(&data, true);
    eventMgr.trigger(event);
    stack->isDirty = true;

    item.matrix = nullptr;
    item.drawing = true;
    funcOriginal(_this, ctx, itemActor, amount, a5, a6, a7);
    item.drawing = false;
}

ItemRendererRenderGroupHook::ItemRendererRenderGroupHook() : Hook("ItemRenderer Hook", GET_SIG_ADDRESS("ItemRenderer::renderItemGroup")) {}

void ItemRendererRenderGroupHook::enableHook() {
    this->autoHook((void*)ItemRendererCallback, (void**)&funcOriginal);
}
