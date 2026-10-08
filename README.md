# Monchi

A free Minecraft Bedrock PvP client for Windows GDK builds. The launcher injects the client DLL into Minecraft. The blue pixel heart is Monchi's logo.

This is a beta source release. Cosmetics, Auto GG and some game hooks still have known bugs. Performance and input latency depend on the game version, hardware and other overlays; this repository does not promise zero latency or a measured advantage over another client.

The client includes configurable HUDs, chat tools, cosmetics, camera settings, server rules and performance diagnostics. It supports English and German. Right Shift opens the in-game menu; Ctrl+L unloads Monchi. Missing version signatures disable the affected game features.

## Build on Windows

Install Visual Studio 2022 Build Tools with C++ and the Windows SDK, plus CMake. Use a Developer PowerShell. Short build paths avoid MSVC object path limits in the optional Flarial core.

```powershell
cmake -S dll -B C:/monchi-build -G "Visual Studio 17 2022" -A x64 -DMONCHI_FLARIAL=ON
cmake --build C:/monchi-build --config Release --target Monchi flarial_core
cmake -S launcher -B build-launcher -G "Visual Studio 17 2022" -A x64 -DMONCHI_DLL=C:/monchi-build/Release/Monchi.dll -DMONCHI_CORE=C:/monchi-build/Release/MonchiFlarial.dll -DMONCHI_COSMETICS="$PWD/cosmetics"
cmake --build build-launcher --config Release
```

The launcher embeds the DLLs and cosmetics into one executable. Without `MONCHI_DLL` it uses the client DLL next to the launcher. Never publish local PDBs, logs, dumps or Minecraft installation files.

Update and Online endpoints are build settings. Copy `local.cmake.example` to the ignored `local.cmake` and configure your own service if needed. The source contains no production credentials or default private service address. See `server/README.md` for deploying the optional Online service and `SECURITY.md` for its identity limitations.

The two bundled Boy/Girl preview skins are included as requested by the project owner. A user's actual skin is read locally at runtime; cached player skins are not shipped in this repository.

## Tests and privacy

```powershell
cmake -S tools/tests -B build-tests -G "Visual Studio 17 2022" -A x64
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
node --test server/test/*.test.js
python tools/check_publish.py --allow-history
```

The Online API tests also run with `STORE=d1` and `STORE=turso`. Linux/Wine development tools are in `tools/cross.sh`.

## License

Copyright (C) 2026 Monchi. AGPL-3.0, see `LICENSE` and `NOTICE`: if you use Monchi's code, credit Monchi and publish your source under the same license.

What is Monchi's own and what comes from Flarial:

- The menu has 175 modules. 155 of them are Monchi's own code. 20 come from the Flarial core described below; 4 of those take the place of a Monchi module (Nametag, View Model, Freelook, Paperdoll), 16 are added on top.
- The launcher, the menu and HUD, input and frame pacing, the live game readers, cosmetics, Monchi Online and the update and signing tools are Monchi's own code.
- The Flarial core (`dll/src/flarial`) is [Flarial](https://github.com/flarialmc/dll-oss) source ported to Minecraft 1.26.52, not a 1:1 copy. It got a 1.26.52 set of 79 signatures and 57 offsets (part of the byte patterns were read from the installed Flarial client, each one checked on 1.26.52.3), hook callbacks rewritten for the new argument layouts, a bridge to Monchi (`flarial/Bridge`, 31 files) and crash guards around every listener. Its own menu, config, overlay, Discord and scripting parts are left out. 93 files there differ from upstream and say so in their first lines; the untouched upstream source and its commit are in `vendor/flarial` and `vendor/flarial/UPSTREAM.json`.
- Two files outside the core are adapted from Flarial (`hook/FreeCamera.cpp`, `hook/OwnNametag.cpp`) and are marked the same way.

Other bundled dependencies retain their own license notices. Distributing a build also requires making its corresponding source available.

Monchi is not affiliated with Mojang or Microsoft. It does not distribute Minecraft files. Version tools rely on the user's own Minecraft entitlement. Follow each server's rules.
