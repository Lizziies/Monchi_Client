#pragma once

#include "core/Config.hpp"
#include "gui/Gui.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <json.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <format>
#include <random>
#include "modules/common/WaypointScope.hpp"

class Waypoints : public Module {
public:
    Waypoints()
        : Module("Waypoints", "Your own markers with beam, name and distance, also as an edge arrow.",
                 Category::Visual, {"hud-self"}) {
        sub("World");
        require(need::player | game::Domain::Camera, need::sigs({"LocalPlayer"}));
        data_.hidden = true;
        beamHeight_.visible = [this] { return beam_.b; };
        beamStyle_.visible = [this] { return beam_.b; };
        for (Setting* s : {&sides_, &radius_, &glow_}) s->visible = [this] { return beam_.b && beamStyle_.i == 1; };
        glowRadius_.visible = [this] { return beam_.b && beamStyle_.i == 1 && glow_.b; };
        deathPoint_.visible = [] { return need::have("HurtEvents"); };
        for (Setting* s : {&bgUse_, &rounding_, &border_}) s->visible = [this] { return box_.b; };
        bgOpacity_.visible = [this] { return box_.b && bgUse_.b; };
        bgColor_.visible = [this] { return box_.b && !bgUse_.b; };
        borderWidth_.visible = [this] { return box_.b && border_.b; };
        borderUse_.visible = [this] { return box_.b && border_.b; };
        borderColor_.visible = [this] { return box_.b && border_.b && !borderUse_.b; };
        textColor_.visible = [this] { return !textUse_.b; };
    }

    // the list is drawn on the render thread, so the key only asks and the point is added there
    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && addKey_.i && ev.vk == addKey_.i) addWanted_ = true;
    }

    void onFrame() override {
        sync();
        if (addWanted_.exchange(false) && addHere(i18n::fmt("Point {}", list_.size() + 1)))
            notify::push(name(), i18n::tr("Waypoint added"), notify::Kind::Ok, 2.f);
        for (auto& e : game::events())
            if (e.kind == game::EventKind::Death && deathPoint_.b) {
                auto& pos = game::state().player.pos;
                std::string death = i18n::tr("Death");
                if (game::state().player.dimension < 0 || place().empty()) continue;
                std::erase_if(list_, [&](const Wp& w) { return w.name == death && w.where == place() && w.dim == game::state().player.dimension; });
                Wp w{death, pos.x, pos.y, pos.z, game::state().player.dimension, {1.f, 0.4f, 0.45f}};
                w.where = place();
                list_.push_back(w);
                save();
            }
    }

    void onRender(ImDrawList* dl) override {
        sync();
        auto& p = game::state().player;
        auto ds = ImGui::GetIO().DisplaySize;
        float s = ui::scale();
        std::string here = place();
        for (auto& w : list_) {
            if (!w.on || w.dim != p.dimension) continue;
            if (!waypointScope::matches(w.where, here, sameServer_.b)) continue;
            game::Vec3 pos{w.x + 0.5f, w.y + 1.0f, w.z + 0.5f};
            float dist = game::distance(p.pos, {w.x, w.y, w.z});
            if (maxDist_.f > 0.f && dist > maxDist_.f) continue;
            float alpha = fade_.b ? std::clamp((dist - 2.f) / 6.f, 0.f, 1.f) : 1.f;
            ImVec4 c = colorOf(w);
            c.w = alpha;
            ImU32 col = ImGui::GetColorU32(c);

            if (beam_.b && w.beam) beamOf(dl, w, c, s);
            auto sp = game::project(pos);
            bool onScreen = sp && sp->x > 0 && sp->x < ds.x && sp->y > 0 && sp->y < ds.y;
            if (onScreen) {
                icon(dl, *sp, col, s);
                label(dl, w, *sp, c, dist, s);
            } else if (edge_.b) {
                edgeArrow(dl, pos, col, dist, s);
            }
        }
    }

    struct Mark {
        std::string name;
        float x, y, z;
        float color[3];
    };

    std::vector<Mark> marks(int dimension) {
        sync();
        std::vector<Mark> out;
        for (auto& w : list_)
            if (w.on && w.dim == dimension && waypointScope::matches(w.where, place(), sameServer_.b))
                out.push_back({w.name, w.x, w.y, w.z, {w.color[0], w.color[1], w.color[2]}});
        return out;
    }

    void drawSettings() override {
        sync();
        ImGui::Spacing();
        if (!need::have("Dimension") || game::state().player.dimension < 0 || place().empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::current().textDim);
            ImGui::TextWrapped("%s", i18n::tr("Waiting for world and dimension data before adding waypoints."));
            ImGui::PopStyleColor();
        }
        ImGui::SetNextItemWidth(180);
        ImGui::InputText("##wpname", name_, sizeof(name_));
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Add here"))) addHere(name_[0] ? name_ : i18n::tr("Dot"));
        int remove = -1;
        for (size_t i = 0; i < list_.size(); i++) {
            auto& w = list_[i];
            ImGui::PushID(int(i));
            bool changed = ImGui::Checkbox("##on", &w.on);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", i18n::tr("Shown"));
            ImGui::SameLine();
            changed |= ImGui::ColorEdit3("##c", w.color, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            if (edit_ == int(i)) {
                ImGui::SetNextItemWidth(140);
                if (ImGui::InputText("##rename", rename_, sizeof(rename_), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (rename_[0]) w.name = rename_;
                    edit_ = -1;
                    changed = true;
                }
            } else {
                ImGui::Text("%s  (%.0f, %.0f, %.0f)", w.name.c_str(), w.x, w.y, w.z);
                if (ImGui::IsItemClicked()) {
                    edit_ = int(i);
                    std::snprintf(rename_, sizeof(rename_), "%s", w.name.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", i18n::tr("Click to rename"));
            }
            ImGui::SameLine();
            changed |= ImGui::Checkbox(i18n::tr("Rainbow"), &w.rgb);
            ImGui::SameLine();
            changed |= ImGui::Checkbox(i18n::tr("Beam"), &w.beam);
            ImGui::SameLine();
            if (ImGui::SmallButton(i18n::tr("Delete"))) remove = int(i);
            ImGui::PopID();
            if (changed) save();
        }
        if (remove >= 0) {
            list_.erase(list_.begin() + remove);
            edit_ = -1;
            save();
        }
    }

private:
    struct Wp {
        std::string name;
        float x, y, z;
        int dim;
        float color[3];
        bool on = true;
        bool rgb = false;
        bool beam = true;
        std::string where;
    };

    static std::string place() {
        auto& st = game::state();
        if (!st.server.empty()) return st.server;
        return st.world.id.empty() ? st.world.name : "world:" + st.world.id;
    }

    ImVec4 colorOf(const Wp& w) const {
        if (!w.rgb) return {w.color[0], w.color[1], w.color[2], 1.f};
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(std::fmod(float(ui::time()) * 0.15f, 1.f), 0.6f, 1.f, r, g, b);
        return {r, g, b, 1.f};
    }

    bool addHere(const std::string& name) {
        auto& p = game::state().player;
        if (p.dimension < 0 || place().empty()) return false;
        Wp w{name, std::floor(p.pos.x), std::floor(p.pos.y), std::floor(p.pos.z), p.dimension, {color_.color.x, color_.color.y, color_.color.z}};
        if (randomColor_.b) {
            static std::mt19937 rng{std::random_device{}()};
            float h = std::uniform_real_distribution<float>(0.f, 1.f)(rng);
            ImGui::ColorConvertHSVtoRGB(h, 0.6f, 1.f, w.color[0], w.color[1], w.color[2]);
        }
        w.where = place();
        list_.push_back(w);
        save();
        return true;
    }

    void label(ImDrawList* dl, const Wp& w, ImVec2 at, ImVec4 c, float dist, float s) {
        std::string text = w.name;
        if (distance_.b) text += std::format("  {:.0f} m", dist);
        float size = 13.f * s * labelScale_.f;
        if (shrink_.b) size *= std::clamp(1.25f - dist / 400.f, 0.7f, 1.25f);
        ImFont* f = fonts::bold();
        ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0.f, text.c_str());
        ImVec2 p{at.x - ts.x * 0.5f, at.y + 11 * s};
        if (box_.b) {
            ImVec2 pad{6 * s, 3 * s};
            ImVec4 bg = bgUse_.b ? ImVec4{c.x, c.y, c.z, bgOpacity_.f} : bgColor_.color;
            bg.w *= c.w;
            float r = rounding_.f * s;
            dl->AddRectFilled(p - pad, p + ts + pad, ImGui::GetColorU32(bg), r);
            if (border_.b) {
                ImVec4 bc = borderUse_.b ? ImVec4{c.x, c.y, c.z, 1.f} : borderColor_.color;
                bc.w *= c.w;
                dl->AddRect(p - pad, p + ts + pad, ImGui::GetColorU32(bc), r, 0, borderWidth_.f * s);
            }
        } else {
            dl->AddText(f, size, p + ImVec2(1, 1), IM_COL32(0, 0, 0, int(160 * c.w)), text.c_str());
        }
        ImVec4 tc = textUse_.b ? c : withAlpha(textColor_.color, c.w);
        dl->AddText(f, size, p, ImGui::GetColorU32(tc), text.c_str());
    }

    void beamOf(ImDrawList* dl, const Wp& w, ImVec4 c, float s) {
        float cx = w.x + 0.5f, cz = w.z + 0.5f, y0 = w.y, y1 = w.y + beamHeight_.f;
        if (beamStyle_.i == 0) {
            ImVec2 a, b;
            if (game::projectLine({cx, y0, cz}, {cx, y1, cz}, a, b)) dl->AddLine(a, b, ImGui::GetColorU32(withAlpha(c, 0.55f)), 2.5f * s);
            return;
        }
        column(dl, cx, cz, y0, y1, radius_.f, withAlpha(c, 0.45f));
        if (glow_.b) column(dl, cx, cz, y0, y1, glowRadius_.f, ImVec4{1.f, 1.f, 1.f, 0.12f * c.w});
    }

    // a beam with real width: each side of the prism is projected as its own quad
    void column(ImDrawList* dl, float cx, float cz, float y0, float y1, float r, ImVec4 c) {
        int n = std::clamp(sides_.i, 3, 32);
        ImU32 col = ImGui::GetColorU32(c);
        for (int i = 0; i < n; i++) {
            float a0 = 6.2831853f * float(i) / float(n), a1 = 6.2831853f * float(i + 1) / float(n);
            game::Vec3 v[4] = {{cx + r * std::cos(a0), y0, cz + r * std::sin(a0)}, {cx + r * std::cos(a0), y1, cz + r * std::sin(a0)},
                               {cx + r * std::cos(a1), y1, cz + r * std::sin(a1)}, {cx + r * std::cos(a1), y0, cz + r * std::sin(a1)}};
            ImVec2 pts[4];
            bool ok = true;
            for (int k = 0; k < 4 && ok; k++) {
                auto pt = game::project(v[k]);
                ok = pt.has_value();
                if (ok) pts[k] = *pt;
            }
            if (ok) dl->AddConvexPolyFilled(pts, 4, col);
        }
    }

    void icon(ImDrawList* dl, ImVec2 c, ImU32 col, float s) {
        float r = 6.f * s;
        switch (iconStyle_.i) {
        case 0: dl->AddQuadFilled({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}, col); break;
        case 1: dl->AddCircleFilled(c, r * 0.8f, col); break;
        default: draw::heart(dl, c, r * 2.f, col); break;
        }
    }

    void edgeArrow(ImDrawList* dl, game::Vec3 target, ImU32 col, float dist, float s) {
        auto ds = ImGui::GetIO().DisplaySize;
        auto& cam = game::state().camera;
        float yaw = cam.yaw * 0.0174533f;
        float dx = target.x - cam.pos.x, dz = target.z - cam.pos.z;
        float fwd = dx * -std::sin(yaw) + dz * std::cos(yaw), right = dx * -std::cos(yaw) + dz * -std::sin(yaw);
        float ang = std::atan2(right, fwd);
        ImVec2 c{ds.x * 0.5f, ds.y * 0.5f};
        float rx = ds.x * 0.45f, ry = ds.y * 0.42f;
        ImVec2 p{c.x + std::sin(ang) * rx, c.y - std::cos(ang) * ry * 0.9f};
        ImVec2 d{std::sin(ang), -std::cos(ang)};
        ImVec2 n{-d.y, d.x};
        float k = 9.f * s;
        dl->AddTriangleFilled(p + d * k, p - d * k * 0.6f + n * k * 0.7f, p - d * k * 0.6f - n * k * 0.7f, col);
        if (distance_.b) dl->AddText(fonts::bold(), 12.f * s, p + ImVec2(-12 * s, 12 * s), col, std::format("{:.0f} m", dist).c_str());
    }

    void sync() {
        if (data_.text == synced_) return;
        synced_ = data_.text;
        load();
    }

    void load() {
        list_.clear();
        auto j = nlohmann::json::parse(data_.text, nullptr, false);
        if (!j.is_array()) return;
        for (auto& e : j) {
            if (!e.is_object()) continue;
            Wp w{e.value("name", "?"), e.value("x", 0.f), e.value("y", 0.f), e.value("z", 0.f), e.value("dim", 0), {1.f, 1.f, 1.f}};
            if (e.contains("c") && e["c"].is_array() && e["c"].size() == 3)
                for (int k = 0; k < 3; k++) w.color[k] = e["c"][k].get<float>();
            w.on = e.value("on", true);
            w.rgb = e.value("rgb", false);
            w.beam = e.value("beam", true);
            w.where = e.value("where", "");
            list_.push_back(w);
        }
    }

    void save() {
        nlohmann::json j = nlohmann::json::array();
        for (auto& w : list_)
            j.push_back({{"name", w.name}, {"x", w.x}, {"y", w.y}, {"z", w.z}, {"dim", w.dim}, {"c", {w.color[0], w.color[1], w.color[2]}},
                         {"on", w.on}, {"rgb", w.rgb}, {"beam", w.beam}, {"where", w.where}});
        data_.text = j.dump();
        synced_ = data_.text;
        config::markDirty();
    }

    Setting& addKey_ = keySetting("addKey", "Set a point at my position", 0);
    Setting& color_ = colorSetting("color", "Default color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& randomColor_ = toggleSetting("randomColor", "Random color for new points", false);
    Setting& iconStyle_ = choice("icon", "Icon", {"Diamond", "Dot", "Heart"});
    Setting& distance_ = toggleSetting("distance", "Show distance", true);
    Setting& labelScale_ = slider("labelScale", "Text size", 1.f, 0.6f, 2.f, "%.2fx");
    Setting& shrink_ = toggleSetting("shrink", "Smaller text far away", false);
    Setting& textUse_ = toggleSetting("textUse", "Text in the waypoint color", true);
    Setting& textColor_ = colorSetting("textColor", "Text color", {1.f, 1.f, 1.f, 1.f});
    Setting& box_ = toggleSetting("box", "Label background", false);
    Setting& bgUse_ = toggleSetting("bgUse", "Background in the waypoint color", false);
    Setting& bgOpacity_ = slider("bgOpacity", "Background opacity", 0.25f, 0.f, 1.f, "%.2f");
    Setting& bgColor_ = colorSetting("bgColor", "Background color", {0.f, 0.f, 0.f, 0.45f});
    Setting& rounding_ = slider("rounding", "Rounding", 5.f, 0.f, 16.f, "%.0f");
    Setting& border_ = toggleSetting("border", "Border", false);
    Setting& borderWidth_ = slider("borderWidth", "Border thickness", 1.5f, 0.5f, 6.f, "%.1f");
    Setting& borderUse_ = toggleSetting("borderUse", "Border in the waypoint color", true);
    Setting& borderColor_ = colorSetting("borderColor", "Border color", {1.f, 1.f, 1.f, 1.f});
    Setting& beam_ = toggleSetting("beam", "Light beam", true);
    Setting& beamStyle_ = choice("beamStyle", "Beam style", {"Line", "Column"});
    Setting& beamHeight_ = slider("beamHeight", "Beam height", 60.f, 10.f, 256.f, "%.0f");
    Setting& sides_ = intSlider("sides", "Sides of the column", 12, 3, 32);
    Setting& radius_ = slider("radius", "Column radius", 0.25f, 0.05f, 0.8f, "%.2f");
    Setting& glow_ = toggleSetting("glow", "Outer glow", true);
    Setting& glowRadius_ = slider("glowRadius", "Glow radius", 0.4f, 0.1f, 1.f, "%.2f");
    Setting& edge_ = toggleSetting("edge", "Arrow at the screen edge", true);
    Setting& fade_ = toggleSetting("fade", "Fade out when close", true);
    Setting& maxDist_ = slider("maxDist", "Maximum distance (0 = unlimited)", 0.f, 0.f, 2000.f, "%.0f");
    Setting& sameServer_ = toggleSetting("sameServer", "Only on the server they were set on", true);
    Setting& deathPoint_ = toggleSetting("death", "Set the death point automatically", true);
    Setting& data_ = textSetting("data", "Data", "[]");
    std::vector<Wp> list_;
    std::atomic<bool> addWanted_{false};
    std::string synced_ = "[]";
    char name_[64] = "";
    char rename_[64] = "";
    int edit_ = -1;
};
