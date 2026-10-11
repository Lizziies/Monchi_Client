#pragma once

#include "modules/common/GameHud.hpp"
#include "modules/common/Inventory.hpp"
#include "modules/common/Needs.hpp"
#include "render/Draw.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>

class Paperdoll : public GameList {
public:
    Paperdoll()
        : GameList("Paperdoll", "A small figure in the HUD that wears your armor, sneaks, sprints and flashes red when you are hit.", need::player | game::Domain::Inventory,
                   need::sigs({"LocalPlayer"}), {"cosmetic"}, {0.915f, 0.8f}) {
        sub("Info displays");
        background_.b = false;
        linger_.visible = [this] { return !always_.b; };
    }

    void onFrame() override {
        for (auto& e : game::events())
            if (e.kind == game::EventKind::Hurt) flash_ = 1.f;
        flash_ = std::max(0.f, flash_ - ui::dt() * 3.f);
        auto& p = game::state().player;
        lean_ = draw::approach(lean_, p.sprinting ? 1.f : 0.f, 10.f);
        crouch_ = draw::approach(crouch_, p.sneaking ? 1.f : 0.f, 14.f);
        bool moving = p.sprinting || p.sneaking || p.swimming || p.gliding || p.flying || p.usingItem || flash_ > 0.f;
        if (moving) lastActive_ = ui::time();
        shown_ = draw::approach(shown_, always_.b || ui::time() - lastActive_ < linger_.f ? 1.f : 0.f, 8.f);
    }

    void onRender(ImDrawList* dl) override {
        if (shown_ < 0.01f && !gui::editingHud()) return;
        GameList::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        int firstVtx = dl->VtxBuffer.Size;
        ImVec2 out = figure(dl, o, s);
        float alpha = gui::editingHud() ? 1.f : shown_;
        if (alpha < 0.999f)
            for (int i = firstVtx; i < dl->VtxBuffer.Size; i++) {
                ImU32& c = dl->VtxBuffer[i].col;
                c = (c & ~IM_COL32_A_MASK) | (ImU32(float((c >> IM_COL32_A_SHIFT) & 0xFF) * alpha) << IM_COL32_A_SHIFT);
            }
        return out;
    }

private:
    ImVec2 figure(ImDrawList* dl, ImVec2 o, float s) {
        auto& p = game::state().player;
        float u = size_.f * s / 32.f;
        float t = float(ui::time());
        float walk = std::hypot(p.vel.x, p.vel.z) > 0.3f ? std::sin(t * 9.f) * (p.sprinting ? 0.6f : 0.4f) : 0.f;
        float h = 32.f * u;
        ImVec2 base = o + ImVec2(12 * u, h);
        float drop = crouch_ * 3.f * u;
        float shear = lean_ * 2.f * u + crouch_ * 1.5f * u;

        auto flash = [&](ImU32 c) {
            if (flash_ <= 0.f || !hurt_.b) return c;
            ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
            v = ImVec4(v.x + (1.f - v.x) * flash_ * 0.6f, v.y * (1.f - flash_ * 0.6f), v.z * (1.f - flash_ * 0.6f), v.w);
            return ImGui::ColorConvertFloat4ToU32(v);
        };
        auto armorColor = [&](int slot, ImU32 fallback) {
            auto& a = p.armor[size_t(slot)];
            return flash(armor_.b && !a.empty() ? materialColor(a.name) : fallback);
        };
        auto box = [&](float x0, float y0, float x1, float y1, ImU32 c, float topShear = 0.f) {
            ImVec2 a = base + ImVec2(x0 * u + topShear, -(32 - y0) * u + drop), b = base + ImVec2(x1 * u + topShear, -(32 - y1) * u + drop);
            dl->AddRectFilled(a, b, c, 1.5f * u);
        };

        ImU32 skin = flash(ImGui::GetColorU32(skin_.color)), shirt = flash(ImGui::GetColorU32(shirt_.color)), pants = flash(ImGui::GetColorU32(pants_.color));
        ImU32 hair = flash(ImGui::GetColorU32(hair_.color));

        float legSwing = walk * 3.f * u;
        box(2, 20, 6, 32, armorColor(2, pants), 0.f);
        dl->AddRectFilled(base + ImVec2(2 * u + legSwing, -12 * u + drop), base + ImVec2(6 * u + legSwing, drop), armorColor(3, pants), 1.f * u);
        dl->AddRectFilled(base + ImVec2(6 * u - legSwing, -12 * u + drop), base + ImVec2(10 * u - legSwing, drop), armorColor(3, pants), 1.f * u);
        box(2, 10, 10, 20, armorColor(1, shirt), shear * 0.5f);
        float arm = walk * 3.f * u;
        dl->AddRectFilled(base + ImVec2(-2 * u - arm + shear * 0.5f, -22 * u + drop), base + ImVec2(2 * u - arm + shear * 0.5f, -10 * u + drop), armorColor(1, skin), 1.f * u);
        dl->AddRectFilled(base + ImVec2(10 * u + arm + shear * 0.5f, -22 * u + drop), base + ImVec2(14 * u + arm + shear * 0.5f, -10 * u + drop), armorColor(1, skin), 1.f * u);
        box(2, 2, 10, 10, skin, shear);
        box(2, 2, 10, 4, hair, shear);
        auto& helm = p.armor[0];
        bool helmet = armor_.b && !helm.empty();
        if (helmet) box(1.5f, 1.5f, 10.5f, 10.5f, armorColor(0, skin), shear);
        ImU32 eye = helmet ? IM_COL32(40, 30, 50, 255) : IM_COL32(60, 40, 70, 255);
        box(3, 6, 4.5f, 7.5f, eye, shear);
        box(7.5f, 6, 9, 7.5f, eye, shear);

        if (item_.b && !p.held().empty())
            dl->AddRectFilled(base + ImVec2(12 * u + arm + shear * 0.5f, -16 * u + drop), base + ImVec2(17 * u + arm + shear * 0.5f, -10 * u + drop), materialColor(p.held().name), 1.f * u);
        return {18 * u, h + 4 * u};
    }

    Setting& size_ = slider("size", "Size", 90.f, 40.f, 220.f, "%.0f");
    Setting& always_ = toggleSetting("always", "Always show", true);
    Setting& linger_ = slider("linger", "Stays after moving (s)", 1.5f, 0.f, 10.f, "%.1f s");
    Setting& armor_ = toggleSetting("armor", "Show armor", true);
    Setting& item_ = toggleSetting("item", "Item in hand", true);
    Setting& hurt_ = toggleSetting("hurt", "Red on hits", true);
    Setting& skin_ = colorSetting("skin", "Skin color", {1.f, 0.82f, 0.72f, 1.f});
    Setting& hair_ = colorSetting("hair", "Hair color", {0.45f, 0.28f, 0.22f, 1.f});
    Setting& shirt_ = colorSetting("shirt", "Shirt", {1.f, 0.55f, 0.75f, 1.f});
    Setting& pants_ = colorSetting("pants", "Pants", {0.55f, 0.5f, 0.85f, 1.f});
    float flash_ = 0.f;
    float lean_ = 0.f;
    float crouch_ = 0.f;
    float shown_ = 1.f;
    double lastActive_ = -100.0;
};
