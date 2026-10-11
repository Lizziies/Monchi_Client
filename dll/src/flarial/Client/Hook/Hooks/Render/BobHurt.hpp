// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/BobHurt.hpp: 1.26.52 has no bobHurt(this, matrix). The nearest thing is
// the camera bob system (0x6730a00), fn(camera, key, view) with three arguments, which adds its offset to the camera
// position at +0x40/+0x44/+0x48 (x sideways, y up, z forward, measured from its own stores). The hook runs it and then
// lets the listeners translate an identity matrix, whose translation is added to that offset.
#pragma once

#include <array>
#include "../Hook.hpp"

class BobHurtHook : public Hook {
private:
    static void callback(void* camera, void* key, void* view);

public:
    typedef void(__fastcall *original)(void* camera, void* key, void* view);

    static inline original funcOriginal = nullptr;

    BobHurtHook();

    void enableHook() override;
};
