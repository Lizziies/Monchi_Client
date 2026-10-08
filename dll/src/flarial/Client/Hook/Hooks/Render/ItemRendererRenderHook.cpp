// SPDX-License-Identifier: AGPL-3.0-only
#include "ItemRendererRenderHook.hpp"
#include "../../../../SDK/Client/Render/MatrixStack.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include "../../../Module/Manager.hpp"

namespace {

// static, ItemRenderer::render (0x55aab75): the actor points at its state vector, whose first three floats are the position
float heightAt(Actor *actor, int offset) {
    __try {
        auto state = *reinterpret_cast<uintptr_t *>(reinterpret_cast<uintptr_t>(actor) + offset);
        return state ? reinterpret_cast<float *>(state)[1] : 0.f;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0.f;
    }
}

float worldY(Actor *actor) {
    static int offset = GET_OFFSET("Actor::stateVector");
    return offset ? heightAt(actor, offset) : 0.f;
}

bool wanted() {
    static std::weak_ptr<Module> physics;
    auto module = physics.lock();
    if (!module) physics = module = ModuleManager::getModule("Item Physics");
    return module && module->isEnabled();
}

}

// static (0x55aaa60, slot 2 of the renderer's vtable): the third argument is the ActorRenderData, the actor at +0 and the
// camera-relative position at +0x10; the function pushes a matrix, moves there, then bobs and spins
void ItemRendererRenderHook::callback(void *self, void *context, ActorRenderData *data) {
    auto *stack = data && data->actor && wanted() ? MatrixStack::ofContext(context) : nullptr;
    if (!stack) return funcOriginal(self, context, data);

    item = {};
    item.actor = data->actor;
    item.ground = glm::vec3(stack->top().matrix * glm::vec4(data->position.x, data->position.y, data->position.z, 1.f));
    item.worldY = worldY(data->actor);
    funcOriginal(self, context, data);
    item = {};
}

ItemRendererRenderHook::ItemRendererRenderHook() : Hook("ItemRenderer render", GET_SIG_ADDRESS("ItemRenderer::render")) {}

void ItemRendererRenderHook::enableHook() {
    this->autoHook((void *) callback, (void **) &funcOriginal);
}
