// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/Freelook/Freelook.cpp: patch and unpatch walk the site table of Freelook.hpp.
#include "Freelook.hpp"



void FreeLook::onSetup() {
    keybindActions.clear();

    //enable action (key press)
    keybindActions.push_back([this](std::vector<std::any> args) -> std::any {

        std::string serverIP = SDK::getServerIP();
        if ((serverIP.find("hive") != std::string::npos ||
             serverIP.find("galaxite") != std::string::npos ||
             serverIP.find("venity") != std::string::npos ||
             serverIP.find("139.99.38.170") != std::string::npos)) { // TODO: make it only show once per server switch?
            FlarialGUI::Notify("Can't use freelook on " + serverIP); // TODO: move restrictions to API
            this->restricted = true;
        }
        else {
            this->restricted = false;
        }
        if (!this->restricted) {
            if (getOps<bool>("toggle")) {
                if (!this->active) {
                    patch();
                }
                else {
                    unpatch();
                }
            }
            else {
                patch();
            }
        }
        else {
            unpatch(); // module restricted
        }
        return {};


    });


    //disable action (key release)
    keybindActions.push_back([this](std::vector<std::any> args) -> std::any {

        if (!getOps<bool>("toggle"))
            unpatch();

        return {};

    });
}

void FreeLook::onEnable() {
    ListenOrdered(this, PerspectiveEvent, &FreeLook::onGetViewPerspective, EventOrder::IMMEDIATE)
    Listen(this, UpdatePlayerEvent, &FreeLook::onUpdatePlayer)
    Listen(this, KeyEvent, &FreeLook::onKey)
    Listen(this, MouseEvent, &FreeLook::onMouse)
    Module::onEnable();

}

void FreeLook::onDisable() {
    this->active = false;
    apply();
    Deafen(this, PerspectiveEvent, &FreeLook::onGetViewPerspective)
    Deafen(this, UpdatePlayerEvent, &FreeLook::onUpdatePlayer)
    Deafen(this, KeyEvent, &FreeLook::onKey)
    Deafen(this, MouseEvent, &FreeLook::onMouse)
    Module::onDisable();
}

// The key arrives on the window thread, and the patched instructions are the camera stores the game runs on every
// mouse move. So the key only states what is wanted; the bytes are changed by apply() on the game's own thread,
// from the listeners the game calls every frame.
void FreeLook::patch() { this->active = true; }

void FreeLook::unpatch() { this->active = false; }

void FreeLook::apply() {
    bool want = this->active;
    if (want == engaged) return;
    engaged = want;
    if (want) {
        bool headFollows = getOps<bool>("headFollows");
        for (size_t i = 0; i < sites.size(); i++)
            if (sites[i].armed() && !(headFollows && headGroup(i))) codepatch::engage(sites[i]);
    } else {
        for (auto& site : sites)
            if (site.armed()) codepatch::release(site);
    }
}

void FreeLook::defaultConfig() {
    getKeybind();
    Module::defaultConfig("core");
    setDef("toggle", true);
    setDef("mode", (std::string)"3rd Person back");
    setDef("headFollows", false);
    
}

void FreeLook::settingsRender(float settingsOffset) {

    initSettingsPage();

    addKeybind("Freelook Keybind", "", "keybind", true);
    addToggle("Toggleable Mode", "Click to toggle or Hold to keep enabled", "toggle");
    addDropdown("Freelook View Mode", "",std::vector<std::string>{"1st Person", "3rd Person back", "3rd Person front"}, "mode", true);
    addToggle("Head follows the camera", "Off keeps your character's head still while you look around", "headFollows");

    FlarialGUI::UnsetScrollView();

    resetPadding();
}

void FreeLook::onKey(KeyEvent &event) {
    if (!this->isEnabled()) return;
    if (this->isKeyPartOfKeybind(event.key) && (SDK::getCurrentScreen() == "hud_screen" || SDK::getCurrentScreen() == "f3_screen" || SDK::getCurrentScreen() == "zoom_screen")) {
        if (this->isKeybind(event.keys)) { // key is defo pressed
            keybindActions[0]({});
        }
        else { // key released
            keybindActions[1]({});
        }
    }

}

void FreeLook::onMouse(MouseEvent &event) {
    if (!this->isEnabled()) return;
    if (Utils::getMouseAsString(event.getButton()) == getOps<std::string>("keybind") && event.getAction() == MouseAction::Press) keybindActions[0]({});
    else if (Utils::getMouseAsString(event.getButton()) == getOps<std::string>("keybind") && event.getAction() == MouseAction::Release) keybindActions[1]({});
}

void FreeLook::onUpdatePlayer(UpdatePlayerEvent& event) {
    apply();
    if (this->active) {
        event.cancel();
    }
}

void FreeLook::onGetViewPerspective(PerspectiveEvent &event) {
    apply();
    if (!this->isEnabled()) return;
    if (this->active) {
        std::string setting = getOps<std::string>("mode");
        // TODO: Let use F5 (perspective switch key)
        if (setting == "1st Person") {
            event.setPerspective(Perspective::FirstPerson);
        }
        if (setting == "3rd Person back") {
            event.setPerspective(Perspective::ThirdPersonBack);
        }
        if (setting == "3rd Person front") {
            event.setPerspective(Perspective::ThirdPersonFront);
        }
    }
}
