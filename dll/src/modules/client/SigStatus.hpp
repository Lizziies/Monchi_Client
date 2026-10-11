#pragma once

#include "gui/Theme.hpp"
#include "modules/Manager.hpp"
#include "modules/Module.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"
#include "sig/Sigs.hpp"

class SigStatus : public Module {
public:
    SigStatus()
        : Module("Game Support",
                 "Shows which modules run on this version. Also home of the demo data switch.",
                 Category::Performance, {"performance"}) {
        sub("Diagnostics");
        demoServer_.visible = [this] { return demo_.b; };
    }

    bool alwaysOn() const override { return true; }

    void onFrame() override {
        static const char* servers[] = {"", "The Hive", "Zeqa"};
        game::setDemoServer(servers[demoServer_.i % 3]);
        if (game::demo() == demo_.b) return;
        game::setDemo(demo_.b);
        modules::refreshSigs();
    }

    void drawSettings() override {
        auto& t = theme::current();
        auto st = sigs::stats();
        ImGui::Spacing();
        ImGui::TextDisabled(i18n::tr("Version %s  ·  %d of %d signatures  ·  Source %s"), st.gameVersion.c_str(), st.found, st.total, st.source.c_str());
        if (demo_.b) ImGui::TextColored(t.warn, i18n::tr("Demo data on: all game modules show simulated values."));

        int missing = 0, ok = 0;
        for (auto& m : modules::all()) {
            if (m->sigs().empty()) continue;
            if (m->missingSigs().empty()) ok++;
            else missing++;
        }
        ImGui::Text(i18n::tr("%d game modules ready, %d waiting for signatures"), ok, missing);

        if (ImGui::CollapsingHeader(i18n::tr("Modules and their signatures"))) {
            for (auto& m : modules::all()) {
                if (m->sigs().empty()) continue;
                bool good = m->missingSigs().empty();
                ImGui::TextColored(good ? t.ok : t.textDim, "%s %s", good ? "●" : "○", m->name().c_str());
                if (!good) {
                    std::string list;
                    for (auto& s : m->missingSigs()) list += (list.empty() ? "" : ", ") + s;
                    ImGui::SameLine();
                    ImGui::TextDisabled(i18n::tr("missing: %s"), list.c_str());
                }
            }
        }
        if (ImGui::CollapsingHeader(i18n::tr("Effect channels"))) {
            for (int i = 0; i < int(fx::Id::Count); i++) {
                auto id = fx::Id(i);
                auto r = fx::report(id);
                bool have = sigs::address(fx::info(id).sig) != 0;
                ImGui::TextColored(have ? t.ok : t.textDim, "%s %s", have ? "●" : "○", fx::info(id).sig);
                ImGui::SameLine();
                ImGui::TextDisabled("%s%s", i18n::tr(fx::info(id).label), r.requested ? i18n::tr("  ·  requested") : "");
            }
        }
    }

private:
    Setting& demo_ = toggleSetting("demo", "Demo data (simulated game values)", false);
    Setting& demoServer_ = choice("demoServer", "Demo server", {"Automatic", "The Hive", "Zeqa"});
};
