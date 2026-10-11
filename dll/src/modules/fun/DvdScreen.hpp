#pragma once

#include "core/Build.hpp"
#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>

class DvdScreen : public Module {
public:
    DvdScreen() : Module("DVD Screen", "A bouncing logo like the DVD screensaver.", Category::Fun, {"cosmetic"}) {
        sub("Games");
        color_.visible = [this] { return !cycle_.b; };
    }

    void onRender(ImDrawList* dl) override {
        if (onlyHud_.b && (gui::open() || game::state().screen != game::Screen::None)) return;
        float s = ui::scale() * scale_.f;
        auto ds = ImGui::GetIO().DisplaySize;
        const char* label = text_.text.empty() ? build::name : text_.text.c_str();
        float fs = 32 * s;
        ImVec2 ts = fonts::bold()->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
        ImVec2 size{48 * s + ts.x + 6 * s, 46 * s};
        float sp = speed_.f * ui::scale() * ui::dt();
        pos_.x += vel_.x * sp * xSpeed_.f;
        pos_.y += vel_.y * sp * ySpeed_.f;
        if (pos_.x < 0 || pos_.x + size.x > ds.x) {
            vel_.x = pos_.x < 0 ? std::fabs(vel_.x) : -std::fabs(vel_.x);
            pos_.x = std::clamp(pos_.x, 0.f, std::max(0.f, ds.x - size.x));
            hue_ += 0.23f;
        }
        if (pos_.y < 0 || pos_.y + size.y > ds.y) {
            vel_.y = pos_.y < 0 ? std::fabs(vel_.y) : -std::fabs(vel_.y);
            pos_.y = std::clamp(pos_.y, 0.f, std::max(0.f, ds.y - size.y));
            hue_ += 0.37f;
        }
        ImVec4 c = color_.color;
        if (cycle_.b) ImGui::ColorConvertHSVtoRGB(std::fmod(hue_, 1.f), 0.45f, 1.f, c.x, c.y, c.z);
        ImU32 col = IM_COL32(int(c.x * 255), int(c.y * 255), int(c.z * 255), int(255 * opacity_.f));
        draw::heart(dl, pos_ + ImVec2(22 * s, 24 * s), 40 * s, col);
        dl->AddText(fonts::bold(), fs, pos_ + ImVec2(48 * s, 6 * s), col, label);
    }

private:
    Setting& speed_ = slider("speed", "Speed", 160.f, 40.f, 600.f, "%.0f");
    Setting& opacity_ = slider("opacity", "Opacity", 0.8f, 0.1f, 1.f, "%.2f");
    Setting& scale_ = slider("scale", "Size", 1.f, 0.4f, 3.f, "%.2fx");
    Setting& xSpeed_ = slider("xSpeed", "Horizontal speed", 1.f, 0.f, 2.f, "%.2fx");
    Setting& ySpeed_ = slider("ySpeed", "Vertical speed", 1.f, 0.f, 2.f, "%.2fx");
    Setting& text_ = textSetting("text", "Text (empty = Monchi)", "");
    Setting& cycle_ = toggleSetting("cycle", "New color on every bounce", true);
    Setting& color_ = colorSetting("color", "Color", {1.f, 0.55f, 0.75f, 1.f});
    Setting& onlyHud_ = toggleSetting("onlyHud", "Hide in menus", true);
    ImVec2 pos_{100, 100};
    ImVec2 vel_{0.7071f, 0.7071f};
    float hue_ = 0.9f;
};
