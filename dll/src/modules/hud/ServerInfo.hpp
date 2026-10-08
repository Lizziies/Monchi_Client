#pragma once

#include "I18n.hpp"
#include "gui/Gui.hpp"
#include "modules/HudModule.hpp"
#include "server/Rules.hpp"
#include "sdk/Game.hpp"

class ServerInfo : public TextHud {
public:
    bool defaultEnabled() const override { return true; }

    ServerInfo() : TextHud("Server Display", "Shows which server you are on.", {"hud-self"}, {0.005f, 0.242f}) {
        sub("Info displays");
    }

    void onRender(ImDrawList* dl) override {
        TextHud::onRender(dl);
    }

protected:
    std::string label() const override { return "Server"; }

    std::string value() override {
        auto st = rules::status();
        if (st.server.empty()) return game::state().world.name.empty() ? i18n::tr("Local world") : game::state().world.name;
        if (mode_.i == 1) return st.host;
        if (mode_.i == 2) return st.ip;
        return st.server;
    }

private:
    Setting& mode_ = choice("mode", "Display", {"Name", "Address", "IP"});
};
