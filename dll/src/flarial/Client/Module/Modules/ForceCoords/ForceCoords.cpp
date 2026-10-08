// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/ForceCoords/ForceCoords.cpp: patch and unpatch use the checked site of ForceCoords.hpp.
#include "ForceCoords.hpp"

void ForceCoords::onEnable() {
    Listen(this, RenderEvent, &ForceCoords::onRender)
    Module::onEnable();
}

void ForceCoords::onDisable() {
    Deafen(this, RenderEvent, &ForceCoords::onRender)
    unpatch();
    Module::onDisable();
}

void ForceCoords::patch() {
    if (patched || !site.armed()) return;
    patched = codepatch::engage(site);
}

void ForceCoords::unpatch() {
    if (!patched) return;
    patched = !codepatch::release(site);
}

void ForceCoords::defaultConfig() {
    Module::defaultConfig("all");
    setDef("MojangStyle", false);
    setDef("textscale", 1.00f);
    
}

void ForceCoords::settingsRender(float settingsOffset) {
    return;
}

void ForceCoords::onRender(RenderEvent &event) {


    if (ClientInstance::getTopScreenName() == "hud_screen") {

        if (SDK::hasInstanced && SDK::clientInstance != nullptr) {

            if (SDK::clientInstance->getLocalPlayer() != nullptr) {

                if (getOps<bool>("MojangStyle") && !mojanged) {
                    patch();
                    mojanged = true;
                }
                else if (!getOps<bool>("MojangStyle")) {
                    if (mojanged) {
                        unpatch();
                        mojanged = false;
                    }

                    Vec3<float> Pos = SDK::clientInstance->getLocalPlayer()->getAABBShapeComponent()->aabb.lower;
                    //Vec3<float> PrevPos = SDK::clientInstance->getLocalPlayer()->stateVector->PrevPos;
                    //Vec3<float> vel = SDK::clientInstance->getLocalPlayer()->stateVector->velocity;

                    std::string cords = FlarialGUI::cached_to_string(static_cast<int>(Pos.x)) + ", " +
                                        FlarialGUI::cached_to_string(static_cast<int>(Pos.y)) + ", " +
                                        FlarialGUI::cached_to_string(static_cast<int>(Pos.z));
                    //std::string cords1 = FlarialGUI::cached_to_string(static_cast<int>(PrevPos.x)) + ", " + FlarialGUI::cached_to_string(static_cast<int>(PrevPos.y)) + ", " + FlarialGUI::cached_to_string(static_cast<int>(PrevPos.z));
                    //std::string cords2 = FlarialGUI::cached_to_string(static_cast<int>(vel.x)) + ", " + FlarialGUI::cached_to_string(static_cast<int>(vel.y)) + ", " + FlarialGUI::cached_to_string(static_cast<int>(vel.z));
                    this->normalRender(6, cords);
                }
            }
        }
    }
    else if (mojanged) {
        unpatch();
        mojanged = false;
    }
}
