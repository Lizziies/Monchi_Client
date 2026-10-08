#pragma once

#include "modules/HudModule.hpp"
#include "render/Ui.hpp"

#include <windows.h>
#include <psapi.h>

#include <format>

class Memory : public TextHud {
public:
    Memory() : TextHud("Memory", "Shows how much RAM Minecraft is using.", {"hud-self"}, {0.005f, 0.21f}) {
        sub("Diagnostics");
    }

protected:
    std::string label() const override { return "RAM"; }

    std::string value() override {
        double now = ui::time();
        if (now - last_ > 1.0) {
            last_ = now;
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) mb_ = pmc.WorkingSetSize / (1024.0 * 1024.0);
            MEMORYSTATUSEX ms{sizeof(ms)};
            if (GlobalMemoryStatusEx(&ms)) {
                load_ = ms.dwMemoryLoad;
                totalGb_ = double(ms.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
                usedGb_ = double(ms.ullTotalPhys - ms.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
            }
        }
        switch (mode_.i) {
        case 1: return i18n::fmt("{}% system", load_);
        case 2: return std::format("{:.{}f}{}{:.{}f}{}", usedGb_, systemDecimals_.i, separator_.text, totalGb_, systemDecimals_.i, units_.b ? " GB" : "");
        case 3: return std::format("{:.{}f}{}  ·  {}", mb_, decimals_.i, units_.b ? " MB" : "", i18n::fmt("{}% system", load_));
        default: return std::format("{:.{}f}{}", mb_, decimals_.i, units_.b ? " MB" : "");
        }
    }

private:
    Setting& units_ = toggleSetting("units", "Show units", true);
    Setting& decimals_ = intSlider("decimals", "Decimals", 0, 0, 3);
    Setting& systemDecimals_ = intSlider("systemDecimals", "System decimals", 1, 0, 3);
    Setting& separator_ = textSetting("separator", "Separator", " / ");
    Setting& mode_ = choice("mode", "Display", {"Minecraft (MB)", "System (%)", "System (used / total GB)", "Minecraft and system"});
    double last_ = -10;
    double mb_ = 0;
    double usedGb_ = 0;
    double totalGb_ = 0;
    unsigned load_ = 0;
};
