// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/ForceCoords/ForceCoords.hpp: on 1.26.52 the patched byte pair is the
// "jne" after "cmp byte [rcx+0x1184], 0" in ClientInstanceScreenModel::shouldDisplayPlayerPosition (0x187f830); as an
// unconditional jump the function returns the true it loaded into al.
#pragma once

#include "../Module.hpp"
#include "../../../Client.hpp"


#include "Assets/Assets.hpp"
#include "Utils/Memory/PatchSite.hpp"

class ForceCoords : public Module {
private:
	static inline codepatch::Site site;
	bool mojanged = false;
	bool patched = false;
public:
	ForceCoords() : Module("Force Coordinates", "Shows your ingame position. (XYZ)",
		IDR_COORDINATES_PNG, "") {
		if (!codepatch::armShortJneToJmp(site, GET_SIG_ADDRESS("ForceCoordsOption")))
			Logger::warn("Force Coordinates: ForceCoordsOption is missing or not a jne, module stays inactive");

		loadSettings();
	};

	void onEnable() override;

	void onDisable() override;

	void patch();

	void unpatch();

	void defaultConfig() override;

	void settingsRender(float settingsOffset) override;

	void onRender(RenderEvent& event);
};

