// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Game/SoundEnginePlayHook.hpp: SoundEngine::play takes (this, params) on 1.26.52,
// the name, position, volume and pitch are fields of one by-value struct.
#pragma once
#include "../Hook.hpp"

class SoundEnginePlayHook : public Hook {

private:
    static uint64_t callback(void* SoundEngine, void* params);

public:
    typedef uint64_t(__thiscall *original)(void* SoundEngine, void* params);

    static inline original funcOriginal = nullptr;

    SoundEnginePlayHook();

    void enableHook() override;
};
