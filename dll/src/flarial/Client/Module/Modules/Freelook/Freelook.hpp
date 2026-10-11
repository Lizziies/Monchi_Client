// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/Freelook/Freelook.hpp: the five camera stores of 1.26.52 are a table of
// whole instructions that are checked against the image before they are patched (see dll/src/flarial/agent_report_camera.md).
#pragma once

#include "../Module.hpp"
#include "Assets/Assets.hpp"
#include "Events/Game/PerspectiveEvent.hpp"
#include "Events/Game/UpdatePlayerEvent.hpp"
#include "Utils/Memory/PatchSite.hpp"

class FreeLook : public Module {
private:
	enum Group : uint8_t { Rotation = 1, Head = 2, Movement = 4 };

	// Rotation: the shared setRot-with-wrap routine (0x33de990) copies the new Vec2 into the rotation component and
	// keeps the previous rotation continuous. Yaw needs both its plain and its wrapped store, the previous-rotation
	// stores keep the interpolation from drifting while the rotation itself stands still.
	// Head: the head rotation setter (0x1a29cb0), two stores in one go.
	// Movement: the wrap routine behind 0x19f93c0, not proven to matter for the local player.
	// Change enabledGroups to change what freelook freezes; _updatePlayer is cancelled separately.
	static constexpr uint8_t enabledGroups = Rotation | Head;

	struct Entry {
		const char* sig;
		uint8_t group;
		uint8_t length;
		std::array<uint8_t, 11> expect;
	};

	static constexpr std::array<Entry, 11> entries{{
		{"CameraYaw", Rotation, 5, {0xF3, 0x0F, 0x11, 0x42, 0x04}},
		{"CameraPitch", Rotation, 4, {0xF3, 0x0F, 0x11, 0x0A}},
		{"CameraYaw3", Rotation, 6, {0xF3, 0x44, 0x0F, 0x11, 0x56, 0x04}},
		{"CameraYawPrev", Rotation, 5, {0xF3, 0x0F, 0x11, 0x57, 0x04}},
		{"CameraPitchPrev", Rotation, 4, {0xF3, 0x0F, 0x11, 0x0F}},
		{"CameraYaw2", Head, 11, {0xF3, 0x0F, 0x11, 0x0C, 0xD0, 0xF3, 0x0F, 0x11, 0x54, 0xD0, 0x04}},
		{"CameraMovement", Movement, 5, {0xF3, 0x44, 0x0F, 0x11, 0x16}},
		{"TurnHeadYaw", Head, 5, {0xF3, 0x0F, 0x11, 0x0C, 0xD0}},
		{"TurnHeadPitch", Head, 7, {0xF3, 0x44, 0x0F, 0x11, 0x4C, 0xD0, 0x04}},
		{"TurnHeadYawBack", Head, 5, {0xF3, 0x0F, 0x11, 0x3C, 0xD0}},
		{"TurnHeadPitchBack", Head, 6, {0xF3, 0x0F, 0x11, 0x7C, 0xD0, 0x04}},
	}};

	static inline std::array<codepatch::Site, entries.size()> sites;
	static inline bool armedOnce = false;
public:

	FreeLook() : Module("FreeLook",
		"Freely move your camera in 3rd person mode\nwhile keeping the player rotation the same.",
		IDR_FREELOOK_PNG, "F", false, {"360"}) {
		arm();
	};

	static void arm() {
		if (armedOnce) return;
		armedOnce = true;
		for (size_t i = 0; i < entries.size(); i++) {
			const auto& e = entries[i];
			if (e.length == 0 || !(e.group & enabledGroups)) continue;
			uintptr_t at = Mgr.getSigAddress(Utils::hash(e.sig));
			uint8_t nops[codepatch::maxLen];
			memset(nops, 0x90, sizeof nops);
			if (!codepatch::armBytes(sites[i], at, e.expect.data(), e.length, nops))
				Logger::warn("FreeLook: {} does not hold the expected instruction, site skipped", e.sig);
		}
	}

	void onSetup() override;

	void onEnable() override;

	void onDisable() override;

	void patch();

	static bool headGroup(size_t i) { return entries[i].group == Head; }

	void unpatch();

	void apply();

	bool engaged = false;

	void defaultConfig() override;

	void settingsRender(float settingsOffset) override;

	void onUpdatePlayer(UpdatePlayerEvent& event);

	void onGetViewPerspective(PerspectiveEvent& event);

	void onKey(KeyEvent& event);

	void onMouse(MouseEvent& event);
};
