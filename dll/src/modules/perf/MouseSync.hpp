#pragma once

#include "I18n.hpp"
#include "hook/GameInput.hpp"
#include "modules/Module.hpp"
#include "sdk/Effects.hpp"
#include "sig/Sigs.hpp"
#include "system/Mice.hpp"

#include <json.hpp>

#include <cmath>
#include <format>

class MouseSync : public Module {
public:
    MouseSync()
        : Module("Mouse Sync",
                 "Detects your mouse, measures its polling rate and keeps your aim the same when you switch mice or change DPI.",
                 Category::Performance, {"performance", "input"}) {
        sub("Input");
        profiles_.hidden = true;
        referenceDpi_.hidden = true;
    }

    void onEnable() override { mice::use(true); }
    void onDisable() override { mice::use(false); }

    void onFrame() override {
        if (!keepAim_.b || referenceDpi_.i <= 0) return;
        int dpi = effectiveDpi();
        if (dpi <= 0) return;
        // the same two ways as Zoom and Sens Multiplier: the game's own value where it is bound, otherwise the mouse
        // movement the game reads
        float k = float(referenceDpi_.i) / float(dpi);
        if (fx::available(fx::Id::Sensitivity)) fx::scale(fx::Id::Sensitivity, k);
        else gameinput::scaleMouse(k);
    }

    std::string proof() const override {
        int dpi = effectiveDpi();
        if (!keepAim_.b || referenceDpi_.i <= 0 || dpi <= 0) return "keeping the aim is off or no mouse profile yet, 0 times";
        return std::format("mouse movement scaled by {:.2f} through {}", float(referenceDpi_.i) / float(dpi),
                           fx::available(fx::Id::Sensitivity) ? "the game's sensitivity" : gameinput::active() ? "the game's input readings" : "nothing (the game's input is not hooked), 0 times");

    }

    void drawSettings() override {
        auto snap = mice::snapshot();
        ImGui::Spacing();
        if (snap.devices.empty()) ImGui::TextDisabled("%s", i18n::tr("No mouse found yet. Move your mouse."));
        for (auto& d : snap.devices) {
            std::string rate = d.hz ? i18n::fmt("{} Hz", d.hz) : std::string(i18n::tr("not moved yet"));
            int dpi = savedDpi(d.key);
            std::string dpiText = dpi ? i18n::fmt("{} DPI", dpi) : std::string(i18n::tr("DPI unknown"));
            ImGui::TextColored(d.active ? ImVec4(0.55f, 0.91f, 0.69f, 1.f) : ImVec4(0.7f, 0.7f, 0.75f, 1.f), "%s %s  ·  %s  ·  %s  ·  %s",
                               d.active ? "●" : "○", d.name.c_str(), d.vendor.c_str(), rate.c_str(), dpiText.c_str());
        }
        if (!snap.software.empty()) {
            std::string list;
            for (auto& s : snap.software) list += (list.empty() ? "" : ", ") + s;
            ImGui::TextDisabled(i18n::tr("Mouse software running: %s. Your DPI is set there."), list.c_str());
        }

        std::string key = mice::activeKey();
        ImGui::Spacing();
        if (!calibrating_) {
            if (ImGui::Button(i18n::tr("Measure DPI"))) {
                mice::calibrateBegin();
                calibrating_ = true;
            }
        } else {
            ImGui::TextWrapped(i18n::tr("Put the mouse at the left end of a ruler, then move it exactly %.0f cm to the right in a straight line and press Done."), distance_.f);
            if (ImGui::Button(i18n::tr("Done"))) {
                long counts = mice::calibrateCounts();
                calibrating_ = false;
                int dpi = int(std::lround(double(counts) / (double(distance_.f) / 2.54) / 50.0)) * 50;
                if (!key.empty() && dpi >= 100) store(key, dpi);
            }
        }
        if (!key.empty() && savedDpi(key) > 0) {
            ImGui::SameLine();
            if (ImGui::Button(i18n::tr("Use as my reference"))) referenceDpi_.i = effectiveDpi();
        }
        if (referenceDpi_.i > 0) ImGui::TextDisabled(i18n::tr("Reference: %d DPI (the DPI your in-game sensitivity was set for)."), referenceDpi_.i);

        for (auto& d : snap.devices) {
            if (!d.active) continue;
            if (d.hz > 0 && d.hz < 500) ImGui::TextColored(ImVec4(1.f, 0.82f, 0.49f, 1.f), i18n::tr("%d Hz is low. Raise the polling rate to 1000 Hz or more in your mouse software."), d.hz);
            if (d.hz >= 4000) ImGui::TextColored(ImVec4(1.f, 0.82f, 0.49f, 1.f), "%s", i18n::tr("Very high polling rates can cost FPS in Minecraft. If it stutters, try 1000 to 2000 Hz."));
        }
        if (keepAim_.b && !sigs::address(fx::sig(fx::Id::Sensitivity)) && !gameinput::active())
            ImGui::TextColored(ImVec4(1.f, 0.82f, 0.49f, 1.f), "%s", i18n::tr("Keeping your aim needs game data for this version. Detection and measuring work already."));
    }

private:
    int savedDpi(const std::string& key) const {
        if (cachedProfiles_ != profiles_.text) {
            cachedProfiles_ = profiles_.text;
            dpiProfiles_ = nlohmann::json::parse(cachedProfiles_, nullptr, false);
        }
        if (!dpiProfiles_.is_object()) return 0;
        auto entry = dpiProfiles_.find(key);
        if (entry == dpiProfiles_.end() || !entry->is_object()) return 0;
        auto dpi = entry->find("dpi");
        if (dpi == entry->end() || !dpi->is_number_integer()) return 0;
        if (*dpi < 100 || *dpi > 32000) return 0;
        return dpi->get<int>();
    }

    void store(const std::string& key, int dpi) {
        auto j = nlohmann::json::parse(profiles_.text, nullptr, false);
        if (!j.is_object()) j = nlohmann::json::object();
        j[key] = {{"dpi", dpi}};
        profiles_.text = j.dump();
    }

    int effectiveDpi() const {
        if (manualDpi_.i > 0) return manualDpi_.i;
        std::string key = mice::activeKey();
        return key.empty() ? 0 : savedDpi(key);
    }

    mutable std::string cachedProfiles_;
    mutable nlohmann::json dpiProfiles_;
    bool calibrating_ = false;
    Setting& keepAim_ = toggleSetting("keepAim", "Keep my aim when the DPI changes", false);
    Setting& manualDpi_ = intSlider("manualDpi", "DPI (0 = use the measured one)", 0, 0, 32000);
    Setting& distance_ = slider("distance", "Measuring distance (cm)", 10.f, 5.f, 30.f, "%.0f cm");
    Setting& profiles_ = textSetting("profiles", "Profiles", "");
    Setting& referenceDpi_ = intSlider("reference", "Reference DPI", 0, 0, 32000);
};
