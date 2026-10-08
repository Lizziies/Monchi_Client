#pragma once

#include "I18n.hpp"
#include "modules/HudModule.hpp"
#include "system/Music.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

class MusicControl : public HudModule {
public:
    MusicControl()
        : HudModule("Music", "Shows what Spotify, Apple Music, Deezer or Amazon Music plays and lets you control it with keys, without leaving the game.",
                    {"hud-self"}, {0.845f, 0.005f}) {
        sub("Music");
        playerSide_.visible = separator_.visible = [this] { return showPlayer_.b && showTitle_.b; };
        iconSide_.visible = iconSize_.visible = iconGap_.visible = [this] { return icon_.b; };
        scrollSpeed_.visible = [this] { return scroll_.b; };
    }

    void onEnable() override { publishKeys(); music::use(true); }
    void onFrame() override { publishKeys(); }
    void onDisable() override { music::use(false); }

    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat || !ev.vk) return;
        for (int i = 0; i < 6; ++i) {
            int key = inputKeys_[i].load(std::memory_order_relaxed);
            if (!key || ev.vk != key) continue;
            music::send(music::Action(i));
            ev.cancel = true;
            return;
        }
    }

    void drawSettings() override {
        auto t = music::now();
        ImGui::Spacing();
        if (!t.found) ImGui::TextDisabled("%s", i18n::tr("No music app found. Start Spotify, Apple Music, Deezer or Amazon Music."));
        else ImGui::TextDisabled(i18n::tr("Found: %s"), t.player.c_str());
        ImGui::TextDisabled("%s", i18n::tr("Keys control whichever player Windows treats as active. The volume keys change the volume of the music app only, not of Minecraft."));
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        if (!showTitle_.b && !showPlayer_.b && !icon_.b) return {};
        auto t = music::now();
        std::string line;
        if (showTitle_.b) line = !t.found ? std::string(i18n::tr("No music")) : t.title.empty() ? t.player : t.title;
        if (showPlayer_.b && t.found && line != t.player) {
            if (line.empty()) line = t.player;
            else if (playerSide_.i == 0) line = t.player + separator_.text + line;
            else line += separator_.text + t.player;
        }
        float maxW = maxWidth_.f * s;
        ImVec2 full = line.empty() ? ImVec2{} : textSize(s, line);
        float x = 0.f;
        if (scroll_.b && full.x > maxW) {
            float over = full.x - maxW;
            x = -std::fmod(float(ImGui::GetTime()) * scrollSpeed_.f * s, over + 60.f * s);
            x = std::max(x, -over);
        }
        float icon = icon_.b ? iconSize_.f * s : 0.f;
        float gap = icon_.b && !line.empty() ? iconGap_.f * s : 0.f;
        float textW = std::min(full.x, maxW);
        float textX = icon_.b && iconSide_.i == 0 ? icon + gap : 0.f;
        float height = std::max(full.y, icon);
        float textY = (height - full.y) * 0.5f;
        if (!line.empty()) {
            dl->PushClipRect(o + ImVec2(textX, textY), o + ImVec2(textX + maxW, textY + full.y + 2), true);
            drawText(dl, o + ImVec2(textX + x, textY), s, line, textColor());
            dl->PopClipRect();
        }
        if (icon_.b) {
            float iconX = iconSide_.i == 0 ? 0.f : textW + gap;
            drawNote(dl, o + ImVec2(iconX + icon * 0.5f, height * 0.5f), icon * 0.5f, t.playing);
        }
        return {icon + gap + textW, height};
    }

private:
    void publishKeys() {
        Setting* keys[] = {&playKey_, &nextKey_, &prevKey_, &upKey_, &downKey_, &muteKey_};
        for (int i = 0; i < 6; ++i) inputKeys_[i].store(keys[i]->i, std::memory_order_relaxed);
    }
    std::atomic<int> inputKeys_[6]{};
    void drawNote(ImDrawList* dl, ImVec2 c, float r, bool playing) {
        ImU32 col = playing ? accentColor() : textColor();
        if (playing) {
            dl->AddTriangleFilled({c.x - r * 0.5f, c.y - r}, {c.x - r * 0.5f, c.y + r}, {c.x + r, c.y}, col);
        } else {
            dl->AddRectFilled({c.x - r * 0.7f, c.y - r}, {c.x - r * 0.15f, c.y + r}, col);
            dl->AddRectFilled({c.x + r * 0.15f, c.y - r}, {c.x + r * 0.7f, c.y + r}, col);
        }
    }

    Setting& showTitle_ = toggleSetting("showTitle", "Show song title", true);
    Setting& showPlayer_ = toggleSetting("showPlayer", "Show music app", false);
    Setting& playerSide_ = choice("playerSide", "Music app position", {"Before value", "After value"});
    Setting& separator_ = textSetting("separator", "Metric separator", " · ");
    Setting& icon_ = toggleSetting("icon", "Show playback icon", true);
    Setting& iconSide_ = choice("iconSide", "Icon position", {"Left", "Right"});
    Setting& iconSize_ = slider("iconSize", "Icon size", 14.f, 6.f, 40.f, "%.0f");
    Setting& iconGap_ = slider("iconGap", "Icon gap", 6.f, 0.f, 24.f, "%.0f");
    Setting& scroll_ = toggleSetting("scroll", "Scroll long titles", true);
    Setting& scrollSpeed_ = slider("scrollSpeed", "Scroll speed", 28.f, 0.f, 100.f, "%.0f");
    Setting& maxWidth_ = slider("maxWidth", "Width", 260.f, 120.f, 600.f, "%.0f");
    Setting& playKey_ = keySetting("playKey", "Play / pause", 0);
    Setting& nextKey_ = keySetting("nextKey", "Next song", 0);
    Setting& prevKey_ = keySetting("prevKey", "Previous song", 0);
    Setting& upKey_ = keySetting("upKey", "Music volume up", 0);
    Setting& downKey_ = keySetting("downKey", "Music volume down", 0);
    Setting& muteKey_ = keySetting("muteKey", "Mute music", 0);
};
