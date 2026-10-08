// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/ItemPhysics/ItemPhysics.hpp: on 1.26.52 the item render hooks hand over the item, its
// matrix and its resting place, see ItemPhysics.cpp.
#pragma once

#include <chrono>
#include <mutex>
#include <unordered_map>

#include "../Module.hpp"
#include "../../../../SDK/Client/Render/ActorRenderData.hpp"

// the dropped item ItemRenderer::render is drawing on this thread; ActorRenderData::extraData of the item events points at it
struct DroppedItem {
    Actor *actor = nullptr;
    glm::mat4 *matrix = nullptr;
    // where the item lies in the matrix stack's space, before the game bobs and spins it
    glm::vec3 ground{};
    // where the matrix stood when the game began to draw the copies
    glm::vec3 group{};
    float worldY = 0.f;
    int copy = 0;
    bool flat = true;
    bool drawing = false;
};

class ItemPhysics : public Module {
private:
    struct State {
        glm::vec3 rotation{};
        float spin = 1.f;
        float height = 0.f;
        bool resting = false;
        std::chrono::steady_clock::time_point moved, seen;
    };

    // keyed by the actor's address, which is only compared and never read; entries of items that are gone are dropped.
    // The game draws on its own thread while the module is switched from another, hence the lock
    std::unordered_map<Actor *, State> items;
    std::mutex lock;
    std::chrono::steady_clock::time_point swept;

    void advance(const DroppedItem &item);

public:
    ItemPhysics() : Module("Item Physics", "Changes rotation behavior of dropped items",
        IDR_ITEM_PHYSICS_PNG, "", false, {"animation"}) {}

    void onEnable() override;
    void onDisable() override;
    void defaultConfig() override;
    void settingsRender(float settingsOffset) override;

    void onItemRenderer(ItemRendererEvent &event);
};
