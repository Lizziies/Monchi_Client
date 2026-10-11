// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/JavaViewBobbing/JavaViewBobbing.hpp: apply hand sway through renderItem's verified matrix stack and Monchi's player reader.
#pragma once
#include "../Module.hpp"
#include "Events/Game/RenderItemInHandEvent.hpp"
#include "SDK/Client/Render/Camera.hpp"
#include "Bridge/PlayerPose.hpp"
#include "Bridge/WalkBob.hpp"
#include <algorithm>
#include <cmath>
#include <glm/glm/ext/matrix_transform.hpp>

class JavaViewBobbing : public Module {
public:
    JavaViewBobbing() : Module("Java View Bobbing", "Adds smooth hand movement when walking, looking and jumping.", IDR_EYE_PNG, "", false) {}
    void onEnable() override {
        previous = {};
        x = y = jump = 0.f;
        walk = {};
        Listen(this, RenderItemInHandEvent, &JavaViewBobbing::render)
        Module::onEnable();
    }
    void onDisable() override {
        Deafen(this, RenderItemInHandEvent, &JavaViewBobbing::render)
        previous = {};
        Module::onDisable();
    }
    void defaultConfig() override {
        Module::defaultConfig("core");
        setDef("velocityfactor", 1.f);
        setDef("jumpvelocityfactor", 4.f);
    }
    void settingsRender(float) override {
        initSettingsPage();
        addSlider("Velocity Factor", "Speed of the hand moving", "velocityfactor", 10.f, 0.1f);
        addSlider("Jump Velocity Factor", "The factor jumping has on the hand movement", "jumpvelocityfactor", 10.f, 0.f);
        FlarialGUI::UnsetScrollView();
        resetPadding();
    }
    void render(RenderItemInHandEvent&) {
        const auto pose = playerPose::read();
        auto* stack = mce::Camera::activeWorld;
        if (!pose.valid || !stack) { previous = {}; walk = {}; return; }
        const double dt = pose.time - previous.time;
        if (!previous.valid || dt < 0.0 || dt > 0.25) {
            previous = pose;
            x = y = jump = 0.f;
            walk = {};
            return;
        }
        if (dt > 0.0) {
            const float smooth = 1.f - std::exp(-12.f * float(dt));
            walk.update(std::hypot(pose.x - previous.x, pose.z - previous.z), float(dt));
            const float speed = getOps<float>("velocityfactor");
            const float yaw = std::remainder(previous.yaw - pose.yaw, 360.f);
            x += (std::clamp(yaw / float(dt) * 0.002f * speed, -0.3f, 0.3f) - x) * smooth;
            y += (std::clamp((pose.pitch - previous.pitch) / float(dt) * 0.002f * speed, -0.3f, 0.3f) - y) * smooth;
            const float rise = (pose.y - previous.y) / float(dt);
            const float target = std::clamp(y - rise * 0.00625f * getOps<float>("jumpvelocityfactor"), -0.3f, 0.3f);
            jump += (target - jump) * smooth;
            previous = pose;
        }
        auto& matrix = stack->top().matrix;
        matrix = glm::translate(matrix, glm::vec3(x + walk.sideways(), jump + walk.vertical(), 0.f));
    }
private:
    playerPose::Pose previous{};
    walkBob::Motion walk;
    float x = 0.f, y = 0.f, jump = 0.f;
};
