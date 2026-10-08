#pragma once

#include "hook/Dx.hpp"
#include "modules/Module.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "modules/server/ServerChat.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"
#include "sig/Sigs.hpp"

#include <windows.h>

#include <algorithm>
#include <vector>
#include <utility>

class CrystalOptimizer : public Module {
public:
    CrystalOptimizer()
        : Module("Crystal Optimizer",
                 "A hit end crystal disappears at once instead of waiting for the server. Client side only, sends nothing extra.",
                 Category::Pvp, {"info-others", "timing"}) {
        sub("Crystal PvP");
        markRisky("Some servers count crystal tweaks as an advantage. Only use it where the server rules allow it.");
        require(need::target | need::combat, need::sigs({"Target", "HideEntities"}));
    }

    void onFrame() override {
        uint64_t now = GetTickCount64();
        std::erase_if(actors_, [now](const auto& item) { return item.second <= now; });
        for (auto& e : game::events()) {
            if (e.kind == game::EventKind::Confirm && e.value > 0.f && e.value < 1500.f) serverMs_ = serverMs_ > 0.f ? serverMs_ * 0.8f + e.value * 0.2f : e.value;
            if (e.kind != game::EventKind::Hit || !e.crystal || !e.actor) continue;
            float keep = keepFor();
            game::hide(e.actor, keep / 1000.f);
            auto it = std::find_if(actors_.begin(), actors_.end(), [&](const auto& item) { return item.first == e.actor; });
            uint64_t until = now + uint64_t(keep);
            if (it == actors_.end()) actors_.emplace_back(e.actor, until);
            else it->second = until;
            hidden_++;
        }
    }

    void onDisable() override {
        for (auto [actor, until] : actors_) game::unhide(actor);
        actors_.clear();
    }

    std::string proof() const override { return std::format("crystals hit and hidden {} times, hidden right now {}", hidden_, game::hidden()); }

    void drawSettings() override {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", i18n::fmt("Crystals hidden by you: {}  ·  hidden right now: {}", hidden_, game::hidden()).c_str());
    }

private:
    // The crystal stays hidden until the server removes it, at most this long. A fixed time ran out on a slow server
    // before its answer came, and the crystal came back for a moment; the time a hit takes to be confirmed by the
    // server (hurt animation or health) is the measure of how long that answer takes.
    float keepFor() const {
        float wait = serverMs_ > 0.f ? serverMs_ * 1.5f + 150.f : 0.f;
        return std::clamp(wait, keep_.f, 2000.f);
    }

    Setting& keep_ = slider("keep", "Keep it hidden for at least (ms)", 500.f, 100.f, 1500.f, "%.0f ms");
    float serverMs_ = 0.f;
    int hidden_ = 0;
    std::vector<std::pair<uintptr_t, uint64_t>> actors_;
};

class KillCleanup : public Module {
public:
    KillCleanup()
        : Module("Kill Cleanup", "Hides a player right after you kill them until the game catches up. Client side only.",
                 Category::Pvp, {"info-others", "timing"}) {
        sub("Hit feedback");
        markRisky("Some servers count this as an advantage. Only use it where the server rules allow it.");
        require(0, {"KillEvents", "HideEntities"});
        words_.visible = [this] { return chat_.b && chatOk(); };
        chat_.visible = [this] { return chatOk(); };
    }

    void onFrame() override {
        auto& st = game::state();
        for (auto& e : game::events()) {
            if (e.kind == game::EventKind::Kill && own_.b && st.combat.lastActor) hide(st.combat.lastActor, e.text);
            if (e.kind == game::EventKind::Chat && chat_.b && chatOk()) deadInChat(e.text, st);
        }
    }

    void drawSettings() override {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", i18n::fmt("Hidden by you: {}  ·  hidden right now: {}", hidden_, game::hidden()).c_str());
    }

    std::string proof() const override { return std::format("players hidden after a kill {} times", hidden_); }

private:
    static bool chatOk() { return game::demo() || (sigs::address("ChatEvents") && sigs::address("ActorList")); }

    void hide(uintptr_t actor, const std::string&) {
        game::hide(actor, keep_.f);
        hidden_++;
    }

    void deadInChat(const std::string& raw, const game::State& st) {
        if (srv::typed(raw)) return;
        std::string line = text::lower(text::strip(raw));
        bool died = false;
        for (auto& w : text::split(text::lower(words_.text), ','))
            if (line.find(w) != std::string::npos) died = true;
        if (!died) return;
        for (auto& o : st.others) {
            if (!o.isPlayer || o.name.empty() || o.name == st.player.name) continue;
            if (line.find(text::lower(o.name)) == line.npos) continue;
            if (line.find(text::lower(o.name)) > line.size() / 2) continue;
            hide(o.id, o.name);
        }
    }

    Setting& own_ = toggleSetting("own", "Hide players you kill", true);
    Setting& chat_ = toggleSetting("chat", "Hide players who die according to chat", false);
    Setting& words_ = textSetting("words", "Death words (comma)", "was killed, was slain, was shot, was eliminated, was blown up, died");
    Setting& keep_ = slider("keep", "Keep them hidden for (s)", 2.f, 0.5f, 6.f, "%.1f s");
    int hidden_ = 0;
};
