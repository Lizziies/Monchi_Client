// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/GuiScale/GuiScale.cpp: on 1.26.52 the scale goes through Minecraft's own option.
#include "GuiScale.hpp"
#include <cmath>
#include "Client.hpp"


#include "Modules/MovableBossbar/MovableBossbar.hpp"
#include "Modules/MovableChat/MovableChat.hpp"
#include "Modules/MovableCoordinates/MovableCoordinates.hpp"
#include "Modules/MovableDayCounter/MovableDayCounter.hpp"
#include "Modules/MovableHotbar/MovableHotbar.hpp"
#include "Modules/MovableScoreboard/MovableScoreboard.hpp"

void GuiScale::onEnable() {
    restored = false;
    settled = false;
    steps = stepDirection = waitFrames = 0;
    stepTarget = 0.f;
    Listen(this, SetupAndRenderEvent, &GuiScale::onSetupAndRender)
    Module::onEnable();
}

void GuiScale::onDisable() {
    Module::onDisable();
    if (option.active()) {
        if (SDK::clientInstance && option.restore(SDK::clientInstance)) relayout();
        Deafen(this, SetupAndRenderEvent, &GuiScale::onSetupAndRender)
        return;
    }
    if (!restored) {
        delayDisable = true;
        return;
    }
    Deafen(this, SetupAndRenderEvent, &GuiScale::onSetupAndRender)
}

void GuiScale::defaultConfig() {
    Module::defaultConfig("core");
    setDef("guiscale", 2.f);
}

void GuiScale::settingsRender(float settingsOffset) {
    initSettingsPage();

    addSlider("UI Scale", "", "guiscale", 4.f, 1.f, false);

    FlarialGUI::UnsetScrollView();

    resetPadding();
}

void GuiScale::onSetupAndRender(SetupAndRenderEvent &event) {
    if (!this->isEnabled() && !delayDisable) return;
    update();
}

// 1.26.52 ignores the forced scale argument of _updateScreenSizeVariables. Minecraft's own GUI scale option is an
// integer step whose range depends on the display, so the module steps it one notch at a time through the notifying
// setter and watches GuiData::GuiScale until it is as close to the target as the game allows. Minecraft then
// lays out every screen itself.
void GuiScale::relayout() {
    auto* client = SDK::clientInstance;
    auto* data = client ? client->getGuiData() : nullptr;
    if (!data) return;
    if (guiScaleOption::refresh(client)) return;
    if (!GET_SIG_ADDRESS("ClientInstance::_updateScreenSizeVariables")) {
        RECT rect{};
        if (Client::window && GetClientRect(Client::window, &rect) && rect.right > 0 && rect.bottom > 0) {
            PostMessageW(Client::window, WM_SIZE, IsZoomed(Client::window) ? SIZE_MAXIMIZED : SIZE_RESTORED,
                MAKELPARAM(rect.right, rect.bottom));
        }
        return;
    }
    auto size = data->ScreenSize;
    Vec2<float> safeZone{0.f, 0.f};
    client->_updateScreenSizeVariables(&size, &safeZone, data->GuiScale);
}

bool GuiScale::stepOption() {
    if (!GET_SIG_ADDRESS("ClientInstance::setGuiScaleOption")) return false;
    auto* client = SDK::clientInstance;
    auto* guiData = client ? client->getGuiData() : nullptr;
    if (!guiData) return true;
    int raw = 0;
    if (!guiScaleOption::read(client, raw)) return true;
    if (raw < -16 || raw > 16) {
        option.reset();
        if (!guiScaleOption::repair(client)) return true;
        settled = false;
        steps = stepDirection = waitFrames = 0;
        Logger::info("GUI Scale: repaired invalid saved option {} to automatic (0)", raw);
        relayout();
        return true;
    }
    const float requested = getOps<float>("guiscale");
    const float target = std::clamp(std::round(requested), 1.f, 4.f);
    if (requested != target) settings.setValue("guiscale", target);
    const float current = guiData->GuiScale;
    if (!std::isfinite(current) || current <= 0.f) return true;
    if (target != stepTarget || fixResize) {
        stepTarget = target;
        settled = false;
        steps = stepDirection = 0;
        fixResize = false;
    }
    if (settled || std::fabs(current - target) < 0.25f) return true;
    if (stepDirection != 0) {
        if (waitFrames++ < 3) return true;
        const float moved = current - stepFrom;
        if (moved == 0.f || (stepFrom < target) != (current < target)) {
            option.apply(client, stepValue);
            relayout();
            settled = true;
            return true;
        }
        if (moved * stepDirection * optionSign < 0.f) optionSign = -optionSign;
        stepDirection = 0;
    }
    int value = 0;
    if (!guiScaleOption::read(client, value) || value < -16 || value > 16 || steps >= 8) {
        settled = true;
        Logger::warn("GUI Scale: stopped because the option value or step budget is invalid");
        return true;
    }
    stepDirection = current < target ? 1 : -1;
    int want = value + stepDirection * optionSign;
    if (want < lowLimit || want > highLimit) {
        stepDirection = 0;
        settled = true;
        return true;
    }
    stepFrom = current;
    stepValue = value;
    waitFrames = 0;
    steps++;
    if (!option.apply(client, value + stepDirection * optionSign)) {
        settled = true;
        return true;
    }
    relayout();
    return true;
}

void GuiScale::update() {
    if (stepOption()) return;
    float targetScale = delayDisable ? originalScale : getOps<float>("guiscale");
    auto guiData = SDK::clientInstance->getGuiData();
    if (targetScale == guiData->GuiScale && !delayDisable && !fixResize) return;
    updateScale(targetScale);
}

void GuiScale::updateScale(float newScale) {
    if (restored && !fixResize) return;

    fixResize = false;
    auto guiData = SDK::clientInstance->getGuiData();

    if (originalScale == 0) originalScale = guiData->GuiScale;
    if (newScale == 0) newScale = getOps<float>("guiscale");

    float oldScale = guiData->GuiScale;

    auto screenSize = guiData->ScreenSize;
    static auto safeZone = Vec2<float>{0.f, 0.f};

    SDK::clientInstance->_updateScreenSizeVariables(&screenSize, &safeZone, newScale < 1.f ? 1.f : newScale);
    SDK::screenView->VisualTree->root->forEachChild([this](std::shared_ptr<UIControl> &control) {
        control->updatePosition();
    });

    if (auto movableHotbar = ModuleManager::getModule("Movable Hotbar"); movableHotbar && movableHotbar->isEnabled()) {
        if (std::shared_ptr<MovableHotbar> mod = std::dynamic_pointer_cast<MovableHotbar>(movableHotbar)) {
            mod->lastAppliedPos = Vec2<float>{-120.f, -120.f};
            mod->update();
        }
    }

    if (auto movableScoreboard = ModuleManager::getModule("Movable Scoreboard"); movableScoreboard && movableScoreboard->isEnabled()) {
        if (std::shared_ptr<MovableScoreboard> mod = std::dynamic_pointer_cast<MovableScoreboard>(movableScoreboard)) {
            mod->lastAppliedPos = Vec2<float>{-120.f, -120.f};
            mod->update();
        }
    }

    if (auto movableBossbar = ModuleManager::getModule("Movable Bossbar"); movableBossbar && movableBossbar->isEnabled()) {
        if (std::shared_ptr<MovableBossbar> mod = std::dynamic_pointer_cast<MovableBossbar>(movableBossbar)) {
            mod->lastAppliedPos = Vec2<float>{-120.f, -120.f};
            mod->update();
        }
    }

    if (auto movableChat = ModuleManager::getModule("Movable Chat"); movableChat && movableChat->isEnabled()) {
        if (std::shared_ptr<MovableChat> mod = std::dynamic_pointer_cast<MovableChat>(movableChat)) {
            mod->lastAppliedPos = Vec2<float>{-120.f, -120.f};
            mod->update();
        }
    }

    if (auto movableCoords = ModuleManager::getModule("Movable Coordinates"); movableCoords && movableCoords->isEnabled()) {
        if (std::shared_ptr<MovableCoordinates> mod = std::dynamic_pointer_cast<MovableCoordinates>(movableCoords)) {
            mod->lastAppliedPos = Vec2<float>{-120.f, -120.f};
            mod->update();
        }
    }

    if (auto movableDayCounter = ModuleManager::getModule("Movable Day Counter"); movableDayCounter && movableDayCounter->isEnabled()) {
        if (std::shared_ptr<MovableDayCounter> mod = std::dynamic_pointer_cast<MovableDayCounter>(movableDayCounter)) {
            mod->lastAppliedPos = Vec2<float>{-120.f, -120.f};
            mod->update();
        }
    }

    if (delayDisable) {
        delayDisable = false;
        restored = true;
    }
}
