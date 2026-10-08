#pragma once

#include "gui/Gui.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "hook/Input.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/ItemIcons.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/PackList.hpp"
#include "modules/common/Text.hpp"
#include "modules/Manager.hpp"
#include "modules/post/PostFx.hpp"
#include "modules/world/Waypoints.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"
#include "server/Rules.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <format>

class Coordinates : public GameList {
public:
    Coordinates()
        : GameList("Coordinates", "Shows your position, optionally with chunk, biome, facing and Nether conversion.", need::player,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.005f, 0.5f}) {
        sub("Info displays");
        labelSide_.visible = labelGap_.visible = [this] { return labels_.b; };
        nether_.visible = dimFormat_.visible = [] { return need::have("Dimension"); };
        for (Setting* st : {&owName_, &netherName_, &endName_}) st->visible = [this] { return dimFormat_.i == 3 && need::have("Dimension"); };
        biome_.visible = [] { return need::have("Biome"); };
        signs_.visible = [this] { return format_.text.empty() && layout_.i == 1; };
        signY_.visible = [this] { return format_.text.empty() && layout_.i == 1 && signs_.b; };
    }

    void onFrame() override {
        if (hideOriginal_.b) fx::skip(fx::Id::HideCoordinates);
        // the clipboard goes through ImGui, which belongs to the render thread; the key only asks
        if (copyWanted_.exchange(false)) {
            auto& p = game::state().player.pos;
            int x = int(std::floor(p.x)), y = int(std::floor(p.y)), z = int(std::floor(p.z));
            std::string out = copyFormat_.i == 1 ? std::format("{}, {}, {}", x, y, z) : copyFormat_.i == 2 ? std::format("X: {} Y: {} Z: {}", x, y, z) : std::format("{} {} {}", x, y, z);
            ImGui::SetClipboardText(out.c_str());
            notify::push(i18n::tr("Copied"), out, notify::Kind::Ok, 2.f);
        }
    }

    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat) return;
        if (ev.vk == hideKey_.i && hideKey_.i) hidden_.fetch_xor(1);
        if (ev.vk == copyKey_.i && copyKey_.i) copyWanted_ = true;
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& p = game::state().player;
        float y = 0.f, w = 0.f;
        auto line = [&](const std::string& label, const std::string& value, ImU32 col) {
            const std::string shown = labels_.b ? label : "";
            ImVec2 a = shown.empty() ? ImVec2(0, 0) : textSize(s, shown);
            ImVec2 b = textSize(s, value);
            float gap = shown.empty() ? 0.f : labelGap_.f * s;
            float valueX = labelSide_.i == 0 ? a.x + gap : 0.f;
            drawText(dl, o + ImVec2(valueX, y), s, value, col);
            if (!shown.empty()) drawText(dl, o + ImVec2(labelSide_.i == 0 ? 0.f : b.x + gap, y), s, shown, accentColor());
            w = std::max(w, a.x + gap + b.x);
            y += std::max(a.y, b.y);
        };
        if (hidden_.load()) {
            line("XYZ", i18n::tr("hidden"), textColor());
            return {w, y};
        }
        float px = p.pos.x, py = p.pos.y, pz = p.pos.z;
        auto fmt = [&](float v) { return blocks_.b ? std::to_string(int(std::floor(v))) : text::num(v, decimals_.i); };
        auto speed = [&](float v) { return std::format(" ({}{:.1f}/s)", v >= 0.f ? "+" : "-", std::fabs(v)); };
        std::string yText = fmt(py) + (ySpeed_.b ? speed(p.vel.y) : "");
        auto formatted = [&](int dim, const std::string& x, const std::string& yv, const std::string& z) {
            std::string out = format_.text;
            for (auto& [key, val] : {std::pair<const char*, std::string>{"{D}", need::have("Dimension") && dim >= 0 ? dimension(dim) : std::string()}, {"{X}", x}, {"{Y}", yv}, {"{Z}", z}})
                for (size_t at = out.find(key); at != std::string::npos; at = out.find(key, at + val.size())) out.replace(at, std::strlen(key), val);
            return out;
        };
        auto sign = [&](float v) { return !signs_.b ? std::string() : v > 0.01f ? "+" : v < -0.01f ? "-" : " "; };
        if (!format_.text.empty()) {
            line("", formatted(p.dimension, fmt(px), yText, fmt(pz)), textColor());
        } else if (layout_.i == 0) {
            line("XYZ", fmt(px) + " / " + yText + " / " + fmt(pz), textColor());
        } else {
            line(sign(p.vel.x) + "X", fmt(px), ImGui::GetColorU32(xColor_.color));
            line((signY_.b ? sign(p.vel.y) : std::string(signs_.b ? " " : "")) + "Y", yText, ImGui::GetColorU32(yColor_.color));
            line(sign(p.vel.z) + "Z", fmt(pz), ImGui::GetColorU32(zColor_.color));
        }
        if (nether_.b && need::have("Dimension") && p.dimension >= 0 && p.dimension != 2) {
            float k = p.dimension == 1 ? 8.f : 1.f / 8.f;
            int other = p.dimension == 1 ? 0 : 1;
            if (!format_.text.empty()) line("", formatted(other, fmt(px * k), fmt(py), fmt(pz * k)), textColor());
            else line(dimension(other), fmt(px * k) + " / " + fmt(pz * k), textColor());
        }
        if (chunk_.b) line("Chunk", std::format("{} {}", int(std::floor(px / 16.f)), int(std::floor(pz / 16.f))), textColor());
        if (inChunk_.b) line(i18n::tr("In chunk"), std::format("{} {} {}", int(std::floor(px)) & 15, int(std::floor(py)) & 15, int(std::floor(pz)) & 15), textColor());
        if (facing_.b) line(i18n::tr("Facing"), compass(p.yaw), textColor());
        if (biome_.b && need::have("Biome")) line(i18n::tr("Biome"), text::pretty(game::state().world.biome), textColor());
        return {w, y};
    }

private:
    std::string dimension(int d) const {
        static const char* full[] = {"Overworld", "Nether", "The End"};
        static const char* shortName[] = {"OW", "N", "E"};
        int i = std::clamp(d, 0, 2);
        if (dimFormat_.i == 3) {
            std::string out = (i == 0 ? owName_ : i == 1 ? netherName_ : endName_).text;
            if (out.empty()) return i18n::tr(full[i]);
            std::string name = i18n::tr(full[i]);
            for (size_t at = out.find("{dim}"); at != std::string::npos; at = out.find("{dim}", at + name.size())) out.replace(at, 5, name);
            return out;
        }
        if (dimFormat_.i == 1) return i18n::tr(shortName[i]);
        if (dimFormat_.i == 2) return std::to_string(i == 1 ? -1 : i == 2 ? 1 : 0);
        return i18n::tr(full[i]);
    }

    static std::string compass(float yaw) {
        static const char* names[] = {"North", "Northeast", "East", "Southeast", "South", "Southwest", "West", "Northwest"};
        float bearing = std::fmod(yaw + 180.f + 360.f, 360.f);
        return i18n::tr(names[int(std::floor((bearing + 22.5f) / 45.f)) % 8]);
    }

    Setting& labels_ = toggleSetting("label", "Show label", true);
    Setting& labelSide_ = choice("labelSide", "Label position", {"Before value", "After value"});
    Setting& labelGap_ = slider("labelGap", "Label gap", 6.f, 0.f, 32.f, "%.0f");
    Setting& format_ = textSetting("format", "Format ({D} {X} {Y} {Z}, empty = layout)", "");
    Setting& layout_ = choice("layout", "Layout", {"One line", "Stacked"});
    Setting& dimFormat_ = choice("dimFormat", "Dimension name", {"Full name", "Short", "Number", "Custom"});
    Setting& owName_ = textSetting("owName", "Overworld name ({dim})", "{dim}");
    Setting& netherName_ = textSetting("netherName", "Nether name ({dim})", "{dim}");
    Setting& endName_ = textSetting("endName", "End name ({dim})", "{dim}");
    Setting& signs_ = toggleSetting("signs", "Movement direction (+/-) per axis", false);
    Setting& signY_ = toggleSetting("signY", "Also for Y", true);
    Setting& ySpeed_ = toggleSetting("ySpeed", "Vertical speed (+/-)", false);
    Setting& decimals_ = intSlider("decimals", "Decimals", 1, 0, 4);
    Setting& blocks_ = toggleSetting("blocks", "Block coordinates only", false);
    Setting& nether_ = toggleSetting("nether", "Coordinates of the other dimension", false);
    Setting& chunk_ = toggleSetting("chunk", "Chunk", false);
    Setting& inChunk_ = toggleSetting("inChunk", "Position in chunk", false);
    Setting& facing_ = toggleSetting("facing", "Compass direction", false);
    Setting& biome_ = toggleSetting("biome", "Biome", false);
    Setting& xColor_ = colorSetting("xColor", "Color X", {1.f, 0.55f, 0.6f, 1.f});
    Setting& yColor_ = colorSetting("yColor", "Color Y", {0.6f, 1.f, 0.7f, 1.f});
    Setting& zColor_ = colorSetting("zColor", "Color Z", {0.6f, 0.75f, 1.f, 1.f});
    Setting& hideKey_ = keySetting("hideKey", "Hide (streamer)", 0);
    Setting& copyKey_ = keySetting("copyKey", "Copy position to the clipboard", 0);
    Setting& copyFormat_ = choice("copyFormat", "Copy format", {"x y z", "x, y, z", "X: x Y: y Z: z"});
    Setting& hideOriginal_ = needs(toggleSetting("hideOriginal", "Hide the game's own coordinates", false), fx::Id::HideCoordinates);
    std::atomic<unsigned> hidden_{0};
    std::atomic<bool> copyWanted_{false};
};

class DirectionHud : public GameList {
public:
    DirectionHud()
        : GameList("Direction HUD", "Compass with direction, degrees and view angle, as text or as a bar with waypoints.", need::player,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.40f, 0.10f}) {
        sub("Info displays");
        for (Setting* st : {&width_, &pxPerDeg_, &fade_, &ticks_, &letters_, &degrees_, &waypoints_, &arrowColor_})
            st->visible = [this] { return style_.i == 1; };
        fadeStart_.visible = [this] { return style_.i == 1 && fade_.b; };
        for (Setting* st : {&tickStep_, &tickWidth_, &cardinalTickWidth_, &tickShadow_, &tickColor_, &cardinalTickColor_})
            st->visible = [this] { return style_.i == 1 && ticks_.b; };
        for (Setting* st : {&ordinals_, &cardinalSize_, &letterOffset_, &cardinalColor_, &letterShadow_})
            st->visible = [this] { return style_.i == 1 && letters_.b; };
        ordinalSize_.visible = [this] { return style_.i == 1 && letters_.b && ordinals_.b; };
        ordinalColor_.visible = [this] { return style_.i == 1 && letters_.b && ordinals_.b; };
        for (Setting* st : {&degDecimals_, &degSize_, &degOffset_, &degColor_})
            st->visible = [this] { return style_.i == 1 && degrees_.b; };
        wpNames_.visible = [this] { return style_.i == 1 && waypoints_.b; };
        wpDistance_.visible = [this] { return style_.i == 1 && waypoints_.b; };
        wpMax_.visible = [this] { return style_.i == 1 && waypoints_.b; };
    }

protected:
    void onRender(ImDrawList* dl) override {
        if (hideOnTab_.b && input::down(VK_TAB) && !gui::editingHud()) return;
        GameList::onRender(dl);
    }

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& p = game::state().player;
        float bearing = std::fmod(p.yaw + 180.f + 360.f, 360.f);
        static const char* short_[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        const char* dir = i18n::tr(short_[int(std::floor((bearing + 22.5f) / 45.f)) % 8]);
        if (style_.i == 0) {
            std::string t = std::format("{}  {:.0f}°", dir, bearing);
            if (angles_.b) t += std::format("  ·  Yaw {:.1f}  Pitch {:.1f}", p.yaw, p.pitch);
            auto sz = drawText(dl, o, s, t, textColor());
            return {sz.x, sz.y};
        }

        float w = width_.f * s, pxPerDeg = pxPerDeg_.f * s;
        float h = fonts::hudSize() * s * 1.25f;
        ImVec2 a = o + ImVec2(0, 0), b = o + ImVec2(w, h);
        float mid = a.x + w * 0.5f, range = w * 0.5f / pxPerDeg + 6.f;
        float start = std::clamp(fadeStart_.f / 100.f, 0.f, 0.99f);
        auto fade = [&](float x) { return fade_.b ? std::clamp((1.f - std::fabs(x - mid) / (w * 0.5f)) / (1.f - start), 0.f, 1.f) : 1.f; };

        dl->PushClipRect(a, b + ImVec2(0, 1), true);
        int step = tickStep_.i == 0 ? 5 : tickStep_.i == 1 ? 10 : 15;
        int first = int(std::floor((bearing - range) / float(step))) * step;
        for (int deg = first; float(deg) <= bearing + range; deg += step) {
            float x = mid + (float(deg) - bearing) * pxPerDeg;
            int norm = ((deg % 360) + 360) % 360;
            bool cardinal = norm % 90 == 0, ordinal = norm % 45 == 0;
            float al = fade(x);
            if (al <= 0.f) continue;
            if (ticks_.b) {
                float len = cardinal ? 0.55f : ordinal ? 0.45f : 0.25f;
                float tw = (cardinal ? cardinalTickWidth_.f : tickWidth_.f) * s;
                ImVec4 tc = cardinal ? cardinalTickColor_.color : tickColor_.color;
                if (tickShadow_.b) dl->AddLine({x + s, b.y - h * len + s}, {x + s, b.y + s}, ImGui::GetColorU32(withAlpha(letterShadow_.color, al)), tw);
                dl->AddLine({x, b.y - h * len}, {x, b.y}, ImGui::GetColorU32(withAlpha(tc, al * 0.7f)), tw);
            }
            if (!letters_.b || (!cardinal && !(ordinal && ordinals_.b))) continue;
            const char* n = i18n::tr(short_[norm / 45]);
            float size = (cardinal ? cardinalSize_.f : ordinalSize_.f) * s;
            ImFont* f = fonts::hud();
            ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0.f, n);
            ImVec4 c = cardinal ? cardinalColor_.color : ordinalColor_.color;
            float ty = a.y + letterOffset_.f * s;
            ImVec4 sc = withAlpha(letterShadow_.color, al);
            if (sc.w > 0.f) dl->AddText(f, size, {x - ts.x * 0.5f + 1.f, ty + 1.f}, ImGui::GetColorU32(sc), n);
            dl->AddText(f, size, {x - ts.x * 0.5f, ty}, ImGui::GetColorU32(withAlpha(c, al)), n);
        }

        struct Tag {
            float x;
            std::string text;
            ImU32 color;
        };
        std::vector<Tag> tags;
        if (waypoints_.b) {
            if (auto* wp = dynamic_cast<Waypoints*>(modules::find("Waypoints")); wp && wp->userEnabled())
                for (auto& m : wp->marks(p.dimension)) {
                    float dist = game::distance(p.pos, {m.x, m.y, m.z});
                    if (wpMax_.f > 0.f && dist > wpMax_.f) continue;
                    float target = std::atan2(-(m.x + 0.5f - p.pos.x), m.z + 0.5f - p.pos.z) * 57.2958f + 180.f;
                    float diff = std::fmod(target - bearing + 540.f, 360.f) - 180.f;
                    float x = mid + diff * pxPerDeg;
                    float al = fade(x);
                    if (al <= 0.f) continue;
                    ImU32 col = ImGui::GetColorU32({m.color[0], m.color[1], m.color[2], al});
                    float r = 4.f * s;
                    dl->AddQuadFilled({x, b.y - h * 0.5f - r}, {x + r, b.y - h * 0.5f}, {x, b.y - h * 0.5f + r}, {x - r, b.y - h * 0.5f}, col);
                    std::string label;
                    if (wpNames_.b) label = m.name;
                    if (wpDistance_.b) label += (label.empty() ? "" : " ") + std::format("{:.0f} m", dist);
                    if (!label.empty()) tags.push_back({x, label, col});
                }
        }
        dl->PopClipRect();
        for (auto& t : tags) {
            ImVec2 ts = fonts::hud()->CalcTextSizeA(fonts::hudSize() * s * 0.7f, FLT_MAX, 0.f, t.text.c_str());
            if (t.x - ts.x * 0.5f < a.x || t.x + ts.x * 0.5f > b.x) continue;
            dl->AddText(fonts::hud(), fonts::hudSize() * s * 0.7f, {t.x - ts.x * 0.5f, b.y + 3 * s}, t.color, t.text.c_str());
        }

        dl->AddTriangleFilled({mid - 4 * s, b.y + 2 * s}, {mid + 4 * s, b.y + 2 * s}, {mid, b.y - 4 * s}, ImGui::GetColorU32(arrowColor_.color));
        float labelH = waypoints_.b && (wpNames_.b || wpDistance_.b) ? fonts::hudSize() * s * 0.7f + 4 * s : 0.f;
        float y = h + degOffset_.f * s + labelH;
        if (degrees_.b || angles_.b) {
            std::string t = degrees_.b ? text::num(bearing, degDecimals_.i) + "°" : "";
            if (angles_.b) t += (t.empty() ? "" : "  ·  ") + std::format("Pitch {:.1f}", p.pitch);
            float size = fonts::hudSize() * s * degSize_.f;
            ImVec2 sz = fonts::hud()->CalcTextSizeA(size, FLT_MAX, 0.f, t.c_str());
            ImVec2 at = o + ImVec2((w - sz.x) * 0.5f, y);
            if (shadow_.b) dl->AddText(fonts::hud(), size, at + ImVec2(shadowOffset_.f * s, shadowOffset_.f * s), ImGui::GetColorU32(letterShadow_.color), t.c_str());
            dl->AddText(fonts::hud(), size, at, ImGui::GetColorU32(degColor_.color), t.c_str());
            y += sz.y;
        }
        return {w, y};
    }

private:
    Setting& style_ = choice("style", "Display style", {"Text", "Bar"}, 1);
    Setting& width_ = slider("width", "Bar width", 260.f, 120.f, 700.f, "%.0f");
    Setting& pxPerDeg_ = slider("pxPerDeg", "Pixels per degree", 1.45f, 0.5f, 5.f, "%.2f");
    Setting& fade_ = toggleSetting("fade", "Fade out at the edges", true);
    Setting& fadeStart_ = slider("fadeStart", "Fade starts at (% of the width)", 67.f, 0.f, 95.f, "%.0f%%");
    Setting& hideOnTab_ = toggleSetting("hideOnTab", "Hide while the player list is open (Tab)", false);
    Setting& ticks_ = toggleSetting("ticks", "Ticks", true);
    Setting& tickStep_ = choice("tickStep", "Tick spacing", {"5 degrees", "10 degrees", "15 degrees"});
    Setting& tickWidth_ = slider("tickWidth", "Tick thickness", 1.f, 0.5f, 6.f, "%.1f");
    Setting& cardinalTickWidth_ = slider("cardinalTickWidth", "Cardinal tick thickness", 1.f, 0.5f, 8.f, "%.1f");
    Setting& tickShadow_ = toggleSetting("tickShadow", "Tick shadow", false);
    Setting& letters_ = toggleSetting("letters", "Direction letters", true);
    Setting& ordinals_ = toggleSetting("ordinals", "Intercardinal directions", true);
    Setting& cardinalSize_ = slider("cardinalSize", "Cardinal text size", 15.f, 8.f, 30.f, "%.0f");
    Setting& ordinalSize_ = slider("ordinalSize", "Intercardinal text size", 12.f, 8.f, 30.f, "%.0f");
    Setting& letterOffset_ = slider("letterOffset", "Letter offset Y", 0.f, -20.f, 20.f, "%.0f");
    Setting& degrees_ = toggleSetting("degrees", "Degrees under the arrow", true);
    Setting& degDecimals_ = intSlider("degDecimals", "Degree decimals", 0, 0, 3);
    Setting& degSize_ = slider("degSize", "Degree text size", 1.f, 0.5f, 2.5f, "%.2fx");
    Setting& degOffset_ = slider("degOffset", "Degree text offset", 6.f, 0.f, 40.f, "%.0f");
    Setting& angles_ = toggleSetting("angles", "Yaw / Pitch", false);
    Setting& waypoints_ = toggleSetting("waypoints", "Waypoints on the compass", true);
    Setting& wpNames_ = toggleSetting("wpNames", "Waypoint names", true);
    Setting& wpDistance_ = toggleSetting("wpDistance", "Waypoint distance", true);
    Setting& wpMax_ = slider("wpMax", "Waypoint range (0 = unlimited)", 0.f, 0.f, 2000.f, "%.0f");
    Setting& arrowColor_ = colorSetting("arrowColor", "Arrow", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& cardinalColor_ = colorSetting("cardinalColor", "Cardinal text", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& ordinalColor_ = colorSetting("ordinalColor", "Intercardinal text", {0.95f, 0.95f, 0.97f, 1.f});
    Setting& tickColor_ = colorSetting("tickColor", "Ticks", {0.8f, 0.75f, 0.85f, 1.f});
    Setting& cardinalTickColor_ = colorSetting("cardinalTickColor", "Cardinal ticks", {0.8f, 0.75f, 0.85f, 1.f});
    Setting& letterShadow_ = colorSetting("letterShadow", "Text shadow color", {0.f, 0.f, 0.f, 0.55f});
    Setting& degColor_ = colorSetting("degColor", "Degree text", {0.95f, 0.95f, 0.97f, 1.f});
};

class SpeedDisplay : public GameText {
public:
    SpeedDisplay()
        : GameText("Speed Display", "Shows your speed in blocks per second.", need::player, need::sigs({"LocalPlayer"}), {"hud-self"},
                   {0.005f, 0.338f}) {
        sub("Info displays");
    }

    void onFrame() override {
        auto& st = game::state();
        auto& p = st.player.pos;
        if (have_ && st.dt > 0.0) {
            float dx = p.x - last_.x, dy = p.y - last_.y, dz = p.z - last_.z;
            float d = axes_.i == 0 ? std::sqrt(dx * dx + dz * dz) : axes_.i == 1 ? std::fabs(dy) : std::sqrt(dx * dx + dy * dy + dz * dz);
            float v = d / float(st.dt);
            if (v < 200.f) speed_ += (v - speed_) * std::min(1.f, float(st.dt) / std::max(0.02f, smooth_.f));
        }
        last_ = p;
        have_ = true;
        peak_ = std::max(peak_ * (1.f - float(st.dt) * 0.05f), speed_);
    }

protected:
    std::string label() const override { return i18n::tr("Speed"); }

    std::string value() override {
        static const float factor[] = {1.f, 3.6f, 2.23694f};
        static const char* suffix[] = {" b/s", " km/h", " mph"};
        int u = std::clamp(unit_.i, 0, 2);
        std::string out = text::num(speed_ * factor[u], decimals_.i) + (unitLabel_.b ? suffix[u] : "");
        if (showPeak_.b) out += "  ·  Max " + text::num(peak_ * factor[u], decimals_.i);
        return out;
    }

private:
    Setting& axes_ = choice("axes", "Direction", {"Horizontal", "Vertical", "All axes"});
    Setting& unit_ = choice("unit", "Unit", {"Blocks per second", "km/h", "mph"});
    Setting& unitLabel_ = toggleSetting("unitLabel", "Show unit", true);
    Setting& decimals_ = intSlider("decimals", "Decimals", 1, 0, 3);
    Setting& smooth_ = slider("smooth", "Smoothing (s)", 0.15f, 0.02f, 1.f, "%.2f s");
    Setting& showPeak_ = toggleSetting("peak", "Show peak value", false);
    game::Vec3 last_;
    bool have_ = false;
    float speed_ = 0.f;
    float peak_ = 0.f;
};

class LookAngles : public GameText {
public:
    LookAngles()
        : GameText("Look Angles", "Shows yaw and pitch of your view direction.", need::player, need::sigs({"LocalPlayer"}), {"hud-self"}, {0.005f, 0.37f}) {
        sub("Info displays");
    }

protected:
    std::string value() override {
        auto& p = game::state().player;
        std::string out;
        if (yaw_.b) out += "Yaw " + text::num(p.yaw, decimals_.i);
        if (pitch_.b) out += std::string(out.empty() ? "" : "  ·  ") + "Pitch " + text::num(p.pitch, decimals_.i);
        return out.empty() ? "–" : out;
    }

private:
    Setting& yaw_ = toggleSetting("yaw", "Yaw", true);
    Setting& pitch_ = toggleSetting("pitch", "Pitch", true);
    Setting& decimals_ = intSlider("decimals", "Decimals", 1, 0, 3);
};

class HealthDisplay : public GameText {
public:
    HealthDisplay()
        : GameText("Health Display", "Shows your health as a number, hearts or bar, with absorption.", need::player, need::sigs({"LocalPlayer", "PlayerStats"}),
                   {"hud-self"}, {0.005f, 0.466f}) {
        sub("Info displays");
    }

protected:
    std::string label() const override { return i18n::tr("Health"); }

    std::string value() override {
        auto& p = game::state().player;
        std::string out = style_.i == 0 ? std::format("{:.1f} / {:.0f}", p.health, p.maxHealth) : std::format("{:.1f} ♥", p.health / 2.f);
        if (absorb_.b && p.absorption > 0.f) out += style_.i == 0 ? std::format(" +{:.0f}", p.absorption) : std::format(" +{:.1f}", p.absorption / 2.f);
        return out;
    }

    ImU32 valueColor() const override {
        auto& p = game::state().player;
        if (!colored_.b) return textColor();
        float f = p.maxHealth > 0.f ? p.health / p.maxHealth : 1.f;
        return ImGui::GetColorU32(rampColor(1.f - f, 0.f, 1.f, good_.color, mid_.color, low_.color));
    }

private:
    Setting& style_ = choice("style", "Display", {"Number", "Hearts"});
    Setting& absorb_ = toggleSetting("absorb", "Absorption", true);
    Setting& colored_ = toggleSetting("colored", "Color by health", true);
    Setting& good_ = colorSetting("good", "Full", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Half", {1.f, 0.82f, 0.49f, 1.f});
    Setting& low_ = colorSetting("low", "Low", {1.f, 0.4f, 0.45f, 1.f});
};

class ExperienceInfo : public GameList {
public:
    ExperienceInfo()
        : GameList("Experience Info", "Shows your level and the progress to the next level.", need::player, need::sigs({"LocalPlayer", "PlayerStats"}),
                   {"hud-self"}, {0.135f, 0.434f}) {
        sub("Info displays");
        percent_.visible = [this] { return mode_.i == 0 || mode_.i == 1; };
        bar_.visible = [this] { return mode_.i != 2 && mode_.i != 3; };
        remaining_.visible = [this] { return mode_.i == 5 || mode_.i == 6; };
        decimalLevel_.visible = [this] { return mode_.i == 6; };
        color_.visible = [this] { return mode_.i == 1 || (mode_.i != 2 && mode_.i != 3 && bar_.b); };
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& p = game::state().player;
        static auto needed = [](int level) { return level <= 15 ? 2 * level + 7 : level <= 30 ? 5 * level - 38 : 9 * level - 158; };
        std::string text = i18n::fmt("Level {}", p.level);
        bool showBar = bar_.b;
        switch (mode_.i) {
        case 1: showBar = true; break;
        case 2:
            text = i18n::fmt("Level {}  ·  {:.0f} points to the next level", p.level, (1.f - p.xp) * float(needed(p.level)));
            showBar = false;
            break;
        case 3: {
            int total = p.level <= 16 ? p.level * p.level + 6 * p.level : p.level <= 31 ? int(2.5f * float(p.level * p.level) - 40.5f * float(p.level) + 360.f) : int(4.5f * float(p.level * p.level) - 162.5f * float(p.level) + 2220.f);
            text = i18n::fmt("{} points in total  ·  level {}", total + int(p.xp * float(needed(p.level))), p.level);
            showBar = false;
            break;
        }
        case 4: text = i18n::fmt("Level {:.2f}", float(p.level) + std::clamp(p.xp, 0.f, 1.f)); break;
        case 5:
        case 6: {
            float progress = std::clamp(p.xp, 0.f, 1.f);
            int next = needed(p.level), have = int(progress * float(next));
            std::string counts = std::format("{} / {}", have, next);
            if (mode_.i == 6) {
                std::string level = decimalLevel_.b ? std::format("{:.2f}", float(p.level) + progress) : std::to_string(p.level);
                counts = i18n::fmt("Level {}  ·  {}  ·  {:.1f}%", level, counts, progress * 100.f);
            }
            text = remaining_.b ? i18n::fmt("{}  ·  {} left", counts, next - have) : counts;
            break;
        }
        default: break;
        }
        if ((mode_.i == 0 || mode_.i == 1) && percent_.b) text += std::format("  ·  {:.0f}%", p.xp * 100.f);
        float bh = barHeight_.f * s, w = std::max(textSize(s, text).x, barWidth_.f * s), y = 0.f;
        auto bar = [&] {
            if (!showBar) return;
            ImVec2 b0 = o + ImVec2(0, y + 2 * s);
            dl->AddRectFilled(b0, b0 + ImVec2(w, bh), IM_COL32(0, 0, 0, 80), bh * 0.5f);
            dl->AddRectFilled(b0, b0 + ImVec2(w * std::clamp(p.xp, 0.f, 1.f), bh), ImGui::GetColorU32(color_.color), bh * 0.5f);
            y += bh + 4 * s;
        };
        if (barAbove_.b) bar();
        if (!hideText_.b) y += drawText(dl, o + ImVec2(0, y), s, text, textColor()).y;
        if (!barAbove_.b) bar();
        return {w, std::max(y, bh)};
    }

private:
    Setting& mode_ = choice("mode", "Mode", {"Level and bar", "Level, bar and percent", "Points to next level", "Total points", "Decimal level", "Points in this level", "Everything"});
    Setting& percent_ = toggleSetting("percent", "Percent", true);
    Setting& bar_ = toggleSetting("bar", "Bar", true);
    Setting& remaining_ = toggleSetting("remaining", "Points left to the next level", false);
    Setting& decimalLevel_ = toggleSetting("decimalLevel", "Level with decimals", false);
    Setting& color_ = colorSetting("color", "Bar color", {0.55f, 0.95f, 0.45f, 1.f});
    Setting& barHeight_ = slider("barHeight", "Bar height", 5.f, 2.f, 16.f, "%.0f");
    Setting& barWidth_ = slider("barWidth", "Minimum width", 110.f, 40.f, 320.f, "%.0f");
    Setting& barAbove_ = toggleSetting("barAbove", "Bar above the text", false);
    Setting& hideText_ = toggleSetting("hideText", "Bar only, no text", false);
};

class DayCounter : public GameText {
public:
    DayCounter()
        : GameText("Day Counter", "Shows the game day and the world time.", need::world, need::sigs({"WorldTime"}), {"hud-self"}, {0.005f, 0.434f}) {
        sub("Info displays");
    }

    void onFrame() override {
        if (hideOriginal_.b) fx::skip(fx::Id::HideDayCounter);
    }

protected:
    std::string label() const override { return i18n::tr("Day"); }

    std::string value() override {
        auto& w = game::state().world;
        std::string out = std::to_string(w.day);
        if (time_.b) {
            int hours = (w.time / 1000 + 6) % 24, minutes = (w.time % 1000) * 60 / 1000;
            out += twelve_.b ? std::format("  ·  {}:{:02} {}", hours % 12 == 0 ? 12 : hours % 12, minutes, hours >= 12 ? "PM" : "AM")
                             : std::format("  ·  {:02}:{:02}", hours, minutes);
        }
        if (phase_.b) out += std::format("  ·  {}", w.time >= 13000 && w.time < 23000 ? i18n::tr("Night") : i18n::tr("Day"));
        return out;
    }

private:
    Setting& time_ = toggleSetting("time", "Game time", true);
    Setting& twelve_ = toggleSetting("twelve", "12-hour format", false);
    Setting& phase_ = toggleSetting("phase", "Day or night", false);
    Setting& hideOriginal_ = needs(toggleSetting("hideOriginal", "Hide the game's own day counter", false), fx::Id::HideDayCounter);
};

class IpDisplay : public TextHud {
public:
    IpDisplay() : TextHud("IP Display", "Shows the server address. Can be hidden for streamers.", {"hud-self"}, {0.005f, 0.274f}) {
        sub("Info displays");
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && ev.vk == hideKey_.i && hideKey_.i) hidden_ = !hidden_;
    }

    void onRender(ImDrawList* dl) override {
        TextHud::onRender(dl);
    }

protected:
    std::string label() const override { return "IP"; }

    std::string value() override {
        auto st = rules::status();
        std::string v = mode_.i == 1 ? st.ip : st.host;
        if (v.empty()) v = st.ip;
        if (v.empty()) return i18n::tr("No server address");
        if (hidden_ || mask_.b) return std::string(std::min<size_t>(v.size(), 14), '*');
        return v;
    }

private:
    Setting& mode_ = choice("mode", "Display", {"Address", "IP"});
    Setting& mask_ = toggleSetting("mask", "Always as stars", false);
    Setting& hideKey_ = keySetting("hideKey", "Hide with a key", 0);
    bool hidden_ = false;
};

class PackDisplay : public GameList {
public:
    PackDisplay()
        : GameList("Pack Display", "Shows your active global resource packs. Packs a server forces on top are not listed.", 0, {}, {"hud-self"},
                   {0.26f, 0.54f}) {
        sub("Info displays");
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        const auto& packs = game::demo() ? game::state().world.packs : packlist::active();
        float y = 0.f, w = 0.f;
        int shown = 0;
        for (auto& p : packs) {
            if (shown++ >= max_.i) break;
            std::string line = (numbered_.b ? std::format("{}. ", shown) : std::string()) + prefix_.text + p;
            auto sz = drawText(dl, o + ImVec2(0, y), s, line, shown == 1 && firstAccent_.b ? accentColor() : textColor());
            w = std::max(w, sz.x);
            y += sz.y * lineGap_.f;
        }
        if (packs.empty()) {
            if (!emptyNote_.b && !gui::editingHud()) return {0.f, 0.f};
            auto sz = drawText(dl, o, s, i18n::tr("No pack"), textColor());
            return sz;
        }
        return {w, y};
    }

private:
    Setting& max_ = intSlider("max", "Show at most", 4, 1, 10);
    Setting& numbered_ = toggleSetting("numbered", "Number the packs", false);
    Setting& prefix_ = textSetting("prefix", "Text in front of each pack", "");
    Setting& firstAccent_ = toggleSetting("firstAccent", "Top pack in accent color", false);
    Setting& lineGap_ = slider("lineGap", "Line spacing", 1.f, 0.8f, 1.8f, "%.2fx");
    Setting& emptyNote_ = toggleSetting("emptyNote", "Show a note when no pack is active", true);
};

class HeldItem : public GameList {
public:
    HeldItem()
        : GameList("Held Item", "Shows the item in your hand with count and durability.", need::inventory, need::sigs({"LocalPlayer"}),
                   {"hud-self"}, {0.135f, 0.5f}) {
        sub("Info displays");
    }

    void onEnable() override { captured_ = false; }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        if (follow_.b || !captured_) {
            item_ = game::state().player.held();
            captured_ = true;
        }
        const auto& it = item_;
        if (it.empty()) return emptyNote_.b || gui::editingHud() ? drawText(dl, o, s, i18n::tr("Empty hand"), textColor()) : ImVec2{};
        float iconSize = iconSize_.f * s;
        float offset = icon_.b ? iconSize + iconGap_.f * s : 0.f;
        if (icon_.b) itemicon::draw(dl, o, iconSize, it.name);
        float w = icon_.b ? iconSize : 0.f, y = 0.f;
        if (name_.b) {
            std::string name = text::pretty(it.name);
            if (count_.b && it.count > 1) name += std::format(" {}{}", countText_.text, it.count);
            auto size = drawText(dl, o + ImVec2(offset, 0), s, name, it.enchanted && glint_.b ? accentColor() : textColor());
            w = std::max(w, offset + size.x);
            y = size.y;
        } else if (count_.b && it.count > 1) {
            auto size = drawText(dl, o + ImVec2(offset, 0), s, std::format("{}{}", countText_.text, it.count), textColor());
            w = std::max(w, offset + size.x);
            y = size.y;
        }
        if (it.maxDamage > 0 && durability_.b) {
            auto d = drawText(dl, o + ImVec2(offset, y), s, percent_.b ? std::format("{:.0f}%", it.fraction() * 100.f) : std::format("{} / {}", it.left(), it.maxDamage),
                              ImGui::GetColorU32(rampColor(1.f - it.fraction(), 0.f, 1.f, ok_.color, warn_.color, bad_.color)));
            w = std::max(w, offset + d.x);
            y += d.y;
        }
        return {w, std::max(y, icon_.b ? iconSize : 0.f)};
    }

private:
    game::Item item_;
    bool captured_ = false;
    Setting& follow_ = toggleSetting("followSelected", "Follow selected item", true);
    Setting& icon_ = toggleSetting("icon", "Show item icon", true);
    Setting& name_ = toggleSetting("showName", "Show item name", false);
    Setting& iconSize_ = slider("iconSize", "Icon size", 32.f, 16.f, 64.f, "%.0f");
    Setting& count_ = toggleSetting("count", "Amount", true);
    Setting& durability_ = toggleSetting("durability", "Durability", true);
    Setting& percent_ = toggleSetting("percent", "As percent", false);
    Setting& glint_ = toggleSetting("glint", "Highlight enchanted", true);
    Setting& iconGap_ = slider("iconGap", "Gap behind the icon", 6.f, 0.f, 24.f, "%.0f");
    Setting& countText_ = textSetting("countText", "Text in front of the amount", "x");
    Setting& emptyNote_ = toggleSetting("emptyNote", "Show a note for an empty hand", true);
    Setting& ok_ = colorSetting("ok", "Color full", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& warn_ = colorSetting("warn", "Color half", {1.f, 0.82f, 0.49f, 1.f});
    Setting& bad_ = colorSetting("bad", "Color almost broken", {1.f, 0.4f, 0.45f, 1.f});
};

class BreakProgress : public Module {
public:
    BreakProgress()
        : Module("Break Progress", "Shows the break progress of the block you are mining at the crosshair, as bar, ring or text (Block Break Indicator).", Category::Visual, {"hud-self"}) {
        sub("World");
        require(need::target, need::sigs({"LocalPlayer", "Target"}));
        for (Setting* bar : {&direction_, &thickness_, &rounding_, &background_, &border_, &blur_})
            bar->visible = [this] { return style_.i == 0; };
        borderColor_.visible = borderWidth_.visible = [this] { return style_.i == 0 && border_.b; };
        glowSize_.visible = [this] { return glow_.b && style_.i != 2; };
    }

    void onRender(ImDrawList* dl) override {
        auto& t = game::state().target;
        bool active = t.kind == game::Target::Kind::Block && t.breakProgress > 0.f;
        shown_ = draw::approach(shown_, active || always_.b ? 1.f : 0.f, 16.f);
        if (active) target_ = t.breakProgress;
        else if (always_.b) target_ = 0.f;
        // the game resets progress to 0 when a block breaks; easing down would show a fake drain
        last_ = smooth_.b && target_ > last_ ? draw::approach(last_, target_, 18.f) : target_;
        if (shown_ < 0.02f) return;
        auto ds = ImGui::GetIO().DisplaySize;
        float s = ui::scale();
        ImVec2 c{std::floor(ds.x * 0.5f) + 0.5f + offsetX_.f, std::floor(ds.y * 0.5f) + 0.5f + offsetY_.f};
        ImVec4 col = theme::mix(color_.color, doneColor_.color, last_ * last_);
        col.w *= shown_;
        if (style_.i == 2) {
            std::string t = std::format("{:.0f}%", last_ * 100.f);
            ImVec2 ts = fonts::bold()->CalcTextSizeA(18.f * s, FLT_MAX, 0.f, t.c_str());
            float ax = align_.i == 0 ? ts.x * 0.5f : align_.i == 2 ? -ts.x * 0.5f : 0.f;
            draw::textCentered(dl, fonts::bold(), 18.f * s, c + ImVec2(ax, 0.f), ImGui::GetColorU32(col), t.c_str());
            return;
        }
        if (style_.i == 0) {
            drawBar(dl, c, s, col);
        } else {
            float r = width_.f * 0.2f * s;
            if (glow_.b) {
                ImVec4 g = col;
                g.w *= 0.5f;
                draw::glow(dl, c - ImVec2(r, r), c + ImVec2(r, r), r, ImGui::GetColorU32(g), glowSize_.f * s);
            }
            dl->AddCircle(c, r, IM_COL32(0, 0, 0, int(110 * shown_)), 32, 4 * s);
            dl->PathArcTo(c, r, -1.5708f, -1.5708f + 6.2832f * last_, 32);
            dl->PathStroke(ImGui::GetColorU32(col), 0, 3 * s);
        }
        if (percent_.b) draw::textCentered(dl, fonts::bold(), 12.f * s, c + ImVec2(0, style_.i == 0 ? 16 * s : 0.f), ImGui::GetColorU32(col),
                                           std::format("{:.0f}%", last_ * 100.f).c_str());
    }

private:
    void drawBar(ImDrawList* dl, ImVec2 c, float s, ImVec4 col) {
        bool vertical = direction_.i == 1;
        float len = width_.f * s, h = thickness_.f * s;
        ImVec2 size = vertical ? ImVec2(h, len) : ImVec2(len, h);
        float shift = (vertical ? h : len) * 0.5f;
        c.x += align_.i == 0 ? shift : align_.i == 2 ? -shift : 0.f;
        ImVec2 lo = c - ImVec2(size.x * 0.5f, vertical ? size.y * 0.5f : 0.f), hi = lo + size;
        float r = rounding_.f * h * 0.5f;
        if (glow_.b) {
            ImVec4 g = col;
            g.w *= 0.5f;
            draw::glow(dl, lo, hi, r, ImGui::GetColorU32(g), glowSize_.f * s);
        }
        if (blur_.b) post::blur(dl, lo, hi, r, 8.f * s, {0.f, 0.f, 0.f, 0.f});
        ImVec4 bg = background_.color;
        bg.w *= shown_;
        dl->AddRectFilled(lo, hi, ImGui::GetColorU32(bg), r);
        ImVec2 fillLo = vertical ? ImVec2(lo.x, hi.y - size.y * last_) : lo;
        ImVec2 fillHi = vertical ? hi : ImVec2(lo.x + size.x * last_, hi.y);
        if (last_ > 0.f) dl->AddRectFilled(fillLo, fillHi, ImGui::GetColorU32(col), r);
        if (!border_.b) return;
        ImVec4 b = borderColor_.color;
        b.w *= shown_;
        dl->AddRect(lo, hi, ImGui::GetColorU32(b), r, 0, borderWidth_.f * s);
    }

    Setting& style_ = choice("style", "Shape", {"Bar", "Ring", "Text only"});
    Setting& align_ = choice("align", "Alignment to the crosshair", {"Left", "Center", "Right"}, 1);
    Setting& width_ = slider("width", "Size", 70.f, 30.f, 200.f, "%.0f");
    Setting& offsetX_ = slider("offsetX", "Offset X", 0.f, -300.f, 300.f, "%.0f");
    Setting& offsetY_ = slider("offsetY", "Offset Y", 40.f, -300.f, 300.f, "%.0f");
    Setting& percent_ = toggleSetting("percent", "Percent", true);
    Setting& color_ = colorSetting("color", "Color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& doneColor_ = colorSetting("done", "Color when almost done", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& direction_ = choice("direction", "Bar direction", {"Horizontal", "Vertical"});
    Setting& thickness_ = slider("thickness", "Thickness", 5.f, 2.f, 30.f, "%.0f");
    Setting& rounding_ = slider("rounding", "Rounding", 1.f, 0.f, 1.f, "%.2f");
    Setting& background_ = colorSetting("background", "Background color", {0.f, 0.f, 0.f, 0.43f});
    Setting& border_ = toggleSetting("border", "Border", false);
    Setting& borderColor_ = colorSetting("borderColor", "Border color", {1.f, 1.f, 1.f, 0.6f});
    Setting& borderWidth_ = slider("borderWidth", "Border width", 1.f, 0.5f, 4.f, "%.1f");
    Setting& blur_ = toggleSetting("blur", "Blur behind", false);
    Setting& glow_ = toggleSetting("glow", "Glow", false);
    Setting& glowSize_ = slider("glowSize", "Glow size", 8.f, 2.f, 30.f, "%.0f");
    Setting& smooth_ = toggleSetting("smooth", "Smooth fill", true);
    Setting& always_ = toggleSetting("always", "Show while not breaking", false);
    float shown_ = 0.f;
    float last_ = 0.f;
    float target_ = 0.f;
};
