// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/NoHurtCam/NoHurtCam.cpp: patch and unpatch use the checked site of NoHurtCam.hpp.
#include "NoHurtCam.hpp"




void NoHurtCam::onEnable() {
    Listen(this, RaknetTickEvent, &NoHurtCam::onRaknetTick)
    Listen(this, TickEvent, &NoHurtCam::onTick)
    Module::onEnable();
}

void NoHurtCam::onDisable() {
    if (patched) unpatch();
    Deafen(this, RaknetTickEvent, &NoHurtCam::onRaknetTick)
    Deafen(this, TickEvent, &NoHurtCam::onTick)
    Module::onDisable();
}

void NoHurtCam::defaultConfig() {
    Module::defaultConfig("core");
}

void NoHurtCam::patch() {
    if (patched || !site.armed()) return;
    patched = codepatch::engage(site);
}

void NoHurtCam::unpatch() {
    if (!patched) return;
    patched = !codepatch::release(site);
}

void NoHurtCam::onRaknetTick(RaknetTickEvent &event) {
    if (this->isEnabled()) {
        std::string serverIP = SDK::getServerIP();
        if (serverIP.find("hive") != std::string::npos or serverIP.find("139.99.38.170") != std::string::npos) {
            if (!this->restricted) {
                FlarialGUI::Notify("Can't use No Hurt Cam on " + serverIP); // TODO: move restrictions to API
                this->restricted = true;
            }
        } else {
            this->restricted = false;
        }
    }
}

void NoHurtCam::onTick(TickEvent &event) {
    if (!this->restricted) {
        patch();
    } else {
        unpatch();
    }
}
