// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Scripting/ModuleScript.hpp: Flarial's Lua runtime is not built (Monchi runs its own
// scripts), so a script module is only the type the menu code names; no instance is ever created.
#pragma once

#include <Client/Module/Modules/Module.hpp>

class ModuleScript : public Module {
public:
    using Module::Module;
};
