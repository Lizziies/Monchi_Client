// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/NametagModifier/NametagModifier.hpp: the patch covers exactly the one
// instruction at ThirdPersonNametag (a two byte je on 1.26.52, six bytes before).
#pragma once

#include "../Module.hpp"
#include "Events/Game/PerspectiveEvent.hpp"
#include "Events/Render/DrawNameTagEvent.hpp"
#include "Utils/Memory/PatchSite.hpp"


class NametagModifier : public Module {

private:

    static inline codepatch::Site site;
    static inline bool patched = false;

public:

    NametagModifier(): Module("Nametag", "Shows your nametag for you while\nin 3rd person mode.",
                IDR_NAMETAG_PNG, "", false, {"third person nametag", "3rd person nametag"}) {
        uintptr_t at = GET_SIG_ADDRESS("ThirdPersonNametag");
        uint8_t head[2]{};
        if (at && codepatch::readBytes(at, head, 2)) {
            // 1.26.52: "je" (74 rel8) that skips the local player in the nametag loop of 0x46b2fd0
            if (head[0] == 0x74) codepatch::armNop(site, at, {head[0], head[1]});
            else if (head[0] == 0x0F && head[1] == 0x84) {
                uint8_t full[6]{};
                if (codepatch::readBytes(at, full, 6)) codepatch::armNop(site, at, {full[0], full[1], full[2], full[3], full[4], full[5]});
            }
        }
        if (!site.armed()) Logger::warn("Nametag: ThirdPersonNametag is missing or not a je, own nametag stays hidden");
    }

    void defaultConfig() override;

    void onEnable() override;

    void onDisable() override;

    static void patch();

    static void unpatch();

    void settingsRender(float settingsOffset) override;

    void onGetViewPerspective(PerspectiveEvent& event);

    void onDrawNameTag(DrawNameTagEvent& event);
};
