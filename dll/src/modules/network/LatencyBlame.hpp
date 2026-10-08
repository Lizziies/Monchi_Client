#pragma once

#include "Probe.hpp"
#include "gui/Theme.hpp"
#include "hook/Dx.hpp"
#include "hook/Input.hpp"
#include "modules/HudModule.hpp"
#include "render/Fonts.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <format>

class LatencyBlame : public HudModule {
public:
    LatencyBlame()
        : HudModule("Lag Analyzer",
                    "Splits the delay into input, frame, network and server tick.",
                    {"hud-self"}, {0.6f, 0.38f}) {
        sub("Network");
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        qpf_ = double(f.QuadPart);
        tick_.visible = [this] { return serverTick_.b; };
    }

    void onEnable() override { probe::use(true); }
    void onDisable() override { probe::use(false); }

    void onFrame() override {
        auto& fi = dx::frame();
        int64_t click = input::lastClickQpc();
        if (!click || click == seen_ || fi.presentQpc <= click) return;
        seen_ = click;
        double ms = double(fi.presentQpc - click) * 1000.0 / qpf_;
        if (ms >= 250) return;
        ring_[head_] = (float)ms;
        head_ = (head_ + 1) % ring_.size();
        count_ = std::min(count_ + 1, ring_.size());
    }

protected:
    struct Part {
        const char* name;
        float ms;
        ImVec4 color;
    };

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& t = theme::current();
        auto snap = probe::metrics();
        auto& fi = dx::frame();

        float sample = 0.f;
        for (size_t i = 0; i < count_; i++) sample += ring_[i];
        sample = count_ ? sample / count_ : 0.f;

        float frame = (float)fi.frameMs;
        float input = std::max(0.f, sample - frame);
        float render = std::min(sample, frame);
        float network = (snap.received > 0 ? snap.avg : 0.f) * (pingMode_.i == 0 ? 1.f : 0.5f);
        float server = serverTick_.b ? tick_.f * 0.5f : 0.f;

        std::array<Part, 4> parts{{
            {i18n::tr("Input"), input, inputColor_.color},
            {i18n::tr("Frame"), render, renderColor_.color},
            {i18n::tr("Network"), network, netColor_.color},
            {i18n::tr("Server"), server, serverColor_.color},
        }};
        float total = 0.f;
        for (auto& p : parts) total += p.ms;

        float lineH = fonts::hudSize() * s * 1.1f;
        float width = width_.f * s, y = 0;

        std::string head = count_ ? i18n::fmt("About {:.0f} ms in total until the hit", total) : i18n::tr("Click once and I will measure");
        if (head_show_.b || !count_) y += drawText(dl, o, s, head, textColor()).y;

        float barH = barHeight_.f * s;
        ImVec2 b0 = o + ImVec2(0, y + 3 * s);
        dl->AddRectFilled(b0, b0 + ImVec2(width, barH), IM_COL32(0, 0, 0, 70), barH * 0.5f);
        if (total > 0.f) {
            float x = 0.f;
            for (auto& p : parts) {
                float w = width * p.ms / total;
                if (w < 0.5f) continue;
                dl->AddRectFilled(b0 + ImVec2(x, 0), b0 + ImVec2(x + w, barH), ImGui::GetColorU32(p.color), x == 0.f ? barH * 0.5f : 0.f);
                x += w;
            }
        }
        y += barH + 8 * s;

        float biggest = 0.f;
        const Part* worst = nullptr;
        for (auto& p : parts) {
            if (p.ms <= 0.f) continue;
            if (p.ms > biggest) {
                biggest = p.ms;
                worst = &p;
            }
            if (!rows_.b) continue;
            if (dots_.b) dl->AddCircleFilled(o + ImVec2(5 * s, y + lineH * 0.5f), 4 * s, ImGui::GetColorU32(p.color));
            drawText(dl, o + ImVec2(dots_.b ? 14 * s : 0.f, y), s, std::format("{}{}{:.{}f} ms", p.name, sep_.text, p.ms, decimals_.i), dots_.b ? textColor() : ImGui::GetColorU32(p.color));
            y += lineH;
        }
        if (verdict_.b && worst && total > 0.f)
            y += drawText(dl, o + ImVec2(0, y + 2 * s), s, i18n::fmt("Biggest share: {} ({:.0f} %)", worst->name, 100.f * biggest / total),
                          ImGui::GetColorU32(t.textDim)).y;
        return {width, y};
    }

private:
    Setting& pingMode_ = choice("pingMode", "Network share", {"Full round trip", "Half round trip (one way)"}, 0);
    Setting& serverTick_ = toggleSetting("serverTick", "Include the server tick", true);
    Setting& tick_ = slider("tick", "Tick interval (ms)", 50.f, 25.f, 100.f, "%.0f");
    Setting& verdict_ = toggleSetting("verdict", "Name the biggest share", true);
    Setting& head_show_ = toggleSetting("head", "Total line", true);
    Setting& rows_ = toggleSetting("rows", "List of the shares", true);
    Setting& dots_ = toggleSetting("dots", "Colored dots (off colors the text)", true);
    Setting& sep_ = textSetting("sep", "Separator", "  ");
    Setting& decimals_ = intSlider("decimals", "Decimals", 1, 0, 2);
    Setting& width_ = slider("width", "Width", 230.f, 140.f, 420.f, "%.0f");
    Setting& barHeight_ = slider("barHeight", "Bar height", 10.f, 4.f, 24.f, "%.0f");
    Setting& inputColor_ = colorSetting("inputColor", "Color input", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& renderColor_ = colorSetting("renderColor", "Color frame", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& netColor_ = colorSetting("netColor", "Color network", {0.71f, 0.61f, 1.f, 1.f});
    Setting& serverColor_ = colorSetting("serverColor", "Color server", {1.f, 0.82f, 0.49f, 1.f});
    std::array<float, 20> ring_{};
    size_t head_ = 0;
    size_t count_ = 0;
    int64_t seen_ = 0;
    double qpf_ = 1;
};
