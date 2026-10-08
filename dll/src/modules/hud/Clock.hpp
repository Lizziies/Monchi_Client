#pragma once

#include "modules/HudModule.hpp"
#include "modules/common/Needs.hpp"
#include "sdk/Game.hpp"

#include <windows.h>

#include <format>

class Clock : public TextHud {
public:
    Clock() : TextHud("Clock", "Shows the real time, the game time or both, with date options.", {"hud-self"}, {0.005f, 0.114f}) {
        sub("Info displays");
        dateFormat_.visible = [this] { return date_.b; };
        weekday_.visible = [this] { return date_.b; };
        mode_.visible = [] { return need::have("WorldTime"); };
        gameDay_.visible = [this] { return need::have("WorldTime") && mode_.i != 0; };
    }

protected:
    std::string value() override {
        std::string out;
        int mode = need::have("WorldTime") ? mode_.i : 0;
        if (mode != 1) out = real();
        if (mode != 0) {
            std::string game = gameTime();
            out += out.empty() ? game : separator_.text + game;
        }
        return out;
    }

private:
    std::string clockText(int h, int m, int s) const {
        const char* suffix = "";
        if (twelve_.b) {
            suffix = h >= 12 ? " PM" : " AM";
            h = h % 12 == 0 ? 12 : h % 12;
        }
        return seconds_.b ? std::format("{:02}:{:02}:{:02}{}", h, m, s, suffix) : std::format("{:02}:{:02}{}", h, m, suffix);
    }

    std::string real() const {
        SYSTEMTIME t;
        GetLocalTime(&t);
        std::string out;
        if (date_.b) {
            static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            switch (dateFormat_.i) {
            case 1: out = std::format("{:02}/{:02}/{}", t.wMonth, t.wDay, t.wYear); break;
            case 2: out = std::format("{}-{:02}-{:02}", t.wYear, t.wMonth, t.wDay); break;
            default: out = std::format("{:02}.{:02}.{}", t.wDay, t.wMonth, t.wYear); break;
            }
            if (weekday_.b) out = std::string(i18n::tr(days[t.wDayOfWeek % 7])) + " " + out;
            out += "  ";
        }
        return out + clockText(t.wHour, t.wMinute, t.wSecond);
    }

    std::string gameTime() const {
        auto& w = game::state().world;
        int ticks = ((w.time % 24000) + 24000) % 24000;
        int hours = (ticks / 1000 + 6) % 24, minutes = (ticks % 1000) * 60 / 1000, secs = (ticks % 1000) * 3600 / 1000 % 60;
        std::string out = clockText(hours, minutes, secs);
        if (gameDay_.b) out = i18n::fmt("Day {}", w.day) + "  " + out;
        return out;
    }

    Setting& mode_ = choice("mode", "Show", {"Real time", "Game time", "Both"});
    Setting& separator_ = textSetting("separator", "Separator", "  ·  ");
    Setting& twelve_ = toggleSetting("12h", "12-hour format", false);
    Setting& seconds_ = toggleSetting("seconds", "Seconds", false);
    Setting& date_ = toggleSetting("date", "Date", false);
    Setting& dateFormat_ = choice("dateFormat", "Date format", {"DD.MM.YYYY", "MM/DD/YYYY", "YYYY-MM-DD"});
    Setting& weekday_ = toggleSetting("weekday", "Weekday", false);
    Setting& gameDay_ = toggleSetting("gameDay", "Game day", false);
};
