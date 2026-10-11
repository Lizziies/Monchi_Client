#pragma once

#include "gui/Notify.hpp"
#include "modules/HudModule.hpp"
#include "render/Ui.hpp"

#include <windows.h>

#include <format>
#include <chrono>
#include <mutex>
#include <vector>

class Stopwatch : public TextHud {
public:
    Stopwatch() : TextHud("Stopwatch", "Stopwatch with its own start/stop and reset keys.", {"hud-self"}, {0.005f, 0.146f}) {
        sub("Timer");
        for (Setting* st : {&runningColor_, &pausedColor_}) st->visible = [this] { return stateColors_.b; };
    }

    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat) return;
        int action = ev.vk == startStop_.i ? 1 : ev.vk == reset_.i ? 2 : 0;
        if (!action) return;
        std::lock_guard lock(commandsMutex_);
        commands_.push_back({action, now()});
    }

    void onFrame() override {
        std::vector<Command> commands;
        {
            std::lock_guard lock(commandsMutex_);
            commands.swap(commands_);
        }
        for (auto command : commands) {
            if (command.action == 1) {
                if (running_) accumulated_ += command.at - started_;
                else started_ = command.at;
                running_ = !running_;
            } else {
                if (running_ && !resetRunning_.b) {
                    notify::push(i18n::tr("Stopwatch"), i18n::tr("Stop it first, or allow resetting while it runs."), notify::Kind::Info, 2.f);
                    continue;
                }
                running_ = false;
                accumulated_ = 0;
            }
        }
    }

    void onDisable() override {
        std::lock_guard lock(commandsMutex_);
        commands_.clear();
    }

protected:
    std::string label() const override { return "Time"; }

    std::string value() override {
        double total = accumulated_ + (running_ ? now() - started_ : 0);
        int ms = int(total * 1000) % 1000;
        int sec = int(total) % 60;
        int min = int(total) / 60 % 60;
        int hours = int(total) / 3600;
        std::string frac = precision_.i == 2 ? std::format("{:03}", ms) : precision_.i == 1 ? std::format("{:02}", ms / 10) : std::to_string(ms / 100);
        if (hours > 0) return std::format("{}:{:02}:{:02}.{}", hours, min, sec, frac);
        return std::format("{:02}:{:02}.{}", min, sec, frac);
    }

    ImU32 valueColor() const override {
        if (stateColors_.b && running_) return ImGui::GetColorU32(runningColor_.color);
        if (stateColors_.b && accumulated_ > 0) return ImGui::GetColorU32(pausedColor_.color);
        return textColor();
    }

private:
    static double now() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    struct Command { int action; double at; };
    std::mutex commandsMutex_;
    std::vector<Command> commands_;
    Setting& startStop_ = keySetting("startStop", "Start / stop", VK_F6);
    Setting& reset_ = keySetting("reset", "Reset", VK_F7);
    Setting& resetRunning_ = toggleSetting("resetRunning", "Allow resetting while it runs", true);
    Setting& precision_ = choice("precision", "Precision", {"Tenths", "Hundredths", "Milliseconds"});
    Setting& stateColors_ = toggleSetting("stateColors", "Color by state", false);
    Setting& runningColor_ = colorSetting("runningColor", "Running", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& pausedColor_ = colorSetting("pausedColor", "Paused", {1.f, 0.65f, 0.f, 1.f});
    bool running_ = false;
    double started_ = 0;
    double accumulated_ = 0;
};
