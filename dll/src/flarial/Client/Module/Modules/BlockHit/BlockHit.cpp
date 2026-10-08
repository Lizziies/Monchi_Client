// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/BlockHit/BlockHit.cpp: the pose is the same, but the hand eases into it and
// out of it within a few frames instead of jumping, and the first time it shows is logged.
#include "BlockHit.hpp"

#include <algorithm>
#include <cmath>

#include "glm/glm/ext/matrix_transform.hpp"

void BlockHit::onEnable() {
    Module::onEnable();
    Listen(this, RenderItemInHandEvent, &BlockHit::onItemInHandRender);
    Listen(this, PerspectiveEvent, &BlockHit::onPerspectiveChange);
}

void BlockHit::onDisable() {
    Deafen(this, RenderItemInHandEvent, &BlockHit::onItemInHandRender);
    Deafen(this, PerspectiveEvent, &BlockHit::onPerspectiveChange);
    amount = 0.f;
    Module::onDisable();
}

void BlockHit::onItemInHandRender(RenderItemInHandEvent &event) {
    if (!this->isEnabled()) return;
    auto itemStack = event.itemStack;
    bool sword = itemStack && itemStack->mItem.get() != nullptr && itemStack->getItem()->name.contains("sword");

    auto now = std::chrono::steady_clock::now();
    float delta = std::clamp(std::chrono::duration<float>(now - seen).count(), 0.f, 0.1f);
    seen = now;
    float target = sword && MC::heldRight ? 1.f : 0.f;
    amount += (target - amount) * (1.f - std::exp(-30.f * delta));
    if (std::abs(target - amount) < 0.002f) amount = target;
    if (amount <= 0.f) return;

    static bool told = false;
    if (!told) {
        told = true;
        Logger::info("block hit: the held sword goes into the blocking pose");
    }

    auto& matrix = SDK::clientInstance->getCamera().getWorldMatrixStack().top().matrix;
    float a = amount;

    switch (perspective) {
        case Perspective::FirstPerson:

            matrix = glm::translate<float>(matrix, glm::vec3(-0.5f, 0.2f, 0.0f) * a);
            matrix = glm::rotate<float>(matrix, glm::radians(30.f * a), glm::vec3(0.f, 1.f, 0.f));
            matrix = glm::rotate<float>(matrix, glm::radians(-80.f * a), glm::vec3(1.f, 0.f, 0.f));
            matrix = glm::rotate<float>(matrix, glm::radians(60.f * a), glm::vec3(0.f, 1.f, 0.f));

            break;

        case Perspective::ThirdPersonBack:
        case Perspective::ThirdPersonFront:

            matrix = glm::translate<float>(matrix, glm::vec3(-0.5f, 0.2f, 0.2f) * a);
            matrix = glm::rotate<float>(matrix, glm::radians(105.f * a), glm::vec3(0.f, 1.f, 0.f));
            matrix = glm::rotate<float>(matrix, glm::radians(-100.f * a), glm::vec3(1.f, 0.f, 0.f));
            matrix = glm::rotate<float>(matrix, glm::radians(130.f * a), glm::vec3(0.f, 1.f, 0.f));

            break;
    }
}

void BlockHit::defaultConfig() {
    Module::defaultConfig("core");

}

void BlockHit::onPerspectiveChange(PerspectiveEvent &event)  {
    if (!this->isEnabled()) return;
    this->perspective = event.getPerspective();
}
