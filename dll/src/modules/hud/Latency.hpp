#pragma once

#include "gui/Theme.hpp"
#include "hook/Dx.hpp"
#include "hook/GameInput.hpp"
#include "hook/Input.hpp"
#include "modules/HudModule.hpp"
#include "modules/Manager.hpp"
#include "render/Fonts.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <format>

class LatencyHud : public HudModule {
public:
    LatencyHud()
        : HudModule("Latency Meter", "Measures frame time and the time from click to the next frame. Use it to compare settings.",
                    {"hud-self"}, {0.66f, 0.02f}) {
        sub("Network");
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        qpf_ = double(f.QuadPart);
        graphWidth_.visible = graphHeight_.visible = graphThickness_.visible = flashTest_.visible = [this] { return graphShown_.b; };
        lowInterval_.visible = [this] { return lowShown_.b; };
    }

    void onFrame() override {
        auto& fi = dx::frame();
        flash_ = std::max(0.f, flash_ - float(std::max(0.0, fi.frameMs)) / 25.f);
        if (fi.frameMs > 0) {
            times_[head_] = (float)fi.frameMs;
            head_ = (head_ + 1) % times_.size();
            count_ = std::min(count_ + 1, times_.size());
            lowElapsed_ += fi.frameMs;
            if (lowShown_.b && (count_ == 1 || lowElapsed_ >= lowInterval_.f * 1000.0)) {
                std::array<float, 300> sorted;
                std::copy_n(times_.begin(), count_, sorted.begin());
                size_t tail = std::max<size_t>(1, count_ / 100);
                std::partial_sort(sorted.begin(), sorted.begin() + tail, sorted.begin() + count_, std::greater<>());
                float sum = 0;
                for (size_t i = 0; i < tail; i++) sum += sorted[i];
                low_ = sum > 0 ? tail * 1000.f / sum : 0;
                peak_ = sorted[0];
                lowElapsed_ = 0;
            }
        }

        int64_t click = input::lastClickQpc();
        if (click && click != seenClick_ && fi.presentQpc > click) {
            seenClick_ = click;
            double ms = double(fi.presentQpc - click) * 1000.0 / qpf_;
            if (ms < 250) {
                clicks_[clickHead_] = (float)ms;
                clickHead_ = (clickHead_ + 1) % clicks_.size();
                clickCount_ = std::min<size_t>(clickCount_ + 1, clicks_.size());
                flash_ = 1.f;
            }
        }
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& t = theme::current();
        auto& fi = dx::frame();
        float w = graphWidth_.f * s, h = graphHeight_.f * s;
        float y = 0, width = w;
        auto row = [&](const std::string& text, ImU32 color) {
            auto size = drawText(dl, o + ImVec2(0, y), s, text, color);
            y += size.y;
            width = std::max(width, size.x);
        };

        float avgClick = 0;
        for (size_t i = 0; i < clickCount_; i++) avgClick += clicks_[i];
        if (clickCount_) avgClick /= clickCount_;

        if (frameShown_.b) row(i18n::fmt("Frame {:.2f} ms", fi.frameMs), textColor());
        if (clickShown_.b) row(clickCount_ ? i18n::fmt("Click to frame {:.1f} ms", avgClick) : i18n::tr("Click to frame: click once"), accentColor());
        // how old the newest input the game has taken is when the frame goes out, and where the frame limit waits
        if (auto start = gameinput::frameStart(); inputShown_.b && start.verified) {
            row(i18n::fmt("Input {:.1f} ms old at the frame", start.sampleAgeMs), textColor());
            if (start.waitMs > 0.05f) row(i18n::fmt("Limit waits {:.1f} ms in front of the input", start.waitMs), textColor());
        }
        if (lowShown_.b) {
            row(i18n::fmt("1% low {:.0f} FPS  ·  peak {:.1f} ms", low_, peak_), textColor());
        }
        if (cost_.b) {
            row(i18n::fmt("Overlay {:.2f} ms per frame", modules::costMs()), textColor());
            if (auto* worst = modules::slowest(); worst && worst->costMs > 0.05f)
                row(i18n::fmt("Slowest: {} ({:.2f} ms)", worst->name(), worst->costMs), ImGui::GetColorU32(theme::current().textDim));
        }
        if (modeShown_.b) {
            std::string mode = i18n::fmt("{}  ·  {}  ·  {} buffered", i18n::tr(fi.lowLatencyActive ? "Low latency on" : "Low latency off"),
                                           dx::tuning().allowTearing && fi.tearingSupported ? "Tearing" : i18n::tr("VSync/default"), fi.bufferCount);
            float tiny = fonts::hudSize() * s * 0.7f;
            dl->AddText(fonts::hud(), tiny, o + ImVec2(0, y + 2 * s), theme::col(t.textDim), mode.c_str());
            width = std::max(width, fonts::hud()->CalcTextSizeA(tiny, FLT_MAX, 0.f, mode.c_str()).x);
            y += tiny + 6 * s;
        }
        if (!graphShown_.b) return {width, y};

        ImVec2 g0 = o + ImVec2(0, y), g1 = g0 + ImVec2(w, h);
        dl->AddRectFilled(g0, g1, IM_COL32(0, 0, 0, 60), 4 * s);
        float maxMs = 1.f;
        for (float v : times_) maxMs = std::max(maxMs, v);
        maxMs = std::min(maxMs * 1.2f, 50.f);
        size_t n = count_;
        size_t start = (head_ + times_.size() - count_) % times_.size();
        for (size_t i = 1; i < n; i++) {
            float a = times_[(start + i - 1) % times_.size()], b = times_[(start + i) % times_.size()];
            ImVec2 p0{g0.x + w * (i - 1) / (n - 1), g1.y - h * std::min(a / maxMs, 1.f)};
            ImVec2 p1{g0.x + w * i / (n - 1), g1.y - h * std::min(b / maxMs, 1.f)};
            dl->AddLine(p0, p1, theme::col(t.accent), graphThickness_.f * s);
        }
        if (flashTest_.b && flash_ > 0.01f) {
            dl->AddRectFilled(g1 - ImVec2(18 * s, h), g1 - ImVec2(0, h - 18 * s), IM_COL32(255, 255, 255, int(255 * flash_)));
        }
        return {width, y + h};
    }

private:
    Setting& frameShown_ = toggleSetting("frameShown", "Show frame time", true);
    Setting& clickShown_ = toggleSetting("clickShown", "Show click latency", true);
    Setting& inputShown_ = toggleSetting("inputShown", "Show input timing", true);
    Setting& modeShown_ = toggleSetting("modeShown", "Show presentation mode", true);
    Setting& graphShown_ = toggleSetting("graphShown", "Show frame graph", true);
    Setting& graphWidth_ = slider("graphWidth", "Graph width", 220.f, 100.f, 600.f, "%.0f");
    Setting& graphHeight_ = slider("graphHeight", "Graph height", 46.f, 15.f, 160.f, "%.0f");
    Setting& graphThickness_ = slider("graphThickness", "Graph line thickness", 1.5f, 0.5f, 5.f, "%.1f");
    Setting& lowInterval_ = slider("lowInterval", "Statistics update interval (s)", 0.25f, 0.05f, 2.f, "%.2f s");
    Setting& lowShown_ = toggleSetting("low", "1% low and peak", true);
    Setting& cost_ = toggleSetting("cost", "Overlay cost", true);
    Setting& flashTest_ = toggleSetting("flash", "Flash test (white square on click)", false);
    std::array<float, 300> times_{};
    size_t head_ = 0, count_ = 0;
    double lowElapsed_ = 0;
    float low_ = 0, peak_ = 0;
    std::array<float, 20> clicks_{};
    size_t clickHead_ = 0;
    size_t clickCount_ = 0;
    int64_t seenClick_ = 0;
    float flash_ = 0;
    double qpf_ = 1;
};
