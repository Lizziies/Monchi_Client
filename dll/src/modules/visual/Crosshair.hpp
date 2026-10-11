#pragma once

#include "core/Bg.hpp"
#include "core/Log.hpp"
#include "core/Config.hpp"
#include "core/Paths.hpp"
#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "hook/Input.hpp"
#include "modules/Module.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/GameTexture.hpp"
#include "modules/common/Image.hpp"
#include "modules/common/Layout.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Png.hpp"
#include "render/Draw.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

class Crosshair : public Module {
public:
    Crosshair()
        : Module("Custom Crosshair",
                 "Your own crosshair with many shapes, a pixel editor and dynamics.",
                 Category::Visual, {"cosmetic"}) {
        sub("Crosshair");
        style_.hidden = true;
        grid_.hidden = true;
        if (grid_.text.size() != cells * cells) grid_.text = preset(0);

        size_.visible = [this] { return style_.i != 9; };
        gap_.visible = [this] { return style_.i == 0 || style_.i == 3 || style_.i == 5; };
        thickness_.visible = [this] { return style_.i != 9; };
        cell_.visible = [this] { return style_.i == 9; };
        outlineColor_.visible = [this] { return outline_.b; };
        activeColor_.visible = [this] { return clickColor_.b; };
        moveSpread_.visible = [this] { return dynamic_.b; };
        jumpSpread_.visible = [this] { return dynamic_.b; };
        sneakShrink_.visible = [this] { return dynamic_.b; };
        clickSpread_.visible = [this] { return dynamic_.b; };
        spinSpeed_.visible = [this] { return spin_.b; };
        rainbowSpeed_.visible = [this] { return rainbow_.b; };
        color_.visible = [this] { return !rainbow_.b; };
        targetColor_.visible = [this] { return targetOn_.b; };
        playersOnly_.visible = [this] { return targetOn_.b && need::have("TargetInfo"); };
        imagePath_.visible = [this] { return style_.i == 10; };
        imageScale_.visible = [this] { return style_.i == 10; };
        imageTint_.visible = [this] { return style_.i == 10; };
        imageTintColor_.visible = [this] { return style_.i == 10 && imageTint_.b; };
        size_.visible = [this] { return style_.i != 9 && style_.i != 10 && style_.i != solid && style_.i != game; };
        thickness_.visible = [this] { return style_.i != 9 && style_.i != 10 && style_.i != solid && style_.i != game; };
        gameScale_.visible = [this] { return style_.i == game; };
        solidArm_.visible = [this] { return style_.i == solid; };
        solidThick_.visible = [this] { return style_.i == solid; };
        guiScale_.visible = [this] { return style_.i == solid; };
        outlineWidth_.visible = [this] { return outline_.b && style_.i != solid; };
        outline_.visible = [this] { return style_.i != solid || !covering(); };
        loadedFor_ = "";
    }

    void onFrame() override {
        auto& st = game::state();
        bool visible = st.inWorld && (!hideThird_.b || st.player.view == game::View::First) &&
            (!hideScreens_.b || st.screen == game::Screen::None);
        nativeHide_ = flarialModules::hideCrosshair(hideVanilla_.b && visible);
        if (hideVanilla_.b && !nativeHide_) fx::skip(fx::Id::HideCrosshair);
    }

    void onDisable() override { flarialModules::hideCrosshair(false); nativeHide_ = false; }

    void onRender(ImDrawList* dl) override {
        if (gui::editingHud()) return;
        auto& st = game::state();
        if (game::has(game::Domain::Player)) {
            if (hideThird_.b && st.player.view != game::View::First) return;
            if (hideScreens_.b && st.screen != game::Screen::None) return;
        }
        auto ds = ImGui::GetIO().DisplaySize;
        center_ = {std::floor(ds.x * 0.5f) + 0.5f + offsetX_.f, std::floor(ds.y * 0.5f) + 0.5f + offsetY_.f};

        bool clicking = input::down(VK_LBUTTON);
        pulse_ = draw::approach(pulse_, clicking ? 1.f : 0.f, clicking ? 40.f : 10.f);
        spread_ = draw::approach(spread_, spreadTarget(clicking), 14.f);
        if (spin_.b) angle_ = std::fmod(angle_ + ui::dt() * spinSpeed_.f, 360.f);
        else angle_ = 0.f;
        rad_ = (rotation_.f + angle_) * 0.0174533f;

        ImVec4 col = baseColor();
        bool onPlayer = targetOn_.b && game::has(game::Domain::Target) && st.target.kind == game::Target::Kind::Entity && (!playersOnly_.b || !need::have("TargetInfo") || st.target.isPlayer);
        targetMix_ = draw::approach(targetMix_, onPlayer ? 1.f : 0.f, 18.f);
        if (targetMix_ > 0.001f) col = lerp(col, targetColor_.color, targetMix_);
        if (clickColor_.b) col = lerp(col, activeColor_.color, pulse_);
        col.w *= opacity_.f;
        float size = size_.f * (1.f + (clickPulse_.b ? pulse_ * 0.25f : 0.f));

        ImVec4 oc = outlineColor_.color;
        oc.w *= opacity_.f;
        if (style_.i == solid) {
            if (covering()) oc.w = 1.f;
            solidCross(dl, ds, ImGui::GetColorU32(col), ImGui::GetColorU32(oc));
            return;
        }
        if (outline_.b) shape(dl, size, thickness_.f + outlineWidth_.f * 2.f, ImGui::GetColorU32(oc), true);
        shape(dl, size, thickness_.f, ImGui::GetColorU32(col), false);
    }

    Preview preview(ImVec2& min, ImVec2& max) const override {
        auto ds = ImGui::GetIO().DisplaySize;
        ImVec2 c{std::floor(ds.x * 0.5f) + 0.5f + offsetX_.f, std::floor(ds.y * 0.5f) + 0.5f + offsetY_.f};
        min = c - ImVec2(110.f, 46.f);
        max = c + ImVec2(110.f, 46.f);
        return Preview::Zoomed;
    }

    void drawSettingsTop() override {
        ImGui::Spacing();
        ImGui::TextUnformatted(i18n::tr("Shape"));
        for (int i = 0; i < int(style_.choices.size()); ++i) {
            ImGui::PushID(i);
            if (ImGui::RadioButton(i18n::tr(style_.choices[i].c_str()), style_.i == i)) {
                style_.i = i;
                config::markDirty();
            }
            ImGui::PopID();
            if (i % 3 != 2) ImGui::SameLine();
        }
        ImGui::NewLine();
        sizeSteps();
        if (style_.i == game) gameInfo();
        if (covering() && style_.i != solid) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::current().warn);
            ImGui::TextWrapped("%s", i18n::tr("The original crosshair cannot be hidden on this version yet, so it shows through. Pick the shape \"Solid cross\" to cover it cleanly."));
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
        if (style_.i == 10) {
            if (ImGui::SmallButton(i18n::tr("Paste path from clipboard"))) {
                if (const char* clip = ImGui::GetClipboardText()) imagePath_.text = clip;
            }
            ImGui::TextDisabled("%s", i18n::tr("Use a PNG with a transparent background, up to 64 pixels."));
            if (!status_.empty()) ImGui::TextColored(theme::current().warn, "%s", status_.c_str());
        }
        // the editor shows with the shape it belongs to, so what is drawn is what is on the screen
        if (style_.i == 9) editor();
        ImGui::Spacing();
    }

private:
    bool nativeHide_ = false;
    static constexpr int cells = 15;
    static constexpr int solid = 11;
    static constexpr int game = 12;
    // the original cross measured in GUI pixels, with a margin so the cover never shows an edge
    static constexpr float coverArm = 8.f;
    static constexpr float coverThick = 1.f;

    bool covering() const { return hideVanilla_.b && !nativeHide_ && !fx::available(fx::Id::HideCrosshair); }

    void solidCross(ImDrawList* dl, ImVec2 display, ImU32 col, ImU32 edgeCol) {
        float k = layout::guiScale(display, guiScale_.f);
        bool cover = covering();
        float arm = solidArm_.f + spread_ / k;
        float th = solidThick_.f;
        if (cover) {
            arm = std::max(arm, coverArm);
            th = std::max(th, coverThick);
        }
        ImVec2 c{std::round(center_.x - 0.5f), std::round(center_.y - 0.5f)};
        auto bar = [&](float hx, float hy, ImU32 color) {
            ImVec2 a = ImVec2(std::round(c.x - hx), std::round(c.y - hy)), b = ImVec2(std::round(c.x + hx), std::round(c.y + hy));
            if (rad_ == 0.f) {
                dl->AddRectFilled(a, b, color);
                return;
            }
            ImVec2 q[4] = {turn({a.x, a.y}), turn({b.x, a.y}), turn({b.x, b.y}), turn({a.x, b.y})};
            dl->AddConvexPolyFilled(q, 4, color);
        };
        float hAxis = arm * k, hThick = th * k * 0.5f;
        bool edge = outline_.b || cover;
        if (edge) {
            float e = k;
            bar(hAxis + e, hThick + e, edgeCol);
            bar(hThick + e, hAxis + e, edgeCol);
        }
        bar(hAxis, hThick, col);
        bar(hThick, hAxis, col);
    }

    static ImVec4 lerp(ImVec4 a, ImVec4 b, float t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    }

    ImVec4 baseColor() const {
        if (!rainbow_.b) return color_.color;
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(std::fmod(float(ui::time()) * rainbowSpeed_.f * 0.2f, 1.f), 0.55f, 1.f, r, g, b);
        return {r, g, b, color_.color.w};
    }

    float spreadTarget(bool clicking) const {
        if (!dynamic_.b) return 0.f;
        float t = 0.f;
        if (input::down('W') || input::down('A') || input::down('S') || input::down('D')) t += moveSpread_.f;
        if (input::down(VK_SPACE)) t += jumpSpread_.f;
        if (input::down(VK_LSHIFT)) t -= sneakShrink_.f;
        if (clicking) t += clickSpread_.f;
        return t;
    }

    ImVec2 turn(ImVec2 p) const {
        if (rad_ == 0.f) return p;
        float s = std::sin(rad_), c = std::cos(rad_);
        ImVec2 d = p - center_;
        return center_ + ImVec2(d.x * c - d.y * s, d.x * s + d.y * c);
    }

    void line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float th) const { dl->AddLine(turn(a), turn(b), col, th); }

    void cross(ImDrawList* dl, float size, float th, ImU32 col, bool tShape) const {
        ImVec2 c = center_;
        float g = std::max(0.f, gap_.f + spread_);
        float out = size + spread_;
        line(dl, {c.x - out, c.y}, {c.x - g, c.y}, col, th);
        line(dl, {c.x + g, c.y}, {c.x + out, c.y}, col, th);
        if (!tShape) line(dl, {c.x, c.y - out}, {c.x, c.y - g}, col, th);
        line(dl, {c.x, c.y + g}, {c.x, c.y + out}, col, th);
    }

    void polygon(ImDrawList* dl, int n, float radius, float th, ImU32 col, float start) const {
        ImVec2 pts[8];
        for (int i = 0; i < n; i++) {
            float a = start + 6.2832f * i / n;
            pts[i] = turn(center_ + ImVec2(std::cos(a), std::sin(a)) * radius);
        }
        dl->AddPolyline(pts, n, col, ImDrawFlags_Closed, th);
    }

    void grid(ImDrawList* dl, ImU32 mainCol, ImU32 altCol, bool outline) const {
        float cs = cell_.f * (1.f + spread_ * 0.05f);
        float half = cells * cs * 0.5f;
        float ow = outlineWidth_.f;
        // a grid of another size comes from an older config
        if (grid_.text.size() != size_t(cells * cells)) return;
        for (int y = 0; y < cells; y++)
            for (int x = 0; x < cells; x++) {
                char v = grid_.text[y * cells + x];
                if (v == '0') continue;
                ImVec2 a = center_ + ImVec2(x * cs - half, y * cs - half);
                ImVec2 b = a + ImVec2(cs, cs);
                if (outline) {
                    dl->AddRectFilled(a - ImVec2(ow, ow), b + ImVec2(ow, ow), mainCol);
                    continue;
                }
                ImU32 c = v == '2' ? altCol : v == '3' ? IM_COL32(0, 0, 0, 230) : mainCol;
                dl->AddRectFilled(a, b, c);
            }
    }

    void shape(ImDrawList* dl, float size, float th, ImU32 col, bool outline) {
        ImVec4 secondary = activeColor_.color;
        secondary.w *= opacity_.f;
        ImU32 alt = ImGui::GetColorU32(secondary);
        switch (style_.i) {
        case 0: cross(dl, size, th, col, false); break;
        case 1: dl->AddCircleFilled(center_, th * 1.2f + (outline ? outlineWidth_.f : 0), col); break;
        case 2: dl->AddCircle(center_, (size + spread_) * 0.6f, col, 32, th); break;
        case 3:
            cross(dl, size, th, col, false);
            dl->AddCircleFilled(center_, th * 0.9f + (outline ? outlineWidth_.f : 0), col);
            break;
        case 4: draw::heart(dl, center_, size * 1.4f + (outline ? outlineWidth_.f * 2.f : 0), col); break;
        case 5: cross(dl, size, th, col, true); break;
        case 6: polygon(dl, 4, (size + spread_) * 0.7f, th, col, 0.7854f); break;
        case 7: polygon(dl, 4, (size + spread_) * 0.8f, th, col, 0.f); break;
        case 8: polygon(dl, 3, (size + spread_) * 0.8f, th, col, -1.5708f); break;
        case 9: grid(dl, col, alt, outline); break;
        case 10:
            ensureImage();
            picture(dl, image_, imageScale_.f, imageTint_.b ? imageTintColor_.color : ImVec4(1.f, 1.f, 1.f, 1.f), col, outline);
            break;
        case game: {
            // the game's own image is white, so the crosshair color (rainbow, click and aim colors included) tints it
            auto shown = gameImage();
            if (shown) picture(dl, *shown, gameScale_.f, ImGui::ColorConvertU32ToFloat4(col), col, outline);
            break;
        }
        }
        if (centerDot_.b && style_.i != 1 && style_.i != 3 && style_.i != 9 && style_.i != 10 && style_.i != solid && style_.i != game)
            dl->AddCircleFilled(center_, th * 0.8f + (outline ? outlineWidth_.f : 0), col);
    }

    void ensureImage() {
        if (imagePath_.text == loadedFor_) return;
        loadedFor_ = imagePath_.text;
        image_ = {};
        if (imagePath_.text.empty()) return;
        std::string clean = imagePath_.text;
        if (clean.size() > 1 && clean.front() == '"' && clean.back() == '"') clean = clean.substr(1, clean.size() - 2);
        if (!img::load(std::filesystem::path(logger::widen(clean)), 64, image_)) status_ = i18n::tr("Could not read the image");
        else status_.clear();
    }

    // One rectangle per run of equal pixels in a row instead of one per pixel: a crosshair is mostly lines.
    void picture(ImDrawList* dl, const img::Pixels& image, float scale, ImVec4 tint, ImU32 outlineCol, bool outline) const {
        if (image.rgba.empty()) return;
        float px = scale * (1.f + spread_ * 0.05f);
        ImVec2 origin = center_ - ImVec2(float(image.w), float(image.h)) * px * 0.5f;
        float ow = outlineWidth_.f;
        for (int y = 0; y < image.h; y++)
            for (int x = 0; x < image.w;) {
                uint32_t c = image.rgba[size_t(y * image.w + x)];
                bool seen = ((c >> 24) & 255) >= 8;
                int end = x + 1;
                while (end < image.w) {
                    uint32_t next = image.rgba[size_t(y * image.w + end)];
                    bool same = outline ? (((next >> 24) & 255) >= 8) == seen : next == c;
                    if (!same) break;
                    ++end;
                }
                if (seen) {
                    ImVec2 p0 = origin + ImVec2(float(x) * px, float(y) * px), p1 = origin + ImVec2(float(end) * px, float(y + 1) * px);
                    if (outline) {
                        dl->AddRectFilled(p0 - ImVec2(ow, ow), p1 + ImVec2(ow, ow), outlineCol);
                    } else {
                        ImVec4 v{float(c & 255) / 255.f * tint.x, float((c >> 8) & 255) / 255.f * tint.y, float((c >> 16) & 255) / 255.f * tint.z,
                                 float((c >> 24) & 255) / 255.f * tint.w * (style_.i == game ? 1.f : opacity_.f)};
                        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(v));
                    }
                }
                x = end;
            }
    }

    // The crosshair the game would draw: from the top resource pack that has one, else the game's own. Read once on a
    // background job, and again on request.
    struct GameImage {
        std::mutex lock;
        std::shared_ptr<const img::Pixels> image;
        std::string source;
        int state = 0;
    };

    static GameImage& gameShared() {
        static GameImage shared;
        return shared;
    }

    static void loadGameImage() {
        auto& shared = gameShared();
        {
            std::scoped_lock g(shared.lock);
            if (shared.state == 1) return;
            shared.state = 1;
        }
        bg::run([] {
            auto pixels = std::make_shared<img::Pixels>();
            std::string source;
            bool ok = gametex::load("textures/ui", "cross_hair.png", 64, *pixels, source);
            // The game blends its crosshair so that black leaves the picture untouched, and packs paint theirs on a
            // black square for that. Brightness becomes coverage here and the hue stays, so black is see-through.
            for (auto& c : pixels->rgba) {
                uint32_t r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255, a = c >> 24;
                uint32_t top = std::max({r, g, b});
                if (!top) {
                    c = 0;
                    continue;
                }
                c = (r * 255 / top) | (g * 255 / top) << 8 | (b * 255 / top) << 16 | (a * top / 255) << 24;
            }
            auto& shared = gameShared();
            std::scoped_lock g(shared.lock);
            shared.image = ok ? pixels : nullptr;
            shared.source = source;
            shared.state = ok ? 2 : 3;
        });
    }

    std::shared_ptr<const img::Pixels> gameImage() {
        auto& shared = gameShared();
        {
            std::scoped_lock g(shared.lock);
            if (shared.state != 0) return shared.image;
        }
        loadGameImage();
        return nullptr;
    }

    void gameInfo() {
        auto& shared = gameShared();
        int state;
        std::string source;
        {
            std::scoped_lock g(shared.lock);
            state = shared.state;
            source = shared.source;
        }
        ImGui::TextDisabled("%s", i18n::tr("The crosshair of your texture pack, or the game's own without one. Size and color are yours."));
        if (state == 2) ImGui::TextDisabled("%s", source.empty() ? i18n::tr("From: the game's own textures") : i18n::fmt("From: {}", source).c_str());
        if (state == 3) ImGui::TextColored(theme::current().warn, "%s", i18n::tr("No crosshair image found"));
        if (ImGui::SmallButton(i18n::tr("Read again"))) {
            {
                std::scoped_lock g(shared.lock);
                if (shared.state != 1) shared.state = 0;
            }
            loadGameImage();
        }
        ImGui::Spacing();
    }

    // The slider stays; these are the same setting in fixed steps, one click each.
    void steps(const char* label, Setting& s, std::initializer_list<float> values, const char* format) {
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n::tr(label));
        for (float v : values) {
            ImGui::SameLine();
            char text[24];
            std::snprintf(text, sizeof(text), format, v);
            bool on = std::fabs(s.f - v) < 0.01f;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, theme::current().accent);
            if (ImGui::SmallButton(text)) {
                s.f = v;
                config::markDirty();
            }
            if (on) ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }

    void sizeSteps() {
        switch (style_.i) {
        case 1: steps("Dot size", thickness_, {1.f, 1.5f, 2.f, 3.f, 4.f, 6.f}, "%g"); break;
        case 9: break;
        case 10: steps("Image pixel size", imageScale_, {0.5f, 1.f, 2.f, 3.f, 4.f}, "%g"); break;
        case solid:
            steps("Arm length (GUI pixels)", solidArm_, {5.f, 7.f, 9.f, 12.f}, "%g");
            steps("Thickness (GUI pixels)", solidThick_, {1.f, 2.f, 3.f}, "%g");
            break;
        case game: steps("Pixel size", gameScale_, {1.f, 2.f, 3.f, 4.f, 5.f, 6.f}, "%g"); break;
        default:
            steps("Size", size_, {4.f, 6.f, 8.f, 10.f, 12.f, 16.f, 20.f}, "%g");
            steps("Thickness", thickness_, {1.f, 1.5f, 2.f, 3.f, 4.f}, "%g");
            break;
        }
        ImGui::Spacing();
    }

    // whatever changes the grid makes it the shape in use, so drawing is never without effect
    void edited() {
        style_.i = 9;
        config::markDirty();
    }

    void put(int x, int y, char value) {
        if (x < 0 || y < 0 || x >= cells || y >= cells) return;
        if (grid_.text.size() != size_t(cells * cells)) grid_.text.assign(size_t(cells * cells), '0');
        char& cell = grid_.text[size_t(y * cells + x)];
        if (cell == value) return;
        cell = value;
        edited();
    }

    static std::string preset(int kind) {
        std::string g(cells * cells, '0');
        auto set = [&](int x, int y, char v = '1') {
            if (x >= 0 && y >= 0 && x < cells && y < cells) g[y * cells + x] = v;
        };
        int m = cells / 2;
        switch (kind) {
        case 0:
            for (int i = 2; i <= 5; i++) {
                set(m - i, m);
                set(m + i, m);
                set(m, m - i);
                set(m, m + i);
            }
            break;
        case 1:
            set(m, m);
            set(m - 1, m);
            set(m + 1, m);
            set(m, m - 1);
            set(m, m + 1);
            break;
        case 2:
            for (int i = 0; i < 360; i += 15) set(m + int(std::lround(5.f * std::cos(i * 0.0174533f))), m + int(std::lround(5.f * std::sin(i * 0.0174533f))));
            set(m, m, '2');
            break;
        case 3: {
            static const char* heart[7] = {".##.##.", "#######", "#######", ".#####.", "..###..", "...#...", "......."};
            for (int y = 0; y < 7; y++)
                for (int x = 0; x < 7; x++)
                    if (heart[y][x] == '#') set(m - 3 + x, m - 3 + y, y < 2 ? '2' : '1');
            break;
        }
        case 4:
            for (int i = 1; i <= 6; i++) {
                set(m - i, m - i);
                set(m + i, m - i);
                set(m - i, m + i);
                set(m + i, m + i);
            }
            set(m, m, '2');
            break;
        }
        return g;
    }

    std::string exportCode() const {
        static const char* hex = "0123456789ABCDEF";
        std::string out;
        for (size_t i = 0; i < grid_.text.size(); i += 2) {
            int a = grid_.text[i] - '0', b = i + 1 < grid_.text.size() ? grid_.text[i + 1] - '0' : 0;
            out += hex[std::clamp(a, 0, 3) * 4 + std::clamp(b, 0, 3)];
        }
        return "MCH1-" + out;
    }

    bool importCode(const std::string& code) {
        if (!code.starts_with("MCH1-")) return false;
        std::string g;
        for (size_t i = 5; i < code.size(); i++) {
            char c = code[i];
            int v = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (v < 0) return false;
            g += char('0' + v / 4);
            g += char('0' + v % 4);
        }
        g.resize(cells * cells, '0');
        grid_.text = g;
        return true;
    }

    void shift(int dx, int dy) {
        std::string next(cells * cells, '0');
        for (int y = 0; y < cells; y++)
            for (int x = 0; x < cells; x++) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && ny >= 0 && nx < cells && ny < cells) next[ny * cells + nx] = grid_.text[y * cells + x];
            }
        grid_.text = next;
    }

    void editor() {
        auto& t = theme::current();
        float s = ui::scale();
        float cs = 15.f * s;
        steps("Pixel size", cell_, {1.f, 1.5f, 2.f, 3.f, 4.f}, "%g");
        int shown = int(std::lround(cells * cell_.f));
        ImGui::TextDisabled("%s", i18n::fmt("On screen: {} x {} pixels", shown, shown).c_str());
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##grid", {cells * cs, cells * cs});
        bool hovered = ImGui::IsItemHovered();
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(origin, origin + ImVec2(cells * cs, cells * cs), theme::col(t.surface), 4 * s);

        ImVec2 mouse = ImGui::GetIO().MousePos;
        for (int y = 0; y < cells; y++)
            for (int x = 0; x < cells; x++) {
                ImVec2 a = origin + ImVec2(x * cs, y * cs);
                ImVec2 b = a + ImVec2(cs, cs);
                char v = grid_.text[y * cells + x];
                if (v != '0')
                    dl->AddRectFilled(a + ImVec2(1, 1), b - ImVec2(1, 1),
                                      v == '2' ? ImGui::GetColorU32(activeColor_.color) : v == '3' ? IM_COL32(0, 0, 0, 255) : ImGui::GetColorU32(color_.color));
                if (gridLines_.b) dl->AddRect(a, b, theme::col(t.textDim, 0.18f));
                if (hovered && mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y) {
                    dl->AddRect(a, b, theme::col(t.accent), 0, 0, 2.f);
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) put(x, y, char('0' + brush_));
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) put(x, y, '0');
                }
            }

        if (middle_.b) {
            float mid = cells * cs * 0.5f;
            dl->AddLine(origin + ImVec2(mid, 0), origin + ImVec2(mid, cells * cs), IM_COL32(255, 60, 60, 200), 1.5f * s);
            dl->AddLine(origin + ImVec2(0, mid), origin + ImVec2(cells * cs, mid), IM_COL32(255, 60, 60, 200), 1.5f * s);
        }

        ImGui::TextDisabled(i18n::tr("Left click paints, right click erases"));
        const char* names[] = {"Main color", "Second color", "Black"};
        for (int i = 0; i < 3; i++) {
            if (i) ImGui::SameLine();
            if (ImGui::RadioButton(i18n::tr(names[i]), brush_ == i + 1)) brush_ = i + 1;
        }
        if (ImGui::SmallButton(i18n::tr("Clear"))) {
            grid_.text.assign(cells * cells, '0');
            edited();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Mirror"))) {
            for (int y = 0; y < cells; y++) std::reverse(grid_.text.begin() + y * cells, grid_.text.begin() + (y + 1) * cells);
            edited();
        }
        const char* moves[] = {"High", "Down", "Left", "Right"};
        const int by[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        for (int i = 0; i < 4; i++) {
            ImGui::SameLine();
            if (!ImGui::SmallButton(i18n::tr(moves[i]))) continue;
            shift(by[i][0], by[i][1]);
            edited();
        }

        ImGui::TextDisabled(i18n::tr("Presets"));
        const char* presets[] = {"Cross", "Plus", "Ring", "Heart", "X"};
        for (int i = 0; i < 5; i++) {
            if (i) ImGui::SameLine();
            if (!ImGui::SmallButton(i18n::tr(presets[i]))) continue;
            grid_.text = preset(i);
            edited();
        }
        if (ImGui::SmallButton(i18n::tr("Copy code"))) ImGui::SetClipboardText(exportCode().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Paste code"))) {
            const char* clip = ImGui::GetClipboardText();
            if (clip && importCode(clip)) edited();
        }
        library();
    }

    static std::filesystem::path folder() { return paths::root() / L"crosshairs"; }

    bool saveGrid(const std::string& name) const {
        std::vector<uint32_t> px(cells * cells, 0);
        auto pack = [](ImVec4 c) {
            return uint32_t(c.x * 255.f + 0.5f) | uint32_t(c.y * 255.f + 0.5f) << 8 | uint32_t(c.z * 255.f + 0.5f) << 16 | uint32_t(c.w * 255.f + 0.5f) << 24;
        };
        for (int i = 0; i < cells * cells; i++) {
            char v = grid_.text[size_t(i)];
            if (v == '1') px[size_t(i)] = pack(color_.color);
            else if (v == '2') px[size_t(i)] = pack(activeColor_.color);
            else if (v == '3') px[size_t(i)] = 0xff000000u;
        }
        std::error_code ec;
        std::filesystem::create_directories(folder(), ec);
        return png::write(folder() / std::filesystem::path(logger::widen(name + ".png")), cells, cells, px);
    }

    // saved crosshairs are plain PNG files, so ones drawn elsewhere can be dropped into the folder too
    void library() {
        ImGui::Spacing();
        ImGui::TextDisabled(i18n::tr("Saved crosshairs"));
        ImGui::SetNextItemWidth(160.f * ui::scale());
        ImGui::InputText("##saveName", saveName_, sizeof(saveName_));
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Save")) && saveName_[0]) status_ = saveGrid(saveName_) ? "" : i18n::tr("Could not save the crosshair");
        std::error_code ec;
        int shown = 0;
        for (auto& f : std::filesystem::directory_iterator(folder(), ec)) {
            if (f.path().extension() != L".png") continue;
            auto utf8 = [](const std::filesystem::path& p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); };
            std::string name = utf8(f.path().stem());
            ImGui::PushID(shown++);
            if (ImGui::SmallButton(i18n::tr("Use"))) {
                style_.i = 10;
                imagePath_.text = utf8(f.path());
                imageScale_.f = std::max(imageScale_.f, cell_.f);
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopID();
        }
        if (!shown) ImGui::TextDisabled("%s", i18n::tr("None yet. Save the grid above or put PNG files into the crosshairs folder."));
    }

    Setting& style_ = choice("style", "Shape", {"Cross", "Dot", "Circle", "Cross + dot", "Heart", "T shape", "Square", "Diamond", "Triangle", "Custom grid", "PNG image", "Solid cross", "Game crosshair"}, 0);
    Setting& solidArm_ = slider("solidArm", "Arm length (GUI pixels)", 9.f, 3.f, 16.f, "%.0f");
    Setting& solidThick_ = slider("solidThick", "Thickness (GUI pixels)", 1.f, 0.5f, 4.f, "%.1f");
    Setting& guiScale_ = slider("guiScale", "GUI scale of the game (0 = automatic)", 0.f, 0.f, 6.f, "%.0f");
    Setting& size_ = slider("size", "Size", 8.f, 2.f, 40.f, "%.0f");
    Setting& cell_ = slider("cell", "Pixel size", 2.f, 1.f, 6.f, "%.1f");
    Setting& gameScale_ = slider("gameScale", "Pixel size of the game crosshair", 3.f, 1.f, 8.f, "%.1f");
    Setting& imagePath_ = textSetting("imagePath", "PNG file path", "");
    Setting& imageScale_ = slider("imageScale", "Image pixel size", 1.f, 0.25f, 6.f, "%.2f");
    Setting& imageTint_ = toggleSetting("imageTint", "Tint the image", false);
    Setting& imageTintColor_ = colorSetting("imageTintColor", "Image tint", {1.f, 1.f, 1.f, 1.f});
    Setting& gap_ = slider("gap", "Gap", 2.f, 0.f, 16.f, "%.0f");
    Setting& thickness_ = slider("thickness", "Thickness", 1.f, 1.f, 8.f, "%.1f");
    Setting& opacity_ = slider("opacity", "Opacity", 1.f, 0.1f, 1.f, "%.2f");
    Setting& color_ = colorSetting("color", "Color", {1.f, 1.f, 1.f, 0.95f});
    Setting& rainbow_ = toggleSetting("rainbow", "Rainbow", false);
    Setting& rainbowSpeed_ = slider("rainbowSpeed", "Rainbow speed", 1.f, 0.1f, 5.f, "%.1f");
    Setting& centerDot_ = toggleSetting("centerDot", "Center dot", false);
    Setting& outline_ = toggleSetting("outline", "Outline", true);
    Setting& outlineWidth_ = slider("outlineWidth", "Outline thickness", 1.f, 0.5f, 4.f, "%.1f");
    Setting& outlineColor_ = colorSetting("outlineColor", "Outline color", {0.f, 0.f, 0.f, 0.8f});
    Setting& clickColor_ = toggleSetting("clickColor", "Color while clicking", true);
    Setting& activeColor_ = colorSetting("activeColor", "Click color / second color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& clickPulse_ = toggleSetting("pulse", "Pulse while clicking", true);
    Setting& dynamic_ = toggleSetting("dynamic", "Dynamic (walking, jumping, sneaking)", false);
    Setting& moveSpread_ = slider("moveSpread", "Spread while walking", 3.f, 0.f, 16.f, "%.1f");
    Setting& jumpSpread_ = slider("jumpSpread", "Spread while jumping", 4.f, 0.f, 16.f, "%.1f");
    Setting& sneakShrink_ = slider("sneakShrink", "Shrink while sneaking", 2.f, 0.f, 8.f, "%.1f");
    Setting& clickSpread_ = slider("clickSpread", "Spread while clicking", 2.f, 0.f, 16.f, "%.1f");
    Setting& hideVanilla_ = toggleSetting("hideVanilla", "Hide the original crosshair", true);
    Setting& hideThird_ = toggleSetting("hideThird", "Hide in third person", true);
    Setting& hideScreens_ = toggleSetting("hideScreens", "Hide in inventory, chat and pause", true);
    Setting& targetOn_ = toggleSetting("targetOn", "Color when aiming at an opponent", false);
    Setting& playersOnly_ = toggleSetting("playersOnly", "Only for players", true);
    Setting& targetColor_ = colorSetting("targetColor", "Color when aiming", {1.f, 0.35f, 0.4f, 1.f});
    Setting& rotation_ = slider("rotation", "Rotation", 0.f, 0.f, 360.f, "%.0f°");
    Setting& spin_ = toggleSetting("spin", "Constant spin", false);
    Setting& spinSpeed_ = slider("spinSpeed", "Spin speed (°/s)", 90.f, 10.f, 720.f, "%.0f");
    Setting& offsetX_ = slider("offsetX", "Offset X", 0.f, -100.f, 100.f, "%.0f");
    Setting& offsetY_ = slider("offsetY", "Offset Y", 0.f, -100.f, 100.f, "%.0f");
    Setting& grid_ = textSetting("grid", "Grid", preset(0));
    Setting& gridLines_ = toggleSetting("gridLines", "Grid lines in the editor", true);
    Setting& middle_ = toggleSetting("middle", "Mark the middle in the editor", false);

    img::Pixels image_;
    std::string loadedFor_;
    std::string status_;
    ImVec2 center_{0, 0};
    float pulse_ = 0.f;
    float targetMix_ = 0.f;
    float spread_ = 0.f;
    float angle_ = 0.f;
    float rad_ = 0.f;
    int brush_ = 1;
    char saveName_[48] = "crosshair";
};
