// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/ItemPhysics/ItemPhysics.cpp: on 1.26.52 the item, its matrix and its resting place come
// from the item render hooks instead of the client instance camera and the actor's components, the rotation advances once
// per drawn item instead of once per drawn copy, and items that are gone leave the table.
#include "ItemPhysics.hpp"
#include "ItemPose.hpp"
#include "Client.hpp"
#include "../../../Events/Render/ItemRendererEvent.hpp"
#include <algorithm>
#include <random>

using namespace std::chrono;

void ItemPhysics::onEnable() {
    Listen(this, ItemRendererEvent, &ItemPhysics::onItemRenderer)

    Module::onEnable();
}

void ItemPhysics::onDisable() {
    Deafen(this, ItemRendererEvent, &ItemPhysics::onItemRenderer)

    {
        std::scoped_lock guard(lock);
        items.clear();
    }

    Module::onDisable();
}

void ItemPhysics::defaultConfig() {
    Module::defaultConfig("core");
    setDef("speed", 8.f);
    setDef("yoffset", 0.3f);
    setDef("preserverots", false);
    setDef("smoothrots", true);
}

void ItemPhysics::settingsRender(float settingsOffset) {
    initSettingsPage();

    addSlider("Speed", "", "speed", 15.f, 3.f, false);
    addSlider("Item Y Offset", "Vertical offset for non-block items", "yoffset", 0.5f, -0.5f, false);
    addToggle("Preserve Rotations", "", "preserverots");
    addToggle("Smooth Rotations", "", "smoothrots");

    FlarialGUI::UnsetScrollView();
    resetPadding();
}

void ItemPhysics::onItemRenderer(ItemRendererEvent &event) {
    if (!isEnabled()) return;

    auto *data = event.getRenderData();
    auto *item = data ? static_cast<DroppedItem *>(data->extraData) : nullptr;
    if (!item || !item->actor || !item->matrix) return;

    std::scoped_lock guard(lock);
    if (event.isItemGroup()) {
        itemPose::dropSpin(*item->matrix);
        advance(*item);
        return;
    }

    auto it = items.find(item->actor);
    if (it == items.end()) return;

    bool eased = getOps<bool>("smoothrots");
    auto rotation = it->second.rotation;
    if (it->second.resting && !eased && !getOps<bool>("preserverots")) rotation = itemPose::lying(item->flat, false);

    itemPose::settle(*item->matrix, item->ground, item->group, item->copy);
    itemPose::lay(*item->matrix, rotation, item->flat, getOps<float>("yoffset"));

    static bool told = false;
    if (told) return;
    told = true;
    Logger::info("item physics: the game draws dropped items through the hooks, first one laid ({})", item->flat ? "flat item" : "block");
}

void ItemPhysics::advance(const DroppedItem &item) {
    auto now = steady_clock::now();
    auto [it, fresh] = items.try_emplace(item.actor);
    auto &state = it->second;

    if (fresh) {
        static std::mt19937 gen(static_cast<unsigned>(now.time_since_epoch().count()));
        state.rotation = {90.f, std::uniform_real_distribution<float>(0.f, 360.f)(gen), 0.f};
        state.spin = std::uniform_int_distribution<int>(0, 1)(gen) ? 1.f : -1.f;
        state.height = item.worldY;
        state.moved = state.seen = now;
    }

    float delta = std::clamp(duration<float>(now - state.seen).count(), 0.f, 0.1f);
    state.seen = now;

    // the position changes every tick while the item falls or floats; two ticks without a change and it lies still
    if (item.worldY != state.height) {
        state.height = item.worldY;
        state.moved = now;
    }
    state.resting = now - state.moved > 110ms;

    itemPose::step(state.rotation, state.spin, state.resting, item.flat, getOps<bool>("smoothrots"), getOps<bool>("preserverots"),
                   getOps<float>("speed"), delta);

    if (now - swept < 5s) return;
    swept = now;
    std::erase_if(items, [&](const auto &entry) { return now - entry.second.seen > 5min; });
}
