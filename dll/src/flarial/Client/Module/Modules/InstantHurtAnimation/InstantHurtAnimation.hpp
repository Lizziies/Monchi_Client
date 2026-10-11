// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/InstantHurtAnimation/InstantHurtAnimation.hpp: on 1.26.52 the animation is started on the
// target itself instead of through a made-up packet, see InstantHurtAnimation.cpp.
#pragma once

#include "../Module.hpp"


class InstantHurtAnimation : public Module {
public:
	InstantHurtAnimation() : Module("Insta Hurt Animation", "Hurt animation becomes ping independent, helps time hits.",
		IDR_COMBO_PNG, "") {

	};

	void onEnable() override;

	void onDisable() override;

	void defaultConfig() override;

	void settingsRender(float settingsOffset) override;

	void onAttack(AttackEvent& event);
};
