#pragma once

#include "Probe.hpp"
#include "Quality.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/HudModule.hpp"
#include "modules/common/Colors.hpp"
#include "render/Fonts.hpp"

#include <algorithm>
#include <format>

class Network : public HudModule {
public:
    Network()
        : HudModule("Network Monitor",
                    "Ping, jitter and packet loss to the server, plus Wi-Fi data and tips.",
                    {"hud-self"}, {0.6f, 0.1f}) {
        sub("Network");
        interval_.visible = [this] { return advanced_.b; };
        window_.visible = [this] { return advanced_.b; };
        host_.visible = [this] { return advanced_.b; };
        port_.visible = [this] { return advanced_.b && method_.i == 1; };
        pingFair_.visible = [this] { return advanced_.b; };
        pingPoor_.visible = [this] { return advanced_.b; };
        jitterFair_.visible = [this] { return advanced_.b; };
        jitterPoor_.visible = [this] { return advanced_.b; };
        lossPoor_.visible = [this] { return advanced_.b; };
        graphHeight_.visible = [this] { return graph_.b; };
        tips_.visible = [this] { return tipsOn_.b; };
        labelSide_.visible = [this] { return labels_.b; };
    }

    void onEnable() override { probe::use(true); }
    void onDisable() override {
        probe::use(false);
        probe::configure(probe::Config{});
    }

    void onFrame() override {
        probe::Config c;
        c.method = method_.i == 1 ? probe::Method::RakNet : probe::Method::Icmp;
        c.intervalMs = int(interval_.f);
        c.window = int(window_.f);
        c.port = port_.i;
        c.host = host_.text;
        probe::configure(c);

        if (!toast_.b) return;
        auto verdict = quality::judge(probe::snapshot(), limits());
        if (toast_.b && verdict.grade == quality::Grade::Poor && last_ != quality::Grade::Poor)
            notify::push(i18n::tr("Connection poor"), verdict.reason, notify::Kind::Warn, 5.f);
        last_ = verdict.grade;
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto snap = probe::snapshot();
        auto verdict = quality::judge(snap, limits());
        auto& t = theme::current();
        float lineH = fonts::hudSize() * s * 1.1f;
        float y = 0, width = 0;
        auto line = [&](const std::string& text, ImU32 col) {
            auto sz = drawText(dl, o + ImVec2(0, y), s, text, col);
            width = std::max(width, sz.x);
            y += lineH;
        };

        if (status_.b) {
            ImVec4 c = gradeColor(verdict.grade);
            float r = fonts::hudSize() * s * 0.32f;
            dl->AddCircleFilled(o + ImVec2(r, lineH * 0.5f), r, ImGui::GetColorU32(c));
            auto sz = drawText(dl, o + ImVec2(r * 2 + 6 * s, 0), s, verdict.label, ImGui::GetColorU32(c));
            width = std::max(width, sz.x + r * 2 + 6 * s);
            y += lineH;
        }

        if (!snap.running || !snap.resolved || snap.received == 0) {
            line(snap.sent > 0 ? i18n::tr("No reply") : i18n::tr("Waiting for the server"), textColor());
        } else {
            std::string row;
            auto metric = [&](const char* label, std::string value, const char* unit) {
                if (!row.empty()) row += separator_.text;
                if (units_.b) value += std::string(" ") + unit;
                if (!labels_.b) row += value;
                else if (labelSide_.i == 0) row += std::string(i18n::tr(label)) + " " + value;
                else row += value + " " + i18n::tr(label);
            };
            if (ping_.b) metric("Ping", std::format("{:.0f}", snap.last >= 0 ? snap.last : snap.avg), "ms");
            if (jitter_.b) metric("Jitter", std::format("{:.1f}", snap.jitter), "ms");
            if (loss_.b) metric("Loss", std::format("{:.1f}", snap.loss), "%");
            if (!row.empty()) {
                ImVec4 c = rampColor(snap.avg, limits().pingFair, limits().pingPoor, good_.color, fair_.color, poor_.color);
                line(row, ImGui::GetColorU32(c));
            }
            if (range_.b) line(i18n::fmt("Min {:.0f}  ·  Avg {:.0f}  ·  Max {:.0f} ms", snap.min, snap.avg, snap.max), accentColor());
            if (link_.b) line(linkText(snap.link), accentColor());
            if (power_.b && snap.link.powerSaving == 1) line(i18n::tr("Adapter power saving is on"), ImGui::GetColorU32(t.warn));
        }

        if (reason_.b && !verdict.reason.empty()) line(verdict.reason, ImGui::GetColorU32(t.textDim));
        if (tipsOn_.b)
            for (size_t i = 0; i < verdict.tips.size() && int(i) < tips_.i; i++) line("→ " + verdict.tips[i], ImGui::GetColorU32(t.textDim));

        if (graph_.b && !snap.history.empty()) {
            float gw = std::max(width, 180.f * s), gh = graphHeight_.f * s;
            drawGraph(dl, o + ImVec2(0, y + 2 * s), {gw, gh}, snap, s);
            width = std::max(width, gw);
            y += gh + 4 * s;
        }
        return {std::max(width, 120.f * s), std::max(y, lineH)};
    }

private:
    quality::Limits limits() const {
        return {pingFair_.f, pingPoor_.f, jitterFair_.f, jitterPoor_.f, lossPoor_.f};
    }

    ImVec4 gradeColor(quality::Grade g) const {
        switch (g) {
        case quality::Grade::Good: return good_.color;
        case quality::Grade::Fair: return fair_.color;
        case quality::Grade::Poor: return poor_.color;
        default: return theme::current().textDim;
        }
    }

    static std::string linkText(const probe::Link& l) {
        switch (l.kind) {
        case probe::LinkKind::Wired: return i18n::fmt("LAN  ·  {} Mbit/s", l.linkMbps);
        case probe::LinkKind::Wifi: {
            std::string out = i18n::tr("Wi-Fi");
            if (!l.band.empty()) out += "  ·  " + l.band;
            if (l.channel) out += i18n::fmt("  ·  channel {}", l.channel);
            if (l.signal >= 0) out += std::format("  ·  {} % ({} dBm)", l.signal, l.rssi);
            if (l.linkMbps) out += std::format("  ·  {} Mbit/s", l.linkMbps);
            return out;
        }
        case probe::LinkKind::Other: return i18n::tr("Connection: ") + l.adapter;
        default: return i18n::tr("Connection type unknown");
        }
    }

    void drawGraph(ImDrawList* dl, ImVec2 p, ImVec2 size, const probe::Snapshot& snap, float s) {
        dl->AddRectFilled(p, p + size, IM_COL32(0, 0, 0, 60), 4 * s);
        float top = std::max(40.f, std::min(snap.max * 1.25f, 400.f));
        size_t n = snap.history.size();
        if (n < 2) return;
        float slot = std::clamp(size.x / float(n), 1.5f, 5.f * s);
        size_t shown = std::min(n, size_t(size.x / slot));
        float bar = slot > 3.f ? slot - std::max(1.f, 0.5f * s) : slot;
        float x0 = p.x + size.x - slot * float(shown);
        for (size_t k = 0; k < shown; k++) {
            float v = snap.history[n - shown + k];
            float x = x0 + slot * float(k);
            if (v < 0.f) {
                dl->AddRectFilled({x, p.y + 2 * s}, {x + bar, p.y + size.y - 2 * s}, ImGui::GetColorU32(withAlpha(poor_.color, 0.6f)));
                continue;
            }
            float h = std::max(size.y * std::min(v / top, 1.f), 1.f);
            ImVec4 c = rampColor(v, limits().pingFair, limits().pingPoor, good_.color, fair_.color, poor_.color);
            dl->AddRectFilled({x, p.y + size.y - h}, {x + bar, p.y + size.y}, ImGui::GetColorU32(c));
        }
    }

    Setting& labels_ = toggleSetting("metricLabels", "Show metric labels", true);
    Setting& labelSide_ = choice("metricLabelSide", "Label position", {"Before value", "After value"});
    Setting& units_ = toggleSetting("metricUnits", "Show metric units", true);
    Setting& separator_ = textSetting("metricSeparator", "Metric separator", "  ·  ");
    Setting& status_ = toggleSetting("status", "Traffic light", true);
    Setting& ping_ = toggleSetting("ping", "Ping", true);
    Setting& jitter_ = toggleSetting("jitter", "Jitter", true);
    Setting& loss_ = toggleSetting("loss", "Packet loss", true);
    Setting& range_ = toggleSetting("range", "Min / avg / max", false);
    Setting& link_ = toggleSetting("link", "Connection (Wi-Fi / LAN)", true);
    Setting& power_ = toggleSetting("power", "Power saving notice", true);
    Setting& reason_ = toggleSetting("reason", "Show reason", true);
    Setting& tipsOn_ = toggleSetting("tipsOn", "Show tips", false);
    Setting& tips_ = intSlider("tips", "Number of tips", 2, 1, 4);
    Setting& graph_ = toggleSetting("graph", "Ping history", true);
    Setting& graphHeight_ = slider("graphHeight", "History height", 36.f, 16.f, 100.f, "%.0f");
    Setting& toast_ = toggleSetting("toast", "Notice on a poor connection", true);
    Setting& good_ = colorSetting("good", "Color good", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& fair_ = colorSetting("fair", "Color medium", {1.f, 0.82f, 0.49f, 1.f});
    Setting& poor_ = colorSetting("poor", "Color bad", {1.f, 0.40f, 0.45f, 1.f});
    Setting& advanced_ = toggleSetting("advanced", "Advanced settings", false);
    Setting& method_ = choice("method", "Measurement method", {"ICMP-Ping", "RakNet-Ping (UDP)"});
    Setting& interval_ = slider("interval", "Measurement interval (ms)", 1000.f, 250.f, 5000.f, "%.0f");
    Setting& window_ = slider("window", "Samples in the window", 60.f, 20.f, 200.f, "%.0f");
    Setting& host_ = textSetting("host", "Custom target (empty = server)", "");
    Setting& port_ = intSlider("port", "UDP-Port", 19132, 1, 65535);
    Setting& pingFair_ = slider("pingFair", "Ping medium from (ms)", 60.f, 10.f, 300.f, "%.0f");
    Setting& pingPoor_ = slider("pingPoor", "Ping bad from (ms)", 150.f, 30.f, 600.f, "%.0f");
    Setting& jitterFair_ = slider("jitterFair", "Jitter medium from (ms)", 6.f, 1.f, 50.f, "%.0f");
    Setting& jitterPoor_ = slider("jitterPoor", "Jitter bad from (ms)", 20.f, 2.f, 100.f, "%.0f");
    Setting& lossPoor_ = slider("lossPoor", "Loss bad from (%)", 3.f, 0.5f, 20.f, "%.1f");
    quality::Grade last_ = quality::Grade::Unknown;
};
