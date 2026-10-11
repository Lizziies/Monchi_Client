#pragma once

#include "gui/Theme.hpp"
#include "hook/Input.hpp"
#include "modules/HudModule.hpp"
#include "modules/post/PostFx.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"

#include <windows.h>

#include <array>
#include <format>

class Keystrokes : public HudModule {
public:
    bool defaultEnabled() const override { return true; }

    Keystrokes()
        : HudModule("Keystrokes", "Shows WASD, space and the mouse buttons live. Glow, borders, custom texts, spacing and animation speed.", {"hud-self"},
                    {0.26f, 0.68f}) {
        sub("Info displays");
        cpsLabel_.hint = "A custom format determines the label position and spacing.";
        background_.b = false;
        cpsFormat_.visible = rmbCpsFormat_.visible = cpsTextScale_.visible = cpsLabel_.visible = [this] { return cpsInside_.b; };
        glowColor_.visible = pressedGlowSize_.visible = [this] { return glowPressed_.b; };
        idleGlowColor_.visible = idleGlowSize_.visible = [this] { return glowIdle_.b; };
        mouseLabels_.visible = [this] { return mouse_.b; };
        keyBorderWidth_.visible = [this] { return keyBorder_.b; };
        keyBorderColor_.visible = [this] { return keyBorder_.b; };
        keyBorderPressed_.visible = [this] { return keyBorder_.b; };
        keyShadowColor_.visible = [this] { return keyShadow_.b; };
        spaceWidth_.visible = [this] { return space_.b; };
        spaceHeight_.visible = [this] { return space_.b; };
        spaceText_.visible = [this] { return space_.b; };
        lmbText_.visible = [this] { return mouse_.b; };
        rmbText_.visible = [this] { return mouse_.b; };
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        float k = keySize_.f * s, gap = spacing_.f * s;
        float row = 0;

        key(dl, 0, o + ImVec2(k + gap, 0), {k, k}, forward_.i, wText_.text, s);
        row += k + gap;
        key(dl, 1, o + ImVec2(0, row), {k, k}, left_.i, aText_.text, s);
        key(dl, 2, o + ImVec2(k + gap, row), {k, k}, back_.i, sText_.text, s);
        key(dl, 3, o + ImVec2((k + gap) * 2, row), {k, k}, right_.i, dText_.text, s);
        row += k + gap;

        float full = k * 3 + gap * 2;
        if (mouse_.b) {
            float half = (full - gap) * 0.5f;
            std::string lc, rc;
            if (cpsInside_.b) {
                lc = cpsText(cpsFormat_.text, input::cps(MouseButton::Left));
                rc = cpsText(rmbCpsFormat_.text.empty() ? cpsFormat_.text : rmbCpsFormat_.text, input::cps(MouseButton::Right));
            }
            std::string ln = mouseLabels_.b ? (lmbText_.text.empty() ? "LMB" : lmbText_.text) : "";
            std::string rn = mouseLabels_.b ? (rmbText_.text.empty() ? "RMB" : rmbText_.text) : "";
            key(dl, 4, o + ImVec2(0, row), {half, k * 0.8f}, VK_LBUTTON, ln, s, lc);
            key(dl, 5, o + ImVec2(half + gap, row), {half, k * 0.8f}, VK_RBUTTON, rn, s, rc);
            row += k * 0.8f + gap;
        }
        if (space_.b) {
            float w = full * spaceWidth_.f;
            key(dl, 6, o + ImVec2((full - w) * 0.5f, row), {w, k * spaceHeight_.f}, jump_.i, spaceText_.text, s, "", true);
            row += k * spaceHeight_.f;
        }
        return {full, row};
    }

private:
    std::string cpsText(const std::string& format, int cps) const {
        bool standard = format.empty() || format == "{value} CPS";
        std::string out = standard ? (cpsLabel_.i == 0 ? "CPS {value}" : cpsLabel_.i == 1 ? "{value} CPS" : "{value}") : format;
        std::string v = std::to_string(cps);
        for (size_t at = out.find("{value}"); at != std::string::npos; at = out.find("{value}", at + v.size())) out.replace(at, 7, v);
        return out;
    }

    void key(ImDrawList* dl, int idx, ImVec2 p, ImVec2 size, int vk, const std::string& custom, float s, const std::string& sub = "", bool space = false) {
        auto& t = theme::current();
        bool down = input::down(vk);
        float& a = anim_[size_t(idx)];
        a = draw::approach(a, down ? 1.f : 0.f, down ? pressSpeed_.f : releaseSpeed_.f);

        float r = rounding_.f * s;
        ImVec2 end = p + size;
        if (keyShadow_.b) draw::glow(dl, p + ImVec2(0, 2 * s), end + ImVec2(0, 2 * s), r, ImGui::GetColorU32(keyShadowColor_.color), 6 * s);
        if (glowIdle_.b && a < 0.99f) draw::glow(dl, p, end, r, theme::col(idleGlowColor_.color, 0.5f * (1.f - a)), idleGlowSize_.f * s);
        if (glowPressed_.b && a > 0.01f) draw::glow(dl, p, end, r, theme::col(glowColor_.color, 0.6f * a), pressedGlowSize_.f * s);
        if (keyBlur_.b && rotation_.f == 0.f) post::blur(dl, p, end, r, 8.f * s, {0.f, 0.f, 0.f, 0.f});
        (void)t;

        ImVec4 bg = theme::mix(idle_.color, pressed_.color, a);
        dl->AddRectFilled(p, end, ImGui::GetColorU32(bg), r);
        if (keyBorder_.b)
            dl->AddRect(p, end, ImGui::GetColorU32(theme::mix(keyBorderColor_.color, keyBorderPressed_.color, a)), r, 0, keyBorderWidth_.f * s);

        std::string text = custom.empty() ? (space || vk == VK_LBUTTON || vk == VK_RBUTTON ? "" : keyLabel(vk)) : custom;
        if (custom.empty() && !space && text.size() > 3) text = text.substr(0, 1);
        ImU32 tc = ImGui::GetColorU32(theme::mix(textColor_.color, pressedText_.color, a));
        ImU32 shadowCol = ImGui::GetColorU32(textShadowColor_.color);
        ImVec2 off{textX_.f * s, textY_.f * s}, drop{shadowOffset_.f * s, shadowOffset_.f * s};
        float fs = fonts::hudSize() * s * keyTextScale_.f * (sub.empty() ? 1.f : 0.85f);
        if (!text.empty()) {
            ImVec2 ts = fonts::hud()->CalcTextSizeA(fs, FLT_MAX, 0, text.c_str());
            ImVec2 at = p + (size - ts) * 0.5f + ImVec2(0, sub.empty() ? 0.f : -6 * s) + off;
            if (shadow_.b) dl->AddText(fonts::hud(), fs, at + drop, shadowCol, text.c_str());
            dl->AddText(fonts::hud(), fs, at, tc, text.c_str());
        }
        if (!sub.empty()) {
            float ss = fonts::hudSize() * s * 0.51f * cpsTextScale_.f * (text.empty() ? 1.4f : 1.f);
            ImVec2 st = fonts::hud()->CalcTextSizeA(ss, FLT_MAX, 0, sub.c_str());
            ImVec2 at = text.empty() ? p + (size - st) * 0.5f + off : p + ImVec2((size.x - st.x) * 0.5f, size.y * 0.5f + 4 * s) + off;
            if (shadow_.b && text.empty()) dl->AddText(fonts::hud(), ss, at + drop, shadowCol, sub.c_str());
            dl->AddText(fonts::hud(), ss, at, tc, sub.c_str());
        }
    }

    static std::string keyLabel(int vk) {
        if (vk >= 'A' && vk <= 'Z') return std::string(1, char(vk));
        if (vk >= '0' && vk <= '9') return std::string(1, char(vk));
        switch (vk) {
        case VK_UP: return "^";
        case VK_DOWN: return "v";
        case VK_LEFT: return "<";
        case VK_RIGHT: return ">";
        }
        return "?";
    }

    Setting& keySize_ = slider("size", "Key size", 38.f, 20.f, 70.f, "%.0f");
    Setting& spacing_ = slider("spacing", "Key spacing", 4.f, 0.f, 20.f, "%.0f");
    Setting& mouse_ = toggleSetting("mouse", "Mouse buttons", true);
    Setting& cpsInside_ = toggleSetting("cps", "CPS in mouse buttons", true);
    Setting& cpsLabel_ = choice("cpsLabel", "CPS label position", {"Before value", "After value", "Hidden"}, 1);
    Setting& cpsFormat_ = textSetting("cpsFormat", "CPS text ({value})", "{value} CPS");
    Setting& rmbCpsFormat_ = textSetting("rmbCpsFormat", "CPS text right button (empty = same)", "");
    Setting& mouseLabels_ = toggleSetting("mouseLabels", "Button names in the mouse buttons", true);
    Setting& keyTextScale_ = slider("keyTextScale", "Key text size", 1.f, 0.5f, 2.f, "%.2fx");
    Setting& cpsTextScale_ = slider("cpsTextScale", "CPS text size", 1.f, 0.5f, 2.f, "%.2fx");
    Setting& keyBlur_ = toggleSetting("keyBlur", "Blur behind each key", false);
    Setting& space_ = toggleSetting("space", "Space", true);
    Setting& spaceWidth_ = slider("spaceWidth", "Space bar width", 1.f, 0.3f, 1.2f, "%.2fx");
    Setting& spaceHeight_ = slider("spaceHeight", "Space bar height", 0.45f, 0.2f, 1.f, "%.2fx");
    Setting& pressSpeed_ = slider("pressSpeed", "Press animation speed", 30.f, 5.f, 80.f, "%.0f");
    Setting& releaseSpeed_ = slider("releaseSpeed", "Release animation speed", 12.f, 3.f, 60.f, "%.0f");
    Setting& textX_ = slider("textX", "Text offset X", 0.f, -12.f, 12.f, "%.0f");
    Setting& textY_ = slider("textY", "Text offset Y", 0.f, -12.f, 12.f, "%.0f");
    Setting& wText_ = textSetting("wText", "Text for forward", "");
    Setting& aText_ = textSetting("aText", "Text for left", "");
    Setting& sText_ = textSetting("sText", "Text for back", "");
    Setting& dText_ = textSetting("dText", "Text for right", "");
    Setting& lmbText_ = textSetting("lmbText", "Text for left mouse button", "");
    Setting& rmbText_ = textSetting("rmbText", "Text for right mouse button", "");
    Setting& spaceText_ = textSetting("spaceText", "Text for space", "");
    Setting& glowPressed_ = toggleSetting("glowPressed", "Glow on pressed keys", true);
    Setting& glowIdle_ = toggleSetting("glowIdle", "Glow on idle keys", false);
    Setting& pressedGlowSize_ = slider("pressedGlowSize", "Glow size when pressed", 7.f, 2.f, 30.f, "%.0f");
    Setting& idleGlowSize_ = slider("idleGlowSize", "Glow size when idle", 6.f, 2.f, 30.f, "%.0f");
    Setting& keyBorder_ = toggleSetting("keyBorder", "Key border", false);
    Setting& keyBorderWidth_ = slider("keyBorderWidth", "Key border thickness", 1.5f, 0.5f, 5.f, "%.1f");
    Setting& keyShadow_ = toggleSetting("keyShadow", "Key shadow", false);
    Setting& idle_ = colorSetting("idle", "Key", {0.08f, 0.08f, 0.09f, 0.55f});
    Setting& pressed_ = colorSetting("pressed", "Key pressed", {0.23f, 0.65f, 0.93f, 0.9f});
    Setting& pressedText_ = colorSetting("pressedText", "Text pressed", {1.f, 1.f, 1.f, 1.f});
    Setting& glowColor_ = colorSetting("pressedGlow", "Glow when pressed", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& idleGlowColor_ = colorSetting("idleGlowColor", "Glow when idle", {0.6f, 0.5f, 1.f, 1.f});
    Setting& keyBorderColor_ = colorSetting("keyBorderColor", "Key border", {1.f, 1.f, 1.f, 0.35f});
    Setting& keyBorderPressed_ = colorSetting("keyBorderPressed", "Key border pressed", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& keyShadowColor_ = colorSetting("keyShadowColor", "Key shadow color", {0.f, 0.f, 0.f, 0.5f});
    Setting& textShadowColor_ = colorSetting("textShadowColor", "Text shadow color", {0.f, 0.f, 0.f, 0.55f});
    Setting& forward_ = keySetting("forward", "Forward", 'W');
    Setting& left_ = keySetting("left", "Left", 'A');
    Setting& back_ = keySetting("back", "Back", 'S');
    Setting& right_ = keySetting("right", "Right", 'D');
    Setting& jump_ = keySetting("jump", "Jump", VK_SPACE);
    std::array<float, 8> anim_{};
};
