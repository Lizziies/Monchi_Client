#pragma once

#include "PostFx.hpp"
#include "core/Paths.hpp"
#include "modules/common/Context.hpp"
#include "gui/Gui.hpp"
#include "modules/Manager.hpp"
#include "modules/Module.hpp"
#include "render/Ui.hpp"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>

class SaturationHue : public Module {
public:
    SaturationHue()
        : Module("Saturation / Hue", "Changes saturation and hue of the game image, with presets and an optional rainbow cycle.",
                 Category::Visual, {"cosmetic"}) {
        sub("Post effects");
        saturation_.visible = [this] { return preset_.i == 0; };
        hue_.visible = [this] { return preset_.i == 0 && !cycle_.b; };
        speed_.visible = [this] { return cycle_.b; };
    }

    void onFrame() override {
        auto& p = post::params();
        float sat = saturation_.f, hue = hue_.f;
        switch (preset_.i) {
        case 1: sat = 0.f; break;
        case 2: sat = 1.45f; break;
        case 3: sat = 0.75f; p.brightness += 0.04f; break;
        case 4: sat = 1.15f; hue = -12.f; break;
        case 5: sat = 0.55f; hue = 18.f; break;
        default: break;
        }
        if (cycle_.b) {
            phase_ = std::fmod(phase_ + ui::dt() * speed_.f * 36.f, 360.f);
            hue = phase_ - 180.f;
        }
        p.saturation *= sat;
        p.hue += hue;
    }

private:
    Setting& preset_ = choice("preset", "Preset", {"Own values", "Black and white", "Vivid", "Pastel", "Warm", "Retro"});
    Setting& saturation_ = slider("saturation", "Saturation", 1.2f, 0.f, 2.5f, "%.2f");
    Setting& hue_ = slider("hue", "Hue", 0.f, -180.f, 180.f, "%.0f°");
    Setting& cycle_ = toggleSetting("cycle", "Cycle the hue", false);
    Setting& speed_ = slider("speed", "Speed", 0.5f, 0.05f, 3.f, "%.2f");
    float phase_ = 0.f;
};

class BrightnessContrast : public Module {
public:
    BrightnessContrast()
        : Module("Brightness / Contrast", "Brightness, contrast, gamma and vignette for the finished game image. Adds to Fullbright, which sets the brightness of the game itself.",
                 Category::Visual, {"cosmetic"}) {
        sub("Post effects");
    }

    void onFrame() override {
        auto& p = post::params();
        p.brightness += brightness_.f;
        p.contrast *= contrast_.f;
        p.gamma *= gamma_.f;
        p.vignette = std::max(p.vignette, vignette_.f);
    }

private:
    Setting& brightness_ = slider("brightness", "Brightness", 0.f, -0.5f, 0.5f, "%.2f");
    Setting& contrast_ = slider("contrast", "Contrast", 1.f, 0.5f, 2.f, "%.2f");
    Setting& gamma_ = slider("gamma", "Gamma", 1.f, 0.5f, 2.5f, "%.2f");
    Setting& vignette_ = slider("vignette", "Vignette", 0.f, 0.f, 1.f, "%.2f");
};

class ScreenTint : public Module {
public:
    ScreenTint()
        : Module("Screen Tint", "Puts a color filter over the game image, for example a soft pink.",
                 Category::Visual, {"cosmetic"}) {
        sub("Post effects");
        pulseSpeed_.visible = [this] { return pulse_.b; };
        pulseDepth_.visible = [this] { return pulse_.b; };
    }

    void onFrame() override {
        float k = strength_.f;
        if (pulse_.b) {
            phase_ = std::fmod(phase_ + ui::dt() * pulseSpeed_.f * 6.2832f, 6.2832f);
            k *= 1.f - pulseDepth_.f * (0.5f + 0.5f * std::sin(phase_));
        }
        auto& p = post::params();
        p.tint[0] = color_.color.x;
        p.tint[1] = color_.color.y;
        p.tint[2] = color_.color.z;
        p.tint[3] = std::clamp(k * color_.color.w, 0.f, 1.f);
        p.tintMode = mode_.i;
    }

private:
    Setting& color_ = colorSetting("color", "Color", {1.f, 0.55f, 0.75f, 1.f});
    Setting& strength_ = slider("strength", "Strength", 0.14f, 0.f, 1.f, "%.2f");
    Setting& mode_ = choice("mode", "Blend mode", {"Mix", "Multiply", "Add", "Overlay"});
    Setting& pulse_ = toggleSetting("pulse", "Pulse", false);
    Setting& pulseSpeed_ = slider("pulseSpeed", "Pulse speed (Hz)", 0.4f, 0.05f, 3.f, "%.2f");
    Setting& pulseDepth_ = slider("pulseDepth", "Pulse depth", 0.5f, 0.f, 1.f, "%.2f");
    float phase_ = 0.f;
};

class Sharpen : public Module {
public:
    Sharpen()
        : Module("Sharpen", "Sharpens the game image, useful at low resolution or scaling.", Category::Visual,
                 {"cosmetic"}) {
        sub("Post effects");
    }

    void onFrame() override { post::params().sharpen += amount_.f; }

private:
    Setting& amount_ = slider("amount", "Strength", 0.6f, 0.f, 3.f, "%.2f");
};

class DepthOfField : public Module {
public:
    DepthOfField()
        : Module("Depth of Field", "Soft focus: the image gets blurry towards the edges, like a camera with a tilt-shift effect.",
                 Category::Visual, {"cosmetic"}) {
        sub("Post effects");
    }

    void onFrame() override {
        float k = onlyZoom_.b ? std::clamp((ctx::zoomLevel - 1.f) / 1.5f, 0.f, 1.f) : 1.f;
        if (onlyMenu_.b && (gui::open() || game::state().screen != game::Screen::None)) k = 0.f;
        float amount = amount_.f * k;
        if (amount <= 0.f) return;
        auto& p = post::params();
        p.dof = std::max(p.dof, amount);
        p.dofSharp = range_.f;
        p.dofEdge = range_.f + softness_.f;
        p.dofFocus = focus_.f;
        p.dofBand = shape_.i == 1;
        p.dofReach = 0.03f * reach_.f;
        p.dofRings = quality_.i;
    }

private:
    Setting& amount_ = slider("amount", "Strength", 0.5f, 0.f, 1.f, "%.2f");
    Setting& shape_ = choice("shape", "Shape of the sharp area", {"Round", "Horizontal band (tilt-shift)"});
    Setting& range_ = slider("range", "Size of the sharp area", 0.2f, 0.f, 0.8f, "%.2f");
    Setting& softness_ = slider("softness", "Transition width", 0.7f, 0.05f, 1.f, "%.2f");
    Setting& focus_ = slider("focus", "Height of the sharp area", 0.5f, 0.f, 1.f, "%.2f");
    Setting& reach_ = slider("reach", "Maximum blur", 1.f, 0.25f, 4.f, "%.2fx");
    Setting& quality_ = intSlider("quality", "Quality", 1, 1, 3);
    Setting& onlyZoom_ = toggleSetting("onlyZoom", "Only while zooming", false);
    Setting& onlyMenu_ = toggleSetting("offInMenus", "Off in menus", false);
};

class Blur : public Module {
public:
    Blur()
        : Module("Blur", "Blurs the game image, always or only while a menu, the inventory or the chat is open.",
                 Category::Visual, {"cosmetic"}) {
        sub("Post effects");
    }

    void onFrame() override {
        bool menu = gui::open() || game::state().screen != game::Screen::None;
        if (when_.i == 1 && !menu) return;
        float px = amount_.f * 24.f * ui::scale();
        post::params().blur = std::max(post::params().blur, px);
    }

private:
    Setting& amount_ = slider("amount", "Strength", 0.5f, 0.05f, 1.f, "%.2f");
    Setting& when_ = choice("when", "When", {"Always", "Only in menus"}, 1);
};

class ShaderPacks : public Module {
public:
    ShaderPacks()
        : Module("Shader Packs",
                 "Loads shaders on top of the game image, also on servers. Four looks are built in, your own .hlsl files go into the shaders folder. Only changes what you see.",
                 Category::Visual, {"cosmetic"}) {
        sub("Post effects");
        rebuild();
    }

    void onEnable() override { rebuild(); }

    void onFrame() override {
        int index = pickIndex();
        if (index < 0) return;
        auto& p = post::params();
        p.shader = index;
        p.shaderMix = amount_.f;
    }

    void drawSettings() override {
        ImGui::Spacing();
        int index = pickIndex();
        if (index >= 0) {
            std::string err = post::shaderError(index);
            if (!err.empty()) ImGui::TextWrapped("%s", err.c_str());
        }
        if (ImGui::SmallButton(i18n::tr("Reload shaders"))) {
            post::reloadShaders();
            rebuild();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Open folder"))) ShellExecuteW(nullptr, L"open", (paths::root() / L"shaders").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

private:
    void rebuild() {
        names_.clear();
        for (auto& s : post::shaders()) names_.push_back(s.name);
        pack_.choices = names_;
        pack_.i = std::clamp(pack_.i, 0, std::max(0, (int)names_.size() - 1));
    }

    int pickIndex() const { return pack_.i >= 0 && pack_.i < (int)names_.size() ? pack_.i : -1; }

    std::vector<std::string> names_;
    Setting& pack_ = choice("pack", "Shader", {"Soft Glow"});
    Setting& amount_ = slider("amount", "Strength", 1.f, 0.f, 1.f, "%.2f");
};

class ColorFilter : public Module {
public:
    ColorFilter()
        : Module("Color Filter", "Color filter for color blindness: corrects or simulates red, green and blue weakness.",
                 Category::Visual, {"cosmetic"}) {
        sub("Post effects");
    }

    void onFrame() override {
        int base = type_.i + 1;
        post::params().colorMode = base + (simulate_.b ? 3 : 0);
    }

private:
    Setting& type_ = choice("type", "Type", {"Red weakness (protanopia)", "Green weakness (deuteranopia)", "Blue weakness (tritanopia)"});
    Setting& simulate_ = toggleSetting("simulate", "Only simulate instead of correcting", false);
};

class NightShift : public Module {
public:
    NightShift()
        : Module("Night Shift", "A warmer blue-light filter for relaxed eyes, optionally automatic in the evening.", Category::Visual,
                 {"cosmetic"}) {
        sub("Post effects");
        from_.visible = [this] { return auto_.b; };
        to_.visible = [this] { return auto_.b; };
    }

    void onFrame() override {
        if (auto_.b && !inWindow()) return;
        float k = temperature_.f;
        float r = 1.f, g, b;
        float t = k / 100.f;
        g = std::clamp(t <= 66.f ? 0.39008657f * std::log(t) - 0.63184144f : 1.29293619f * std::pow(t - 60.f, -0.1332047592f), 0.f, 1.f);
        b = t >= 66.f ? 1.f : (t <= 19.f ? 0.f : std::clamp(0.54320679f * std::log(t - 10.f) - 1.19625409f, 0.f, 1.f));
        auto& p = post::params();
        p.night[0] = r;
        p.night[1] = g;
        p.night[2] = b;
        p.night[3] = strength_.f;
    }

private:
    bool inWindow() const {
        SYSTEMTIME st;
        GetLocalTime(&st);
        float now = st.wHour + st.wMinute / 60.f;
        float a = from_.f, e = to_.f;
        return a <= e ? now >= a && now < e : now >= a || now < e;
    }

    Setting& temperature_ = slider("temperature", "Color temperature (K)", 3400.f, 1500.f, 6500.f, "%.0f");
    Setting& strength_ = slider("strength", "Strength", 0.8f, 0.f, 1.f, "%.2f");
    Setting& auto_ = toggleSetting("auto", "Only at certain hours", false);
    Setting& from_ = slider("from", "From (hour)", 20.f, 0.f, 24.f, "%.1f");
    Setting& to_ = slider("to", "Until (hour)", 7.f, 0.f, 24.f, "%.1f");
};
