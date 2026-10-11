// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/BlockHit/BlockHit.hpp: the perspective starts as first person instead of
// undefined, and the hand eases into the blocking pose, see BlockHit.cpp.
#pragma once

#include <chrono>

#include "../Module.hpp"
#include "../CPS/CPSCounter.hpp"
#include "../../../Events/Game/RenderItemInHandEvent.hpp"
#include "Events/Game/PerspectiveEvent.hpp"
#include "../../../../Assets/Assets.hpp"

class BlockHit : public Module {
private:
	float amount = 0.f;
	std::chrono::steady_clock::time_point seen;

public:
	Perspective perspective = Perspective::FirstPerson;

	BlockHit() : Module("Block Hit", "Sword Blocking Animation like Java (visual only)",
		IDR_SWORD_PNG, "", false, {"java", "sword", "swing"}) {};

	void onEnable() override;

	void onDisable() override;

	void onPerspectiveChange(PerspectiveEvent& event);

	void onItemInHandRender(RenderItemInHandEvent& event);

	void defaultConfig() override;
};
