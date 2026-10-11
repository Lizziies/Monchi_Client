#pragma once

#include "core/Config.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Text.hpp"
#include "sdk/Particles.hpp"

#include <algorithm>
#include <format>

// Lists the particle effects the game creates while you play and lets each one be switched off. Client side only:
// nothing is sent, the effect is simply not created on this screen.
class ParticleFilter : public Module {
public:
    ParticleFilter()
        : Module("Particle Filter", "Switches single particle effects off: critical hits, totem, a server's own effects. Play for a moment, then pick from the list of what the game has shown.",
                 Category::Visual, {"cosmetic"}, {"ParticleEffect"}) {
        sub("World");
        list_.prefill = "minecraft:critical_hit_emitter, minecraft:magic_critical_hit_emitter";
    }

    void onEnable() override { particles::use(true); }
    void onDisable() override { particles::use(false); }

    void onFrame() override {
        particles::use(true);
        if (applied_ == list_.text && appliedAll_ == all_.b) return;
        applied_ = list_.text;
        appliedAll_ = all_.b;
        particles::block(names(), all_.b);
    }

    void drawSettings() override {
        ImGui::Spacing();
        if (!particles::hooked()) {
            ImGui::TextDisabled("%s", i18n::tr("Waiting for the game: the list fills once you are in a world."));
            return;
        }
        auto seen = particles::seen();
        ImGui::TextDisabled("%s", i18n::fmt("Effects kept from the screen so far: {}", particles::blockedTotal()).c_str());
        if (seen.empty()) {
            ImGui::TextDisabled("%s", i18n::tr("No effect seen yet. Hit something or walk around, then look here again."));
            return;
        }
        ImGui::TextDisabled("%s", i18n::tr("Seen in this session (click to switch an effect off or on):"));
        auto off = names();
        int shown = 0;
        for (auto& s : seen) {
            if (shown++ >= 40) break;
            bool blocked = std::find(off.begin(), off.end(), s.name) != off.end();
            std::string label = std::format("{}  {}  ·  {}x###{}", blocked ? "[off]" : "[on] ", s.name, s.asked, s.name);
            ImGui::PushStyleColor(ImGuiCol_Text, blocked ? theme::current().warn : theme::current().text);
            if (ImGui::Selectable(label.c_str())) toggle(s.name, blocked);
            ImGui::PopStyleColor();
        }
    }

    std::string proof() const override {
        if (!particles::hooked()) return "the particle function is not hooked, 0 times";
        return std::format("{} kinds of effect seen, {} effects kept from being created", particles::seen().size(), particles::blockedTotal());
    }

private:
    std::vector<std::string> names() const {
        std::vector<std::string> out;
        for (auto& part : text::split(list_.text, ',')) {
            auto a = part.find_first_not_of(' '), b = part.find_last_not_of(' ');
            if (a != std::string::npos) out.push_back(part.substr(a, b - a + 1));
        }
        return out;
    }

    void toggle(const std::string& name, bool wasBlocked) {
        auto list = names();
        if (wasBlocked) std::erase(list, name);
        else list.push_back(name);
        std::string joined;
        for (auto& n : list) joined += (joined.empty() ? "" : ", ") + n;
        list_.text = joined;
        config::markDirty();
    }

    Setting& all_ = toggleSetting("all", "Switch every named effect off", false);
    Setting& list_ = textSetting("list", "Effects that are off (comma)", "");
    std::string applied_ = "\x01";
    bool appliedAll_ = false;
};
