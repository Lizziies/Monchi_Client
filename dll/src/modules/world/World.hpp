#pragma once

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/Context.hpp"
#include "modules/common/Needs.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

inline ImVec4 rainbow(float speed, float sat = 0.6f) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(std::fmod(float(ui::time()) * speed * 0.2f, 1.f), sat, 1.f, r, g, b);
    return {r, g, b, 1.f};
}

class BlockOutline : public Module {
public:
    BlockOutline()
        : Module("Block Outline", "Your own outline for the block you look at: color, thickness, fill, rainbow and pulsing.", Category::Visual,
                 {"cosmetic"}) {
        sub("World");
        require(need::target | game::Domain::Camera, need::sigs({"LocalPlayer", "Target"}));
        fillColor_.visible = [this] { return fill_.b; };
        fillFace_.visible = [this] { return fill_.b; };
        color_.visible = [this] { return outline_.b && !rainbow_.b; };
        thickness_.visible = [this] { return outline_.b; };
        speed_.visible = [this] { return rainbow_.b || pulse_.b; };
        glide_.visible = [this] { return smooth_.b; };
    }

    void onFrame() override {
        if (hideVanilla_.b) fx::skip(fx::Id::BlockOutline);
    }

    void onDisable() override { seen_ = false; }

    void onRender(ImDrawList* dl) override {
        auto& t = game::state().target;
        // only while playing: in the inventory, the chat or a menu the outline would lie over the screen
        if (t.kind != game::Target::Kind::Block || game::state().screen != game::Screen::None || gui::open()) {
            seen_ = false;
            return;
        }
        game::Vec3 want{float(t.blockX), float(t.blockY), float(t.blockZ)};
        bool jump = std::fabs(want.x - at_.x) + std::fabs(want.y - at_.y) + std::fabs(want.z - at_.z) > 4.f;
        if (!smooth_.b || !seen_ || jump) at_ = want;
        float k = std::min(1.f, ui::dt() * glide_.f);
        at_ = {at_.x + (want.x - at_.x) * k, at_.y + (want.y - at_.y) * k, at_.z + (want.z - at_.z) * k};
        seen_ = true;

        float g = grow_.f;
        game::Vec3 mn{at_.x - g, at_.y - g, at_.z - g};
        game::Vec3 mx{at_.x + 1.f + g, at_.y + 1.f + g, at_.z + 1.f + g};
        game::Vec3 c[8] = {{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
                           {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}};

        ImVec4 col = rainbow_.b ? rainbow(speed_.f) : color_.color;
        if (pulse_.b) col.w *= 0.65f + 0.35f * std::sin(float(ui::time()) * speed_.f * 4.f);
        ImU32 line = ImGui::GetColorU32(col);

        if (fill_.b) {
            static const int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
            auto& cam = game::state().camera.pos;
            int only = fillFace_.i == 1 ? lookedFace(want) : -1;
            for (int i = 0; i < 6; i++) {
                auto& f = faces[i];
                if (only >= 0 && i != only) continue;
                game::Vec3 ctr{(c[f[0]].x + c[f[2]].x) * 0.5f, (c[f[0]].y + c[f[2]].y) * 0.5f, (c[f[0]].z + c[f[2]].z) * 0.5f};
                game::Vec3 mid{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f};
                game::Vec3 out{ctr.x - mid.x, ctr.y - mid.y, ctr.z - mid.z};
                game::Vec3 toCam{cam.x - ctr.x, cam.y - ctr.y, cam.z - ctr.z};
                if (out.x * toCam.x + out.y * toCam.y + out.z * toCam.z <= 0.f) continue;
                ImVec2 pts[4];
                bool ok = true;
                for (int k = 0; k < 4; k++) {
                    auto p = game::project(c[f[k]]);
                    if (!p) {
                        ok = false;
                        break;
                    }
                    pts[k] = *p;
                }
                if (ok) dl->AddConvexPolyFilled(pts, 4, ImGui::GetColorU32(fillColor_.color));
            }
        }

        if (!outline_.b) return;
        static const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (auto& e : edges) {
            ImVec2 a, b;
            if (game::projectLine(c[e[0]], c[e[1]], a, b)) dl->AddLine(a, b, line, thickness_.f);
        }
    }

private:
    // the ray from the eyes enters the block through the face whose slab it crosses last; index into faces[]
    int lookedFace(game::Vec3 b) const {
        auto& p = game::state().player;
        float yaw = p.yaw * 0.0174533f, pitch = p.pitch * 0.0174533f;
        float d[3] = {-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
        auto e = p.eye();
        float o[3] = {e.x, e.y, e.z}, lo[3] = {b.x, b.y, b.z};
        int axis = -1;
        float enter = -1e9f;
        for (int a = 0; a < 3; a++) {
            if (std::fabs(d[a]) < 1e-6f) continue;
            float t = ((d[a] > 0.f ? lo[a] : lo[a] + 1.f) - o[a]) / d[a];
            if (t > enter) {
                enter = t;
                axis = a;
            }
        }
        if (axis == 0) return d[0] > 0.f ? 5 : 3;
        if (axis == 1) return d[1] > 0.f ? 0 : 1;
        if (axis == 2) return d[2] > 0.f ? 2 : 4;
        return -1;
    }

    Setting& outline_ = toggleSetting("outline", "Draw the outline", true);
    Setting& thickness_ = slider("thickness", "Thickness", 2.f, 1.f, 6.f, "%.1f");
    Setting& grow_ = slider("grow", "Distance to the block", 0.003f, 0.f, 0.05f, "%.3f");
    Setting& color_ = colorSetting("color", "Color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& rainbow_ = toggleSetting("rainbow", "Rainbow", false);
    Setting& pulse_ = toggleSetting("pulse", "Pulse", false);
    Setting& speed_ = slider("speed", "Speed", 1.f, 0.1f, 5.f, "%.1f");
    Setting& fill_ = toggleSetting("fill", "Fill the faces", false);
    Setting& fillColor_ = colorSetting("fillColor", "Fill color", {0.23f, 0.65f, 0.93f, 0.18f});
    Setting& fillFace_ = choice("fillFace", "Fill", {"All visible faces", "Only the face you look at"});
    Setting& smooth_ = toggleSetting("smooth", "Glide to the next block", false);
    Setting& glide_ = slider("glide", "Glide speed", 18.f, 4.f, 40.f, "%.0f");
    Setting& hideVanilla_ = needs(toggleSetting("hideVanilla", "Hide the original outline", true), fx::Id::BlockOutline);
    game::Vec3 at_;
    bool seen_ = false;
};

class ChunkBorder : public Module {
public:
    ChunkBorder()
        : Module("Chunk Border", "Shows the chunk borders around you as lines, with sub-chunks, range and colors.", Category::Visual, {"cosmetic"}) {
        sub("World");
        require(need::player | game::Domain::Camera, need::sigs({"LocalPlayer"}));
        radius_.visible = [this] { return !current_.b; };
        for (Setting* s : {&gridStep_, &gridColor_}) s->visible = [this] { return wall_.b; };
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && key_.i && ev.vk == key_.i) visible_ = !visible_;
    }

    // Like Java's F3+G: yellow lines at the chunk corners and every 16 blocks of height, a fine blue grid on the walls of
    // the chunk you stand in, everything fading out with the distance to you so only what is near stays bright.
    void onRender(ImDrawList* dl) override {
        if (!visible_) return;
        auto& p = game::state().player;
        player_ = p.pos;
        int cx = int(std::floor(p.pos.x / 16.f)), cz = int(std::floor(p.pos.z / 16.f));
        int r = current_.b ? 0 : radius_.i;
        float y0 = std::floor(p.pos.y) - height_.f, y1 = std::floor(p.pos.y) + height_.f;
        reach_ = height_.f + 4.f;

        for (int dx = -r; dx <= r; dx++)
            for (int dz = -r; dz <= r; dz++) {
                bool cur = dx == 0 && dz == 0;
                float x0 = float((cx + dx) * 16), z0 = float((cz + dz) * 16), x1 = x0 + 16.f, z1 = z0 + 16.f;
                if (cur && wall_.b) walls(dl, x0, z0, y0, y1);
                const ImVec4& corner = cur ? cornerColor_.color : neighborColor_.color;
                float w = width_.f * (cur ? 1.6f : 1.f);
                for (auto [x, z] : {std::pair{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}}) line(dl, {x, y0, z}, {x, y1, z}, corner, w);
                if (!subchunks_.b) continue;
                for (float y = std::ceil(y0 / 16.f) * 16.f; y <= y1; y += 16.f) {
                    line(dl, {x0, y, z0}, {x1, y, z0}, levelColor_.color, width_.f);
                    line(dl, {x1, y, z0}, {x1, y, z1}, levelColor_.color, width_.f);
                    line(dl, {x1, y, z1}, {x0, y, z1}, levelColor_.color, width_.f);
                    line(dl, {x0, y, z1}, {x0, y, z0}, levelColor_.color, width_.f);
                }
            }
    }

private:
    // a line cut into short pieces, each as bright as it is close to the player
    void line(ImDrawList* dl, game::Vec3 a, game::Vec3 b, const ImVec4& c, float w) const {
        float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        int n = std::max(1, int(std::ceil(len / 4.f)));
        for (int i = 0; i < n; i++) {
            float t0 = float(i) / float(n), t1 = float(i + 1) / float(n), tm = (t0 + t1) * 0.5f;
            float mx = a.x + dx * tm - player_.x, my = a.y + dy * tm - player_.y, mz = a.z + dz * tm - player_.z;
            float f = 1.f - std::sqrt(mx * mx + my * my + mz * mz) / (reach_ * 2.2f);
            if (f <= 0.04f) continue;
            ImVec2 sa, sb;
            if (game::projectLine({a.x + dx * t0, a.y + dy * t0, a.z + dz * t0}, {a.x + dx * t1, a.y + dy * t1, a.z + dz * t1}, sa, sb))
                dl->AddLine(sa, sb, theme::col(c, f * f), w);
        }
    }

    void walls(ImDrawList* dl, float x0, float z0, float y0, float y1) {
        int step = std::max(1, gridStep_.i);
        float yFrom = std::ceil(y0 / float(step)) * float(step);
        for (int i = step; i < 16; i += step) {
            float o = float(i);
            line(dl, {x0 + o, y0, z0}, {x0 + o, y1, z0}, gridColor_.color, width_.f * 0.6f);
            line(dl, {x0 + o, y0, z0 + 16.f}, {x0 + o, y1, z0 + 16.f}, gridColor_.color, width_.f * 0.6f);
            line(dl, {x0, y0, z0 + o}, {x0, y1, z0 + o}, gridColor_.color, width_.f * 0.6f);
            line(dl, {x0 + 16.f, y0, z0 + o}, {x0 + 16.f, y1, z0 + o}, gridColor_.color, width_.f * 0.6f);
        }
        for (float y = yFrom; y <= y1; y += float(step)) {
            line(dl, {x0, y, z0}, {x0 + 16.f, y, z0}, gridColor_.color, width_.f * 0.6f);
            line(dl, {x0 + 16.f, y, z0}, {x0 + 16.f, y, z0 + 16.f}, gridColor_.color, width_.f * 0.6f);
            line(dl, {x0 + 16.f, y, z0 + 16.f}, {x0, y, z0 + 16.f}, gridColor_.color, width_.f * 0.6f);
            line(dl, {x0, y, z0 + 16.f}, {x0, y, z0}, gridColor_.color, width_.f * 0.6f);
        }
    }

    Setting& key_ = keySetting("toggle", "On/off key", 0);
    Setting& current_ = toggleSetting("current", "Current chunk only", true);
    Setting& radius_ = intSlider("radiusN", "Range (chunks)", 1, 1, 3);
    Setting& height_ = slider("height", "Height above and below you", 14.f, 4.f, 60.f, "%.0f");
    Setting& subchunks_ = toggleSetting("subchunks", "Sub-chunk levels", true);
    Setting& wall_ = toggleSetting("wallGrid", "Grid on the walls of the current chunk", true);
    Setting& gridStep_ = intSlider("gridStep", "Line spacing (blocks)", 2, 1, 8);
    Setting& width_ = slider("width", "Thickness", 1.2f, 0.8f, 3.f, "%.1f");
    Setting& cornerColor_ = colorSetting("cornerColor2", "Corner color", {1.f, 0.86f, 0.2f, 0.95f});
    Setting& levelColor_ = colorSetting("levelColor", "Color levels", {1.f, 0.86f, 0.2f, 0.6f});
    Setting& gridColor_ = colorSetting("gridColor2", "Grid color", {0.35f, 0.62f, 1.f, 0.55f});
    Setting& neighborColor_ = colorSetting("neighborColor", "Color neighbor chunks", {1.f, 0.86f, 0.2f, 0.55f});
    bool visible_ = true;
    game::Vec3 player_{};
    float reach_ = 18.f;
};

class HideHand : public Module {
public:
    HideHand()
        : Module("Hide Hand", "Hides hand and item in first person, for a clear view or screenshots.", Category::Visual, {"cosmetic"}) {
        sub("Model");
        require(0, {fx::sig(fx::Id::HideHand)});
        onlyEmpty_.visible = [] { return game::has(game::Domain::Inventory); };
    }

    void onFrame() override {
        auto& p = game::state().player;
        if (onlyEmpty_.b && game::has(game::Domain::Inventory) && !p.held().empty()) return;
        if (main_.b) fx::skip(fx::Id::HideHand);
        if (offhand_.b) fx::skip(fx::Id::HideOffhand);
    }

private:
    Setting& main_ = toggleSetting("main", "Hide main hand", true);
    Setting& offhand_ = needs(toggleSetting("offhand", "Hide offhand", true), fx::Id::HideOffhand);
    Setting& onlyEmpty_ = toggleSetting("onlyEmpty", "Only with an empty hand", false);
};

class ViewModel : public Module {
public:
    ViewModel()
        : Module("View Model", "Changes the field of view of hand and item in first person, and where the game version allows it their position, size and rotation.", Category::Visual, {"cosmetic"}) {
        sub("Model");
        requireAny({fx::sig(fx::Id::HandMatrix), fx::sig(fx::Id::ItemFov), fx::sig(fx::Id::HandMatrixThird)});
        itemFov_.visible = [this] { return changeFov_.b; };
        changeFov_.visible = [] { return fx::available(fx::Id::ItemFov); };
        third_.visible = [] { return fx::available(fx::Id::HandMatrixThird); };
    }

    void onFrame() override {
        float k = uniform_.f;
        game::Vec3 move{x_.f, y_.f, z_.f}, scale{sx_.f * k, sy_.f * k, sz_.f * k}, rot{rx_.f, ry_.f, rz_.f};
        fx::transform(fx::Id::HandMatrix, move, scale, rot);
        if (third_.b) fx::transform(fx::Id::HandMatrixThird, move, scale, rot);
        if (changeFov_.b) fx::set(fx::Id::ItemFov, itemFov_.f);
    }

private:
    Setting& changeFov_ = toggleSetting("changeFov", "Change the item field of view", false);
    Setting& itemFov_ = slider("itemFov", "Item field of view", 70.f, 30.f, 180.f, "%.0f");
    Setting& third_ = toggleSetting("third", "Also in third person", false);
    Setting& x_ = needs(slider("posX", "Position X", 0.f, -4.f, 4.f, "%.2f"), fx::Id::HandMatrix);
    Setting& y_ = needs(slider("posY", "Position Y", 0.f, -4.f, 4.f, "%.2f"), fx::Id::HandMatrix);
    Setting& z_ = needs(slider("z", "Position Z", 0.f, -4.f, 4.f, "%.2f"), fx::Id::HandMatrix);
    Setting& uniform_ = needs(slider("scale", "Overall size", 1.f, 0.3f, 3.f, "%.2fx"), fx::Id::HandMatrix);
    Setting& sx_ = needs(slider("sx", "Width", 1.f, -3.f, 3.f, "%.2fx"), fx::Id::HandMatrix);
    Setting& sy_ = needs(slider("sy", "Height", 1.f, -3.f, 3.f, "%.2fx"), fx::Id::HandMatrix);
    Setting& sz_ = needs(slider("sz", "Depth", 1.f, -3.f, 3.f, "%.2fx"), fx::Id::HandMatrix);

    Setting& rx_ = needs(slider("rx", "Rotation X", 0.f, -180.f, 180.f, "%.0f°"), fx::Id::HandMatrix);
    Setting& ry_ = needs(slider("ry", "Rotation Y", 0.f, -180.f, 180.f, "%.0f°"), fx::Id::HandMatrix);
    Setting& rz_ = needs(slider("rz", "Rotation Z", 0.f, -180.f, 180.f, "%.0f°"), fx::Id::HandMatrix);
};
