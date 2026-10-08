#pragma once

#include "hook/Dx.hpp"
#include "core/Log.hpp"
#include "modules/Module.hpp"
#include "system/FrameStats.hpp"
#include "system/GpuLatency.hpp"
#include "Tuning.hpp"

class FpsBoost : public Module {
public:
    FpsBoost() : Module("FPS Boost", "Reduces redundant DirectX bridge submissions. Compare overlay time and frame lows in the same world.", Category::Performance, {"performance"}) { sub("Frame timing"); }
    void onFrame() override {
        dx::tuning().efficientOverlay = true;
        if (sync_.b) perf::syncToDisplay();
        if (++samples_ >= 600) {
            samples_ = 0;
            logger::info("fps boost: overlay {} ms, frame p99 {} ms, bridge flushes {}", dx::frame().overlayMs, stats_.p99, dx::frame().overlayFlushes);
        }
        const auto& f = dx::frame();
        if (f.presentQpc != seen_) { seen_ = f.presentQpc; frames_.add(f.frameMs); }
        if (ImGui::GetTime() >= nextStats_) {
            stats_ = frames_.summary();
            nextStats_ = ImGui::GetTime() + 0.25;
        }
    }
    void drawSettings() override {
        const auto& f = dx::frame();
        ImGui::Text(i18n::tr("Overlay CPU time: %.3f ms"), f.overlayMs);
        ImGui::Text(i18n::tr("DirectX bridge flushes: %u"), f.overlayFlushes);
        ImGui::Text(i18n::tr("Frame time p99: %.2f ms"), stats_.p99);
    }
private:
    Setting& sync_ = toggleSetting("tearFree", "Never tear (every frame waits for the display)", true);
    unsigned samples_ = 0;
    int64_t seen_ = 0;
    timing::FrameStats frames_;
    timing::Summary stats_;
    double nextStats_ = 0.0;
};

class UltraInput : public Module {
public:
    UltraInput() : Module("Ultra Low Input Delay", "Requests a one-frame queue and GPU pacing before verified game input. Prevents tearing by default with display sync.", Category::Performance, {"performance"}) { sub("Frame timing"); }
    void onFrame() override {
        perf::lowLatency();
        perf::alignToInput();
        if (sync_.b) perf::syncToDisplay();
        perf::gpu(boost_.b ? gpuLatency::Mode::Boost : gpuLatency::Mode::On);
    }
    void drawSettings() override {
        const auto s = gpuLatency::status();
        ImGui::TextDisabled("%s", i18n::tr(dx::frame().lowLatencyActive ? "Short frame queue active" : "Short frame queue unavailable"));
        ImGui::TextDisabled("%s", i18n::tr(s.frameStartVerified ? "Game input pacing verified" : "Waiting for verified game input pacing"));
        ImGui::TextDisabled("%s", i18n::tr("Compare click timing in Latency Meter at the same FPS limit."));
    }
private:
    Setting& sync_ = toggleSetting("tearFree", "Never tear (every frame waits for the display)", true);
    Setting& boost_ = toggleSetting("boost", "NVIDIA Reflex Boost", false);
};
