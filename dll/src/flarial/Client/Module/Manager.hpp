// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Manager.hpp: reject blocked enable requests at queue and execution time.
#pragma once
#include "Bridge/Policy.hpp"
#include "Bridge/ToggleQueue.hpp"
#include "Utils/Logger/Logger.hpp"

#include <vector>
#include <queue>
#include <mutex>
#include "Modules/Module.hpp"
#include "ModuleState.hpp"
#include "../Events/Listener.hpp"

// TODO make moduleMap AND modules but use moduleMap for search
namespace ModuleManager {
    extern std::map<size_t, std::shared_ptr<Module>> moduleMap;
    extern std::vector<std::shared_ptr<Listener>> services;

    // Queue for deferred enable/disable to avoid deadlock when called from event callbacks
    struct PendingToggle {
        std::shared_ptr<Module> module;
        bool enable;
    };
    inline std::queue<PendingToggle> pendingToggles;
    inline std::mutex pendingTogglesMutex;

    // Queue a module enable/disable to be processed outside of event callbacks
    inline void queueToggle(std::shared_ptr<Module> mod, bool enable) {
        if (!mod || (enable && !modulePolicy::allowed(mod->name))) return; // Ignore null modules
        std::lock_guard<std::mutex> lock(pendingTogglesMutex);
        pendingToggles.push({mod, enable});
    }

    // Process all pending toggles - call this at a safe point (not inside event callbacks)
    // A switch that throws used to stay at the front of the queue and throw again every frame, so no later switch was
    // ever applied and the core's frame never reached its render event. Every entry is taken off the queue first and
    // runs on its own.
    inline void processPendingToggles() {
        toggleQueue::drain(
            pendingToggles, pendingTogglesMutex,
            [](PendingToggle& toggle) {
                if (!toggle.module || (toggle.enable && !modulePolicy::allowed(toggle.module->name))) return;
                if (toggle.enable) {
                    toggle.module->onEnable();
                } else {
                    toggle.module->onDisable();
                }
                toggle.module->enabledState = toggle.enable;
                if (auto* enabled = toggle.module->settings.getSettingByName<bool>("enabled")) enabled->value = toggle.enable;
                Logger::info("flarial core: {} is {}", toggle.module->name, toggle.enable ? "on" : "off");
            },
            [](PendingToggle& toggle) {
                Logger::warn("flarial core: {} threw while it was switched {}", toggle.module->name, toggle.enable ? "on" : "off");
                toggle.module->enabledState = false;
            });
    }

    void initialize();
    void restart();
    void terminate();

    /// Creates, initializes, and registers a module by type; triggers GUI refresh.
    template<typename T, typename... ArgsT>
    void addModule(ArgsT... args) {
        auto modulePtr = std::make_shared<T>(args...);
        modulePtr->postConstructInitialize();
        size_t hash = std::hash<std::string>{}(modulePtr->name);
        moduleMap[hash] = modulePtr;
        ModuleState::cguiRefresh = true;
    }

    /// Factory that creates a module instance without registering it in the module map.
    template<typename T, typename... ArgsT>
std::shared_ptr<T> makeModule(ArgsT... args) {
        return std::make_shared<T>(args...);
    }

    /// Registers a listener service that receives events without being a full module.
    template<typename T, typename... ArgsT>
    void addService(ArgsT... args) {
        auto servicePtr = std::make_shared<T>(args...);
        services.emplace_back(servicePtr);
    }

    void getModules();

    void syncState();

    /// Checks if any registered module has a setting with the given name.
    bool doesAnyModuleHave(const std::string& settingName);
    void updateModulesVector();
    /// Retrieves a registered module by its display name; returns nullptr if not found.
    std::shared_ptr<Module> getModule(const std::string& name);

    inline std::map<size_t, std::shared_ptr<Module>> moduleMap;
    inline std::vector<std::shared_ptr<Listener>> services;
    inline std::vector<std::shared_ptr<Module>> modulesVector;

    // Aliases to ModuleState for backwards compatibility
    inline bool& initialized = ModuleState::initialized;
    inline bool& restartModules = ModuleState::restartModules;
    inline bool& cguiRefresh = ModuleState::cguiRefresh;
}

