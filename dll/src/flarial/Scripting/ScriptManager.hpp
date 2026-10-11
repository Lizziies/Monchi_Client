// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Scripting/ScriptManager.hpp: Monchi runs its own Lua scripts, so Flarial's script
// runtime is not built and this keeps only the calls the rest of Flarial makes, all doing nothing.
#pragma once

#include <memory>
#include <string>
#include <vector>

class Module;
class ModuleScript;

class ScriptManager {
public:
    static void initialize() {}
    static void shutdown() {}
    static void reloadScripts() {}
    static void _reloadScripts() {}
    static void saveSettings() {}
    static std::vector<std::shared_ptr<ModuleScript>> getLoadedModules() { return {}; }
    static std::shared_ptr<Module> getModuleByName(const std::vector<std::shared_ptr<ModuleScript>>&, const std::string&) { return nullptr; }
    static inline bool initialized = false;
};
