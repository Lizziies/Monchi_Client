// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial getFovHook.cpp: Monchi zoom scales the rendered view without changing game options.
#include "getFovHook.hpp"
#include "Bridge/CameraRequests.hpp"
#include <cmath>

float getFovHook::getFovCallback(void* a1, float f, void* a3, void* a4)
{

    float fov = funcOriginal(a1, f, a3, a4);

    auto event = nes::make_holder<FOVEvent>(fov);
    eventMgr.trigger(event);

    float result = event->getFOV();
    float zoom = monchiCamera::zoom.load();
    return zoom > 1.001f ? 2.f * std::atan(std::tan(result * 0.00872664626f) / zoom) * 57.2957795f : result;
}

getFovHook::getFovHook(): Hook("getFovHook", GET_SIG_ADDRESS("LevelRendererPlayer::getFov"))
{}

void getFovHook::enableHook()
{
    this->autoHook((void *) getFovCallback, (void **) &funcOriginal);
}
