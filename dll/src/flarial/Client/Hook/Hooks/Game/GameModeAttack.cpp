// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Game/GameModeAttack.cpp: the target argument is only reported when its vtable
// is an Actor one (every Actor-family vtable shares the baseTick slot), so a wrong guess about the arguments costs the
// event, not the game.
#include "GameModeAttack.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include "Events/Game/AttackEvent.hpp"
#include "Bridge/AttackQueue.hpp"

namespace {

int tickSlot() {
    static const int slot = GET_OFFSET("Actor::baseTickVft");
    return slot;
}

uintptr_t baseTick() {
    static uintptr_t expected = [] {
        auto base = GET_SIG_ADDRESS("Actor::vtable");
        if (!base) return uintptr_t(0);
        int offset = *reinterpret_cast<int *>(base + 3);
        auto **vft = reinterpret_cast<uintptr_t **>(base + offset + 7);
        return reinterpret_cast<uintptr_t>(vft[tickSlot()]);
    }();
    return expected;
}

bool isActor(void *p) {
    auto expected = baseTick();
    if (!expected || !p) return false;
    __try {
        auto *vft = *reinterpret_cast<uintptr_t **>(p);
        return vft && vft[tickSlot()] == expected;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}

uint64_t GameModeAttackHook::callback(Gamemode *gamemode, Actor *actor, uint64_t a3, uint64_t a4) {
    if (SDK::clientInstance && SDK::clientInstance->getLocalPlayer() != nullptr && isActor(actor)) {
        if (SDK::clientInstance->getLocalPlayer() == gamemode->getPlayer()) {
            monchiAttack::push(reinterpret_cast<uintptr_t>(actor));
            auto event = nes::make_holder<AttackEvent>(actor);
            eventMgr.trigger(event);
        }
    }
    return ((original)funcOriginal)(gamemode, actor, a3, a4);
}

GameModeAttackHook::GameModeAttackHook() : Hook("GameModeAttack", 0) {}

void GameModeAttackHook::enableHook() {
    static auto addr = GET_SIG_ADDRESS("GameMode::attack");
    this->manualHook((void *) addr, (void *) callback, (void **) &funcOriginal);
}
