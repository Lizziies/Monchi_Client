# Flarial source baseline

Upstream: https://github.com/flarialmc/dll-oss, revision recorded in `UPSTREAM.json`.
The copied source is licensed under AGPL-3.0; the original license is included.
All 117 module folders are preserved alongside their events, hooks, SDK and signature definitions.

These are source files for comparison and adaptation. They are not all compiled into Mochi:
Flarial's module manager, renderer, configuration and SDK interfaces differ from Mochi's.
Keep Mochi's GUI and additional modules when adapting a module. Mark an adaptation tested
only after its native hook and visible effect have both been verified on the target game.

First adaptation: `dll/src/hook/FreeCamera.cpp` uses the native three-argument camera/player
update callback from Flarial, controlled by Mochi's Freelook settings. This replaces the
previous mutation of entity registry membership. Body- and head-yaw stores are also
patched and restored, with the complete five-byte head store used by 1.26. The signatures
are upstream candidates for Bedrock 1.26.52 and have not yet been verified in a Mochi game session.
Missing candidates keep the module unavailable. No new injection is authorized during
the user's Flarial gameplay session.

Second adaptation: `dll/src/hook/OwnNametag.cpp` opens the native self-name gate while
in third person. It leaves Minecraft's text, fonts, resource-pack glyphs, colors and
formatting untouched. Mochi's previous overlay remains an explicit customization.
The gate signature also needs in-game verification. The local lifecycle tests cover
missing bindings, hook-enable failure, restoration and refusing another owner's patch.
