// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/NoHurtCam/NoHurtCam.hpp: on 1.26.52 the hurt camera is a camera system
// (0x6730520) that rotates the camera orientation only when its parameter block is flagged; the patch turns the
// "flag is not set" branch into an unconditional jump so the system leaves the orientation alone.
#pragma once

#include "../Module.hpp"
#include "Events/Game/RaknetTickEvent.hpp"
#include "Events/Game/TickEvent.hpp"
#include "Utils/Memory/PatchSite.hpp"


class NoHurtCam : public Module {

private:

    static inline bool patched = false;
    static inline codepatch::Site site;

public:

    NoHurtCam(): Module("No Hurt Cam", "Disables hurt camera animation",
        IDR_REACH_PNG, "") {
        if (!codepatch::armJneToJmp(site, GET_SIG_ADDRESS("CameraAssignAngle")))
            Logger::warn("No Hurt Cam: CameraAssignAngle is missing or not a jne, module stays inactive");
    }

    void onEnable() override;

    void onDisable() override;

    void defaultConfig() override;

    static void patch();

    static void unpatch();

    void onRaknetTick(RaknetTickEvent &event);

    void onTick(TickEvent &event);
};
