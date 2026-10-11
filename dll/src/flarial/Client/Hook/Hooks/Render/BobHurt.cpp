// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/BobHurt.cpp: see BobHurt.hpp for what the hooked function is on 1.26.52.
#include "BobHurt.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include <glm/glm/glm.hpp>

namespace {
bool nudge(void* camera, float x, float y, float z) {
    __try {
        auto* offset = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(camera) + 0x40);
        offset[0] += x;
        offset[1] += y;
        offset[2] += z;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
}

BobHurtHook::BobHurtHook() : Hook("BobHurt", GET_SIG_ADDRESS("BobHurt")) {}

void BobHurtHook::enableHook() {
    this->autoHook((void *) callback, (void **) &funcOriginal);
}

void BobHurtHook::callback(void* camera, void* key, void* view) {
    funcOriginal(camera, key, view);

    glm::mat4 matrix(1.0f);
    auto event = nes::make_holder<BobHurtEvent>(&matrix);
    eventMgr.trigger(event);

    if (camera && (matrix[3].x != 0.0f || matrix[3].y != 0.0f || matrix[3].z != 0.0f))
        nudge(camera, matrix[3].x, matrix[3].y, matrix[3].z);
}
