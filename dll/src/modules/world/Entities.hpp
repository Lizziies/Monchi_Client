#pragma once

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "gui/Widgets.hpp"
#include "core/Log.hpp"
#include "hook/OwnNametag.hpp"
#include "modules/Module.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Fonts.hpp"
#include "render/GameText.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>
#include <format>

class ThirdPersonNametag : public Module {
public:
    ThirdPersonNametag()
        : Module("Third Person Nametag", "Shows your own name tag above your head when you play in third person.", Category::Visual, {"cosmetic"}) {
        sub("World");
        require(need::player | need::camera, need::sigs({"LocalPlayer", "TargetBox"}));
        always_.visible = [this] { return custom_.b; };
        scale_.visible = [this] { return custom_.b || fallback_; };
        nativeColors_.visible = [this] { return !custom_.b && !fallback_ && fx::available(fx::Id::NametagText); };
        for (Setting* c : {&textColor_, &bgColor_}) c->visible = [this] { return custom_.b || fallback_ || nativeColors_.b; };
    }

    // The game's own tag keeps server titles, colors and the pack font. When it cannot be switched on (signature
    // missing or the gate looks different) the overlay draws the name instead, so the module never does nothing.
    void onFrame() override {
        auto& s = game::state();
        bool want = s.inWorld && !custom_.b && s.player.view != game::View::First;
        if (!want) {
            ownNametag::show(false);
            fallback_ = false;
            return;
        }
        fallback_ = !ownNametag::show(true);
        if (!fallback_ && nativeColors_.b) {
            auto t = textColor_.color, b = bgColor_.color;
            fx::out(fx::Id::NametagText, {t.x, t.y, t.z, t.w});
            fx::out(fx::Id::NametagBackground, {b.x, b.y, b.z, b.w});
        }
        if (fallback_ && !reported_) logger::info("third person nametag: the game's own tag is not available, drawing it ourselves");
        reported_ = reported_ || fallback_;
    }

    void onDisable() override {
        ownNametag::show(false);
        fallback_ = false;
    }

    void drawSettings() override {
        if (!fallback_) return;
        ImGui::Spacing();
        widgets::hint("The game's own name tag is not available on this version, so Monchi draws it. Server colors still show, the pack font does not.");
    }

    void onRender(ImDrawList* dl) override {
        if (!custom_.b && !fallback_) return;
        auto& me = game::state().player;
        if (!game::state().inWorld || me.name.empty() || !me.hasBox) return;
        if (me.view == game::View::First && !always_.b) return;
        game::Vec3 top{(me.boxMin.x + me.boxMax.x) * 0.5f, me.boxMax.y + 0.5f, (me.boxMin.z + me.boxMax.z) * 0.5f};
        auto at = game::project(top);
        if (!at) return;
        float dist = std::max(1.f, game::distance(game::state().camera.pos, top));
        float size = std::clamp(100.f / dist, 10.f, 32.f) * ui::scale() * scale_.f;
        const std::string& name = me.name;
        ImFont* f = fonts::regular();
        ImVec2 ts = gameText::size(f, size, name);
        ImVec2 p = *at - ImVec2(ts.x * 0.5f, ts.y);
        float pad = size * 0.2f;
        dl->AddRectFilled(p - ImVec2(pad, pad * 0.5f), p + ts + ImVec2(pad, pad * 0.5f), ImGui::GetColorU32(bgColor_.color));
        gameText::draw(dl, f, size, p, ImGui::GetColorU32(textColor_.color), name);
    }

private:
    Setting& nativeColors_ = toggleSetting("nativeColors", "Override native colors", false);
    Setting& custom_ = toggleSetting("customStyle", "Custom nametag overlay", false);
    Setting& always_ = toggleSetting("always", "Also in first person", false);
    Setting& textColor_ = colorSetting("textColor", "Text color", {1.f, 1.f, 1.f, 1.f});
    Setting& bgColor_ = colorSetting("bgColor", "Background color", {0.f, 0.f, 0.f, 0.25f});
    Setting& scale_ = slider("scale", "Size", 1.f, 0.5f, 2.5f, "%.2fx");
    bool fallback_ = false;
    bool reported_ = false;
};

class TntTimer : public Module {
public:
    TntTimer()
        : Module("TNT Timer", "Shows the time left on the primed TNT you are looking at, fixed on the screen or above the block.", Category::Visual, {"hud-self"}) {
        sub("World");
        require(need::target, need::sigs({"Target", "TargetFuse"}));
        warn_.visible = [this] { return colored_.b; };
        danger_.visible = [this] { return colored_.b; };
        fuseTime_.visible = [this] { return bar_.b; };
        posX_.visible = posY_.visible = [this] { return fixed_.b; };
    }

    void onRender(ImDrawList* dl) override {
        auto& t = game::state().target;
        bool tnt = t.kind == game::Target::Kind::Entity && t.name.find("tnt") != std::string::npos && t.fuse > 0.f;
        shown_ += ((tnt ? 1.f : 0.f) - shown_) * std::min(1.f, ui::dt() * 14.f);
        if (tnt) last_ = t;
        if (shown_ < 0.02f) return;
        auto ds = ImGui::GetIO().DisplaySize;
        float s = ui::scale() * size_.f;
        float fuse = last_.fuse;
        std::string text = format_.text.empty() ? "{value}" : format_.text;
        std::string value = text::num(fuse, decimals_.i) + (unit_.b ? "s" : "");
        for (const char* key : {"{value}", "{VALUE}", "{Value}"})
            for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size())) text.replace(at, 7, value);
        ImVec4 col = textColor_.color;
        if (colored_.b) col = fuse <= danger_.f ? dangerColor_.color : fuse <= warn_.f ? warnColor_.color : textColor_.color;
        col.w *= shown_;
        // fixed on the screen it stays where it is put while the view moves; otherwise it follows the TNT
        ImVec2 at{ds.x * posX_.f, ds.y * posY_.f};
        if (!fixed_.b)
            if (auto p = game::project({last_.pos.x, last_.pos.y + 0.9f, last_.pos.z})) at = *p;
        ImFont* f = fonts::bold();
        float size = 20.f * s;
        ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0.f, text.c_str());
        ImVec2 p = at - ts * 0.5f;
        if (bg_.b) dl->AddRectFilled(p - ImVec2(6 * s, 3 * s), p + ts + ImVec2(6 * s, 3 * s), ImGui::GetColorU32(withAlpha({0.f, 0.f, 0.f, 0.45f}, shown_)), 5 * s);
        dl->AddText(f, size, p + ImVec2(1, 1), IM_COL32(0, 0, 0, int(160 * shown_)), text.c_str());
        dl->AddText(f, size, p, ImGui::GetColorU32(col), text.c_str());
        if (!bar_.b) return;
        float w = std::max(ts.x, 40.f * s), h = 3.f * s;
        ImVec2 b0{at.x - w * 0.5f, p.y + ts.y + 5.f * s};
        float k = std::clamp(fuse / std::max(fuseTime_.f, 0.1f), 0.f, 1.f);
        dl->AddRectFilled(b0, b0 + ImVec2(w, h), ImGui::GetColorU32(withAlpha({0.f, 0.f, 0.f, 0.5f}, shown_)), h);
        dl->AddRectFilled(b0, b0 + ImVec2(w * k, h), ImGui::GetColorU32(col), h);
    }

private:
    Setting& decimals_ = intSlider("decimals", "Decimals", 1, 0, 3);
    Setting& unit_ = toggleSetting("unit", "Show the unit (s)", true);
    Setting& format_ = textSetting("format", "Format ({value})", "{value}");
    Setting& fixed_ = toggleSetting("fixed", "Fixed on the screen", true);
    Setting& posX_ = slider("posX", "Horizontal position", 0.5f, 0.f, 1.f, "%.2f");
    Setting& posY_ = slider("posY", "Vertical position", 0.42f, 0.f, 1.f, "%.2f");
    Setting& bg_ = toggleSetting("bg", "Background", true);
    Setting& size_ = slider("size", "Size", 1.f, 0.6f, 2.5f, "%.2fx");
    Setting& colored_ = toggleSetting("colored", "Color by time left", true);
    Setting& warn_ = slider("warn", "Warning below (s)", 2.f, 0.5f, 4.f, "%.1f");
    Setting& danger_ = slider("danger", "Danger below (s)", 1.f, 0.1f, 3.f, "%.1f");
    Setting& textColor_ = colorSetting("textColor", "Text color", {1.f, 1.f, 1.f, 1.f});
    Setting& warnColor_ = colorSetting("warnColor", "Warning color", {1.f, 0.82f, 0.49f, 1.f});
    Setting& dangerColor_ = colorSetting("dangerColor", "Danger color", {1.f, 0.35f, 0.4f, 1.f});
    Setting& bar_ = toggleSetting("bar", "Progress bar", false);
    Setting& fuseTime_ = slider("fuseTime", "Full fuse (s)", 4.f, 0.5f, 10.f, "%.1f");

    game::Target last_;
    float shown_ = 0.f;
};
