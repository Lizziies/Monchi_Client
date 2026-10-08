// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/GuiScale/GuiScale.hpp: state for the option based scale on 1.26.52.
#pragma once

#include "../Module.hpp"

#include <climits>
#include "Events/Render/SetupAndRenderEvent.hpp"
#include "../../../../Assets/Assets.hpp"
#include "SDK/Client/Core/GuiScaleOption.hpp"

class GuiScale : public Module {
private:
	float originalScale = 0.f;
	bool restored = false;
	guiScaleOption::Override option;
	float stepFrom = 0.f;
	float stepTarget = 0.f;
	int stepDirection = 0;
	int optionSign = 1;
	bool settled = false;
	// The option is an integer step whose useful range depends on the window: past the ends the game keeps storing the
	// value but the scale stays put. A step that moved nothing marks that end, so a target that keeps changing while the
	// slider is dragged cannot walk the option away.
	int stepValue = 0;
	int lowLimit = INT_MIN;
	int highLimit = INT_MAX;
	float limitsFor = 0.f;

	int steps = 0;
	int waitFrames = 0;
	void relayout();
	bool stepOption();
public:
	static inline bool fixResize = false;
	GuiScale() : Module("MC GUI Scale", "Adjust the Minecraft UI to the nearest supported scale.",
		IDR_SCALE_PNG, "", false, {"size"}) {
		
	};

	void onEnable() override;

	void onDisable() override;

	void defaultConfig() override;

	void settingsRender(float settingsOffset) override;

	void onSetupAndRender(SetupAndRenderEvent& event);;

	void update();

	void updateScale(float newScale);
};
