// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Game/ItemInHandRendererRenderItem.hpp, see the header.
#include "ItemInHandRendererRenderItem.hpp"
#include "../../../../Utils/Memory/Memory.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include "../../../../SDK/Client/Render/Camera.hpp"
#include "../Render/ItemRendererRenderHook.hpp"
#include "Events/Game/RenderItemInHandEvent.hpp"
#include "Events/Render/ItemRendererEvent.hpp"
#include "Module/Manager.hpp"
#include "ScopeCleanup.hpp"

namespace {

uintptr_t target() {
    auto addr = GET_SIG_ADDRESS("ItemInHandRenderer::renderItem");
    if (addr && *reinterpret_cast<uint8_t *>(addr) == 0xE8) return Memory::offsetFromSig(addr, 1);
    return addr;
}


}

ItemInHandRendererRenderItem::ItemInHandRendererRenderItem() : Hook("ItemInHandRendererRenderItem", target()) {}

void ItemInHandRendererRenderItem::enableHook() {
    this->autoHook((void *) callback, (void **) &funcOriginal);
}

void *ItemInHandRendererRenderItem::callback(void *a1, void *context, Actor *entity, ItemStack *itemStack, bool a5, bool a6, bool a7, bool a8) {
    // a copy of a dropped item: the game has pushed a matrix for it and pops it after this call
    auto &dropped = ItemRendererRenderHook::item;
    if (dropped.drawing && entity == dropped.actor) {
        if (auto *stack = MatrixStack::ofContext(context)) {
            dropped.matrix = &stack->top().matrix;
            ActorRenderData data{};
            data.actor = entity;
            data.extraData = &dropped;
            auto event = nes::make_holder<ItemRendererEvent>(&data);
            eventMgr.trigger(event);
            stack->isDirty = true;
            dropped.matrix = nullptr;
            dropped.copy++;
        }
        return funcOriginal(a1, context, entity, itemStack, a5, a6, a7, a8);
    }

    bool transforms = false;
    for (const char *name : {"View Model", "Block Hit", "Swing Animations", "Java View Bobbing"}) {
        auto module = ModuleManager::getModule(name);
        if (module && module->isEnabled()) transforms = true;
    }
    if (!transforms) return funcOriginal(a1, context, entity, itemStack, a5, a6, a7, a8);
    auto *player = SDK::clientInstance ? SDK::clientInstance->getLocalPlayer() : nullptr;
    if (!player || static_cast<Actor *>(player) != entity || !itemStack) return funcOriginal(a1, context, entity, itemStack, a5, a6, a7, a8);

    auto *stack = MatrixStack::ofContext(context);
    if (!stack) return funcOriginal(a1, context, entity, itemStack, a5, a6, a7, a8);

    struct Call {
        MatrixStack* stack;
        MatrixStack* previous;
        void *a1, *context;
        Actor* entity;
        ItemStack* item;
        bool a5, a6, a7, a8;
        void* result = nullptr;
        bool pushed = false;
    } call{stack, mce::Camera::activeWorld, a1, context, entity, itemStack, a5, a6, a7, a8};
    guard::withCleanup([](void* value) {
        auto& c = *static_cast<Call*>(value);
        c.stack->push();
        c.pushed = true;
        mce::Camera::activeWorld = c.stack;
        auto event = nes::make_holder<RenderItemInHandEvent>(c.item);
        eventMgr.trigger(event);
        c.result = funcOriginal(c.a1, c.context, c.entity, c.item, c.a5, c.a6, c.a7, c.a8);
    }, [](void* value) {
        auto& c = *static_cast<Call*>(value);
        mce::Camera::activeWorld = c.previous;
        if (c.pushed) c.stack->pop();
    }, &call);
    return call.result;
}
