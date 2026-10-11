#pragma once

#include "I18n.hpp"
#include "core/Log.hpp"
#include "Tuning.hpp"
#include "hook/Dx.hpp"
#include "hook/GameInput.hpp"
#include "hook/Input.hpp"
#include "modules/Module.hpp"
#include "system/GpuLatency.hpp"
#include "system/FrameStats.hpp"

#include <windows.h>

#include <algorithm>

class LowLatency : public Module {
public:
    LowLatency()
        : Module("Low Latency", "Shorter frame queue, optional tearing instead of VSync, a higher priority render thread and a precise FPS limiter. Shows the time from click to frame so you can compare.",
                 Category::Performance, {"performance"}) {
        sub("Frame timing");
        tearing_.visible = [this] { return !sync_.b; };
        underRefresh_.visible = [this] { return sync_.b; };
        limit_.visible = [this] { return useLimit_.b; };
        driver_.visible = [] {
            const auto status = gpuLatency::status();
            return status.vendor == gpuLatency::Vendor::Nvidia && status.stage != gpuLatency::Stage::Unsupported ||
                   status.vendor == gpuLatency::Vendor::Amd && status.frameStartVerified;
        };
    }

    void onDisable() override {
        restore();
    }

    void onFrame() override {
        if (queue_.b) perf::lowLatency();
        if (sync_.b) perf::syncToDisplay(underRefresh_.b);
        else if (tearing_.b) perf::tearing();
        if (useLimit_.b) {
            perf::limit(limit_.f);
            perf::alignToInput();
        }
        if (priority_.b) boost();
        else restore();
        perf::gpu(static_cast<gpuLatency::Mode>(driver_.i));
        report();
        track();
    }

    void drawSettings() override {
        auto& fi = dx::frame();
        if (ImGui::GetTime() >= nextStats_) {
            stats_ = frames_.summary();
            nextStats_ = ImGui::GetTime() + 0.25;
        }
        const auto& frames = stats_;
        ImGui::Spacing();
        if (frames.count)
            ImGui::TextDisabled(i18n::tr("Frame intervals: %.2f ms average, %.2f ms P95, %.2f ms P99"),
                                frames.mean, frames.p95, frames.p99);
        const auto driver = gpuLatency::status();
        const char* state = "GPU latency unavailable";
        switch (driver.stage) {
        case gpuLatency::Stage::Off: state = "GPU latency off"; break;
        case gpuLatency::Stage::DriverOnly: state = "NVIDIA driver latency active; input pacing not connected"; break;
        case gpuLatency::Stage::BeforeInput: state = "GPU pacing connected before input"; break;
        case gpuLatency::Stage::Failed: state = "GPU latency driver rejected the request"; break;
        default: break;
        }
        ImGui::TextDisabled("%s", i18n::tr(state));
        if (sync_.b && underRefresh_.b)
            ImGui::TextDisabled("%s", perf::displayFollows() ? i18n::fmt("Display {} Hz, frame rate held at {:.0f} FPS", perf::refreshRate(), perf::underRefreshCap()).c_str()
                                                             : i18n::fmt("Display {} Hz with a fixed rate: no cap, every frame waits for it", perf::refreshRate()).c_str());
        if (perf::foreignPacer())
            ImGui::TextColored(ImVec4(1.f, 0.82f, 0.49f, 1.f), "%s", i18n::tr("RTSS / Afterburner is running in the game: Monchi leaves Reflex and the frame limit to it. Close RTSS to let Monchi do both."));
        if (driver.vendor == gpuLatency::Vendor::Amd && !driver.frameStartVerified)
            ImGui::TextDisabled("%s", i18n::tr("AMD Anti-Lag 2 needs a verified frame start hook"));
        if (driver.error) ImGui::TextDisabled(i18n::tr("Driver error: %d"), driver.error);
        ImGui::TextDisabled(i18n::tr("Status: %s, %s"), i18n::tr(fi.lowLatencyActive ? "short queue active" : "default queue"),
                            i18n::tr(fi.tearingSupported ? "tearing possible" : "tearing not allowed by the game"));
        auto start = gameinput::frameStart();
        if (start.hooked) {
            ImGui::TextDisabled("%s", i18n::tr(start.verified ? "Frame start found: the limit and GPU pacing wait in front of the input."
                                                              : "Frame start not found yet: waiting for a steady input poll."));
            if (start.verified) {
                ImGui::TextDisabled(i18n::tr("Newest input at Present: %.1f ms old"), start.sampleAgeMs);
                ImGui::TextDisabled(i18n::tr("Wait in front of the input: %.1f ms per frame"), start.waitMs);
            }
        }
        if (count_) ImGui::TextDisabled(i18n::tr("Click event to Present return: %.1f ms (%d samples)"), sum_ / count_, count_);
        else ImGui::TextDisabled(i18n::tr("Click to measure the latency."));
        ImGui::TextDisabled("%s", i18n::tr("This does not measure input to the displayed image."));
        if (ImGui::SmallButton(i18n::tr("Reset measurement"))) {
            sum_ = 0;
            count_ = 0;
            frames_.clear();
            stats_ = {};
            nextStats_ = 0.0;
        }
    }

private:
    void report() {
        const auto s = gpuLatency::status();
        auto& fi = dx::frame();
        int key = int(s.stage) * 1000 + s.error * 10 + (fi.lowLatencyActive ? 1 : 0) + (fi.tearingSupported ? 2 : 0) + (gameinput::frameStartVerified() ? 4 : 0);
        if (key == reported_) return;
        reported_ = key;
        logger::info("low latency: gpu stage {}, driver error {}, short queue {}, tearing {}, frame start {}", int(s.stage), s.error,
                     fi.lowLatencyActive ? "on" : "off", fi.tearingSupported ? "possible" : "not allowed", gameinput::frameStartVerified() ? "found" : "not found");
    }

    int reported_ = -1;
    timing::Summary stats_;
    double nextStats_ = 0.0;

    void boost() {
        if (thread_) return;
        thread_ = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
        if (!thread_) return;
        base_ = GetThreadPriority(thread_);
        SetThreadPriority(thread_, std::max(base_, (int)THREAD_PRIORITY_ABOVE_NORMAL));
    }

    void restore() {
        if (!thread_) return;
        SetThreadPriority(thread_, base_);
        CloseHandle(thread_);
        thread_ = nullptr;
    }

    void track() {
        auto& fi = dx::frame();
        if (fi.presentQpc != frameSeen_) {
            frameSeen_ = fi.presentQpc;
            frames_.add(fi.frameMs);
        }
        int64_t click = input::lastClickQpc();
        if (!click || click == seen_ || fi.presentQpc <= click) return;
        seen_ = click;
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        double ms = double(fi.presentQpc - click) * 1000.0 / double(f.QuadPart);
        if (ms < 250) {
            sum_ += (float)ms;
            count_++;
        }
    }

    Setting& queue_ = toggleSetting("queue", "Short frame queue", true);
    Setting& driver_ = choice("gpuLatency", "NVIDIA Reflex / AMD Anti-Lag 2", {"Off", "On", "On + NVIDIA Boost"}, 1);
    Setting& sync_ = toggleSetting("tearFree", "Never tear (every frame waits for the display)", true);
    Setting& underRefresh_ = toggleSetting("underRefresh", "Hold the FPS just under the refresh rate (G-Sync, FreeSync, smoother with VSync)", true);
    Setting& tearing_ = toggleSetting("tearing", "Allow tearing (VSync off)", false);
    Setting& priority_ = toggleSetting("priority", "Render thread with higher priority", true);
    Setting& useLimit_ = toggleSetting("limit", "Custom FPS limiter", false);
    Setting& limit_ = slider("fps", "FPS limit", 240.f, 30.f, 1000.f, "%.0f");
    HANDLE thread_ = nullptr;
    int base_ = 0;
    int64_t seen_ = 0;
    int64_t frameSeen_ = 0;
    timing::FrameStats frames_;
    float sum_ = 0.f;
    int count_ = 0;
};
