#pragma once

#include "gui/Notify.hpp"
#include "modules/Module.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <format>

class MatchSummary : public Module {
public:
    MatchSummary()
        : Module("Match Summary", "Summary when you leave a server: duration, hits, combo, K/D.", Category::Server,
                 {"hud-self"}) {
        sub("Statistics");
        require(need::combat, need::sigs({"LocalPlayer"}));
    }

    void onServer(const ServerEvent& ev) override {
        if (ev.joined) {
            start_ = ui::time();
            game::resetCombat();
            return;
        }
        auto& c = game::state().combat;
        std::string body = i18n::fmt("Duration {}", text::clock(float(ui::time() - start_)));
        if (hits_.b) body += i18n::fmt("  ·  {} hits", c.hits);
        if (accuracy_.b && c.swings > 0) body += std::format(" ({:.0f}%)", 100.f * c.hits / c.swings);
        if (combo_.b) body += i18n::fmt("  ·  Combo {}", c.bestCombo);
        if (kd_.b) body += std::format("  ·  K/D {}/{}", c.kills, c.deaths);
        notify::push(i18n::tr("Summary: ") + ev.name, body, notify::Kind::Info, 10.f);
    }

private:
    Setting& hits_ = toggleSetting("hits", "Hits", true);
    Setting& accuracy_ = toggleSetting("accuracy", "Hit rate", true);
    Setting& combo_ = toggleSetting("combo", "Combo record", true);
    Setting& kd_ = toggleSetting("kd", "Kills and deaths", true);
    double start_ = 0.0;
};
