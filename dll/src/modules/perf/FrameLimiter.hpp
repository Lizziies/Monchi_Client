#pragma once

#include "Tuning.hpp"
#include "gui/Gui.hpp"
#include "hook/GameInput.hpp"
#include "hook/Dx.hpp"
#include "modules/Module.hpp"

#include <windows.h>

#include <algorithm>

class FrameLimiter : public Module {
public:
    FrameLimiter()
        : Module("Frame Limiter",
                 "Lowers the FPS while Minecraft is in the background. In the game and in its pause menu the game's own limit applies, unless you set one here.",
                 Category::Performance, {"performance"}) {
        sub("Frame timing");
        mode_.visible = menu_.visible = alignInput_.visible = [this] { return !backgroundOnly_.b; };
        fps_.visible = [this] { return !backgroundOnly_.b && mode_.i == 0; };
        offset_.visible = [this] { return !backgroundOnly_.b && mode_.i == 1; };
        bgFps_.visible = [this] { return background_.b; };
        menuFps_.visible = [this] { return !backgroundOnly_.b && menu_.b; };
    }

    void onFrame() override {
        // in front the game keeps its own limit (it has one for the pause menu too); only the background is held down
        float cap = backgroundOnly_.b ? 0.f : baseLimit();
        if (background_.b && !focused()) cap = tighter(cap, bgFps_.f);
        if (!backgroundOnly_.b && menu_.b && gui::open()) cap = tighter(cap, menuFps_.f);
        effective_ = cap;
        perf::limit(cap);
        if (alignInput_.b && !backgroundOnly_.b) perf::alignToInput();
    }

    void drawSettings() override {
        ImGui::Spacing();
        if (effective_ > 0.f) ImGui::TextDisabled(i18n::tr("Active limit: %.0f FPS  ·  Monitor: %d Hz"), effective_, refresh());
        else ImGui::TextDisabled(i18n::tr("No limit active  ·  Monitor: %d Hz"), refresh());
        float ms = (float)dx::frame().frameMs;
        if (ms > 0.f) ImGui::TextDisabled(i18n::tr("Last frame: %.2f ms (%.0f FPS)"), ms, 1000.f / ms);
        auto start = gameinput::frameStart();
        if (alignInput_.b && effective_ > 0.f)
            ImGui::TextDisabled("%s", i18n::tr(start.verified ? "Waiting in front of the input: the frame you see uses the newest input."
                                                              : "The game's input poll is not known yet, so the limit waits after Present."));
    }

private:
    static float tighter(float cap, float limit) { return cap <= 0.f ? limit : std::min(cap, limit); }

    float baseLimit() const {
        if (mode_.i == 0) return fps_.f;
        if (mode_.i == 1) return std::max(10.f, float(refresh()) - offset_.f);
        return 0.f;
    }

    static bool focused() {
        HWND w = dx::window();
        DWORD owner = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &owner);
        return !w || owner == GetCurrentProcessId();
    }

    static int refresh() { return perf::refreshRate(); }

    Setting& backgroundOnly_ = toggleSetting("backgroundOnly", "Only limit in the background", true);
    Setting& mode_ = choice("mode", "Limit", {"Fixed value", "Refresh rate minus offset", "Off"}, 1);
    Setting& fps_ = slider("fps", "FPS limit", 240.f, 30.f, 1000.f, "%.0f");
    Setting& offset_ = slider("offset", "Offset (FPS)", 3.f, 0.f, 20.f, "%.0f");
    Setting& background_ = toggleSetting("background", "Throttle in the background", true);
    Setting& bgFps_ = slider("bgFps", "Background limit", 30.f, 5.f, 120.f, "%.0f");
    Setting& menu_ = toggleSetting("menu", "Throttle while the menu is open", false);
    Setting& menuFps_ = slider("menuFps", "Menu limit", 90.f, 30.f, 240.f, "%.0f");
    Setting& alignInput_ = toggleSetting("alignInput", "Wait in front of the input (lowest latency)", true);
    float effective_ = 0.f;
};
