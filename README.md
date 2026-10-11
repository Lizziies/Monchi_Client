# Monchi

A free PvP client for Minecraft Bedrock on Windows. The launcher starts Minecraft and loads Monchi into it.

It's still in beta, so there are a few bugs left, mostly in cosmetics, Auto GG and some game hooks.

Right Shift opens the menu, Ctrl+L unloads Monchi. There are HUD modules, chat tools, camera settings, server rules, cosmetics and a lot more, in English and German. If a Minecraft update breaks something, only that feature turns off until it's fixed.

To just play, grab `MonchiLauncher.exe` from the releases page.

## Building

You need Visual Studio 2022 Build Tools (C++ and the Windows SDK) and CMake. Run this in a Developer PowerShell and keep the build folder short, the Flarial core has really long object paths.

```powershell
cmake -S dll -B C:/monchi-build -G "Visual Studio 17 2022" -A x64 -DMONCHI_FLARIAL=ON
cmake --build C:/monchi-build --config Release --target Monchi flarial_core
cmake -S launcher -B build-launcher -G "Visual Studio 17 2022" -A x64 -DMONCHI_DLL=C:/monchi-build/Release/Monchi.dll -DMONCHI_CORE=C:/monchi-build/Release/MonchiFlarial.dll
cmake --build build-launcher --config Release
```

The launcher packs both DLLs into one exe. Without `MONCHI_DLL` it looks for `Monchi.dll` next to itself. The cosmetics aren't in this repo, so a launcher you build yourself runs without them.

The update and Online addresses are build settings. Copy `local.cmake.example` to `local.cmake` (git ignores it) and point it at your own service. `server/README.md` explains how to set up the Online service, `SECURITY.md` what it can and can't check.

The boy and girl skins in the launcher preview are part of the repo. Your own skin only gets read on your PC.

## Tests

```powershell
cmake -S tools/tests -B build-tests -G "Visual Studio 17 2022" -A x64
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
node --test server/test/*.test.js
python tools/check_publish.py --allow-history
```

The Online tests also run with `STORE=d1` and `STORE=turso`. For working on Linux with Wine there's `tools/cross.sh`.

## License

AGPL-3.0, see `LICENSE` and `NOTICE`. If you use code from Monchi, credit it and publish your source under the same license.

Most of Monchi is its own code: the launcher, the menu and HUD, input and frame pacing, the game readers, the cosmetics code, Monchi Online and the update tools. 155 of the 175 modules in the menu are Monchi's.

The other 20 come from [Flarial](https://github.com/flarialmc/dll-oss), ported to Minecraft 1.26.52 in `dll/src/flarial`. Four of them replace a Monchi module (Nametag, View Model, Freelook, Paperdoll). The port has new signatures and offsets for 1.26.52, hooks changed for the new argument layouts, a bridge to Monchi in `flarial/Bridge` and crash guards around every listener. Some byte patterns were read from the installed Flarial client and checked on 1.26.52.3. Flarial's own menu, config, overlay, Discord and scripting parts aren't in here. Every changed file says so in its first lines, and the untouched original is in `vendor/flarial` with its commit in `vendor/flarial/UPSTREAM.json`. `hook/FreeCamera.cpp` and `hook/OwnNametag.cpp` are adapted from Flarial too.

The other libraries keep their own licenses. If you share a build, you have to share its source as well.

Monchi isn't affiliated with Mojang or Microsoft and doesn't ship any Minecraft files. Please stick to the rules of the servers you play on.
