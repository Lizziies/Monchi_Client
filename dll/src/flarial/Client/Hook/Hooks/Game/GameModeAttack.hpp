// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Game/GameModeAttack.hpp: GameMode::attack takes four arguments on 1.26.52
// (this, target, r8d, r9) and the callback hands all four on and returns what the game returned.
#pragma once
#include "../Hook.hpp"
#include "../../../../SDK/Client/Actor/Actor.hpp"
#include "../../../../SDK/Client/Actor/Gamemode.hpp"

class GameModeAttackHook : public Hook {

private:
    static uint64_t callback(Gamemode *gamemode, Actor *actor, uint64_t a3, uint64_t a4);

public:
    typedef uint64_t(__thiscall *original)(Gamemode *, Actor *, uint64_t, uint64_t);
    static inline void* funcOriginal = nullptr;

    GameModeAttackHook();

    void enableHook() override;
};
