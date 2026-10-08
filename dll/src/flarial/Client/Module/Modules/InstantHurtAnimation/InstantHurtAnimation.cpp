// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/InstantHurtAnimation/InstantHurtAnimation.cpp. Upstream feeds the client a made-up hurt
// packet and swallows the server's real one; that needs runtime ids from the entity registry (not readable on 1.26.52,
// the module faulted there) and a packet layout that is not confirmed. The game itself starts the animation with two
// writes (static: Mob's hurt handlers at 0x207a0b8 and 0x207e905 set the hurt time at +0x19c and the counter behind
// +0x428 to ten), so the attack does the same on the target. Nothing is sent and nothing is swallowed: the server's own
// hurt event only sets the same ten again.
#include "InstantHurtAnimation.hpp"

#include "Client.hpp"

namespace {

// Only for a mob that is not flashing already: a hit during the half second after the last one does no damage, and
// the animation should not claim otherwise. Other things one can hit (boats, crystals, frames) have no such counter.
bool start(Actor *actor, bool playersOnly, int categories, int time, int counter) {
    __try {
        auto base = reinterpret_cast<uintptr_t>(actor);
        int kinds = *reinterpret_cast<int *>(base + categories);
        if (!(kinds & int(ActorCategory::Mob)) || (playersOnly && !(kinds & int(ActorCategory::Player)))) return false;
        int *hurtTime = reinterpret_cast<int *>(base + time);
        int *left = *reinterpret_cast<int **>(base + counter);
        if (!left || *hurtTime < 0 || *hurtTime > 10 || *left < 0 || *left > 10 || *hurtTime > 0) return false;
        *hurtTime = 10;
        *left = 10;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}

void InstantHurtAnimation::onEnable() {
    Listen(this, AttackEvent, &InstantHurtAnimation::onAttack)
    Module::onEnable();
}

void InstantHurtAnimation::onDisable() {
    Deafen(this, AttackEvent, &InstantHurtAnimation::onAttack)
    Module::onDisable();
}

void InstantHurtAnimation::defaultConfig() {
    Module::defaultConfig("core");
    setDef("playersOnly", true);
}

void InstantHurtAnimation::settingsRender(float settingsOffset) {
    initSettingsPage();

    addToggle("Only for players", "", "playersOnly");

    FlarialGUI::UnsetScrollView();

    resetPadding();
}

void InstantHurtAnimation::onAttack(AttackEvent &event) {
    if (!this->isEnabled() || !event.getActor()) return;
    static const int categories = GET_OFFSET("Actor::categories"), time = GET_OFFSET("Actor::hurtTime"),
                     counter = GET_OFFSET("Mob::hurtCounter");
    if (!categories || !time || !counter) return;
    if (!start(event.getActor(), getOps<bool>("playersOnly"), categories, time, counter)) return;
    static bool told = false;
    if (told) return;
    told = true;
    Logger::info("insta hurt animation: started the hurt animation on the target of an attack");
}
