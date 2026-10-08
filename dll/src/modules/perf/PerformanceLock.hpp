#pragma once

#include "modules/Manager.hpp"
#include "modules/Module.hpp"

#include <sstream>

class PerformanceLock : public Module {
public:
    PerformanceLock()
        : Module("Performance Lock",
                 "One switch for steady frame times: turns on System Boost, Low Latency and the Frame Limiter and pauses post effects and extras. Your old setup returns when you switch it off.",
                 Category::Performance, {"performance"}) {
        sub("Frame timing");
        snapshot_.hidden = true;
    }

    // The setup is changed once, at the moment the lock is switched on. A snapshot that is already there means the
    // game was started with the lock on: forcing everything again then undid whatever the player had changed in the
    // meantime (a module switched off came back on with every start, one switched on went off).
    void onEnable() override {
        if (!snapshot_.text.empty()) return;
        std::string saved;
        for (auto* m : targets()) saved += m->name() + "=" + (m->userEnabled() ? "1" : "0") + ";";
        snapshot_.text = saved;
        for (auto* m : targets()) m->setEnabled(isBoost(m));
    }

    void onDisable() override {
        std::stringstream in(snapshot_.text);
        std::string item;
        while (std::getline(in, item, ';')) {
            size_t eq = item.rfind('=');
            if (eq == std::string::npos) continue;
            if (auto* m = modules::find(item.substr(0, eq))) m->setEnabled(item.substr(eq + 1) == "1");
        }
        snapshot_.text.clear();
    }

private:
    static bool isBoost(const Module* m) {
        return m->name() == "System Boost" || m->name() == "Low Latency" || m->name() == "Frame Limiter";
    }

    bool isHeavy(const Module* m) const {
        if (m->category() == Category::Fun) return extras_.b;
        return m->sub() == "Post effects" && effects_.b;
    }

    std::vector<Module*> targets() const {
        std::vector<Module*> out;
        for (auto& m : modules::all())
            if (m.get() != this && (isBoost(m.get()) || isHeavy(m.get()))) out.push_back(m.get());
        return out;
    }

    Setting& effects_ = toggleSetting("effects", "Pause post effects", true);
    Setting& extras_ = toggleSetting("extras", "Pause extras (pet, petals, games)", true);
    Setting& snapshot_ = textSetting("snapshot", "Snapshot", "");
};
