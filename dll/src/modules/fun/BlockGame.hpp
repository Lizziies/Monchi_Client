#pragma once

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <random>

class BlockGame : public Module {
public:
    BlockGame()
        : Module("Block Game", "Falling blocks while you queue. Space drops, ESC quits.",
                 Category::Fun, {"cosmetic"}) {
        sub("Games");
    }

    bool persistent() const override { return false; }
    void onEnable() override { reset(); }

    void onFrame() override {
        if (gui::open()) return;
        gui::claimKeyboard();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            setEnabled(false);
            return;
        }
        if (dead_) {
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) reset();
            draw();
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) paused_ = !paused_;
        if (!paused_) {
            input();
            fall_ += ui::dt();
            float interval = std::max(0.05f, std::pow(0.8f - (level() - 1) * 0.007f, float(level() - 1)));
            if (soft_) interval = std::min(interval, 0.04f);
            while (fall_ >= interval && !dead_) {
                fall_ -= interval;
                if (!move(0, 1)) lock();
            }
        }
        draw();
    }

private:
    static constexpr int W = 10, H = 20;
    using Cells = std::array<std::array<int, 2>, 4>;

    static constexpr std::array<std::array<Cells, 4>, 7> shapes = {{
        {{{{{0, 1}, {1, 1}, {2, 1}, {3, 1}}}, {{{2, 0}, {2, 1}, {2, 2}, {2, 3}}}, {{{0, 2}, {1, 2}, {2, 2}, {3, 2}}}, {{{1, 0}, {1, 1}, {1, 2}, {1, 3}}}}},
        {{{{{1, 0}, {2, 0}, {1, 1}, {2, 1}}}, {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}}, {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}}, {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}}}},
        {{{{{1, 0}, {0, 1}, {1, 1}, {2, 1}}}, {{{1, 0}, {1, 1}, {2, 1}, {1, 2}}}, {{{0, 1}, {1, 1}, {2, 1}, {1, 2}}}, {{{1, 0}, {0, 1}, {1, 1}, {1, 2}}}}},
        {{{{{1, 0}, {2, 0}, {0, 1}, {1, 1}}}, {{{1, 0}, {1, 1}, {2, 1}, {2, 2}}}, {{{1, 1}, {2, 1}, {0, 2}, {1, 2}}}, {{{0, 0}, {0, 1}, {1, 1}, {1, 2}}}}},
        {{{{{0, 0}, {1, 0}, {1, 1}, {2, 1}}}, {{{2, 0}, {1, 1}, {2, 1}, {1, 2}}}, {{{0, 1}, {1, 1}, {1, 2}, {2, 2}}}, {{{1, 0}, {0, 1}, {1, 1}, {0, 2}}}}},
        {{{{{0, 0}, {0, 1}, {1, 1}, {2, 1}}}, {{{1, 0}, {2, 0}, {1, 1}, {1, 2}}}, {{{0, 1}, {1, 1}, {2, 1}, {2, 2}}}, {{{1, 0}, {1, 1}, {0, 2}, {1, 2}}}}},
        {{{{{2, 0}, {0, 1}, {1, 1}, {2, 1}}}, {{{1, 0}, {1, 1}, {1, 2}, {2, 2}}}, {{{0, 1}, {1, 1}, {2, 1}, {0, 2}}}, {{{0, 0}, {1, 0}, {1, 1}, {1, 2}}}}},
    }};

    int level() const { return std::max(startLevel_.i, 1 + lines_ / 10); }

    void reset() {
        for (auto& row : board_) row.fill(0);
        score_ = 0;
        lines_ = 0;
        dead_ = false;
        paused_ = false;
        held_ = -1;
        canHold_ = true;
        fall_ = 0;
        bag_.clear();
        next_ = draw7();
        spawn();
    }

    int draw7() {
        if (bag_.empty()) {
            for (int i = 0; i < 7; i++) bag_.push_back(i);
            std::shuffle(bag_.begin(), bag_.end(), rng_);
        }
        int v = bag_.back();
        bag_.pop_back();
        return v;
    }

    void spawn() {
        kind_ = next_;
        next_ = draw7();
        rot_ = 0;
        x_ = 3;
        y_ = 0;
        canHold_ = true;
        if (collides(x_, y_, rot_)) {
            dead_ = true;
            best_ = std::max(best_, score_);
        }
    }

    bool collides(int px, int py, int r) const {
        for (auto& c : shapes[kind_][r]) {
            int x = px + c[0], y = py + c[1];
            if (x < 0 || x >= W || y >= H) return true;
            if (y >= 0 && board_[y][x]) return true;
        }
        return false;
    }

    bool move(int dx, int dy) {
        if (collides(x_ + dx, y_ + dy, rot_)) return false;
        x_ += dx;
        y_ += dy;
        return true;
    }

    void rotate(int dir) {
        int r = (rot_ + dir + 4) % 4;
        for (int kick : {0, -1, 1, -2, 2})
            if (!collides(x_ + kick, y_, r)) {
                x_ += kick;
                rot_ = r;
                return;
            }
    }

    void lock() {
        for (auto& c : shapes[kind_][rot_]) {
            int x = x_ + c[0], y = y_ + c[1];
            if (y >= 0) board_[y][x] = kind_ + 1;
        }
        int cleared = 0;
        for (int y = H - 1; y >= 0; y--) {
            if (std::all_of(board_[y].begin(), board_[y].end(), [](int v) { return v != 0; })) {
                for (int yy = y; yy > 0; yy--) board_[yy] = board_[yy - 1];
                board_[0].fill(0);
                cleared++;
                y++;
            }
        }
        static const int points[] = {0, 100, 300, 500, 800};
        score_ += points[cleared] * level();
        lines_ += cleared;
        spawn();
    }

    void input() {
        auto pressed = [](ImGuiKey a, ImGuiKey b) { return ImGui::IsKeyPressed(a, true) || ImGui::IsKeyPressed(b, true); };
        if (pressed(ImGuiKey_LeftArrow, ImGuiKey_A)) move(-1, 0);
        if (pressed(ImGuiKey_RightArrow, ImGuiKey_D)) move(1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) || ImGui::IsKeyPressed(ImGuiKey_W, false) || ImGui::IsKeyPressed(ImGuiKey_X, false)) rotate(1);
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) rotate(-1);
        soft_ = ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S);
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
            int dist = 0;
            while (move(0, 1)) dist++;
            score_ += dist * 2;
            lock();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_C, false) && canHold_) {
            int old = held_;
            held_ = kind_;
            if (old < 0) {
                spawn();
            } else {
                kind_ = old;
                rot_ = 0;
                x_ = 3;
                y_ = 0;
            }
            canHold_ = false;
        }
    }

    ImVec4 tint(int v) const {
        auto& t = theme::current();
        static const float hues[] = {0.52f, 0.14f, 0.78f, 0.38f, 0.0f, 0.62f, 0.07f};
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hues[(v - 1) % 7] + (hue_.f - 0.5f) * 0.2f, 0.45f, 1.f, r, g, b);
        return theme::mix(ImVec4(r, g, b, 1.f), t.accent, 0.12f);
    }

    void block(ImDrawList* dl, ImVec2 p, float cs, ImVec4 c, float alpha = 1.f) const {
        dl->AddRectFilled(p + ImVec2(1, 1), p + ImVec2(cs - 1, cs - 1), theme::col(c, alpha), cs * 0.2f);
    }

    void mini(ImDrawList* dl, ImVec2 o, float cs, int kind) const {
        if (kind < 0) return;
        for (auto& c : shapes[kind][0]) block(dl, o + ImVec2(c[0] * cs, c[1] * cs), cs, tint(kind + 1));
    }

    void draw() {
        auto& t = theme::current();
        float s = ui::scale();
        auto* dl = ImGui::GetForegroundDrawList();
        auto ds = ImGui::GetIO().DisplaySize;
        float cs = cell_.f * s;
        ImVec2 size{W * cs, H * cs};
        ImVec2 o = (ds - size) * 0.5f;

        dl->AddRectFilled(o - ImVec2(130 * s, 54 * s), o + size + ImVec2(130 * s, 14 * s), theme::col(t.bg, 0.96f), t.rounding * s);
        dl->AddText(fonts::bold(), 20 * s, o - ImVec2(0, 40 * s), theme::col(t.text),
                    i18n::fmt("Block Game  ·  {}  ·  Level {}  ·  Lines {}  ·  Best {}", score_, level(), lines_, best_).c_str());
        dl->AddRectFilled(o, o + size, theme::col(t.surface), 6 * s);

        if (grid_.b)
            for (int i = 1; i < W; i++) dl->AddLine(o + ImVec2(i * cs, 0), o + ImVec2(i * cs, size.y), theme::col(t.textDim, 0.12f));
        if (grid_.b)
            for (int i = 1; i < H; i++) dl->AddLine(o + ImVec2(0, i * cs), o + ImVec2(size.x, i * cs), theme::col(t.textDim, 0.12f));

        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (board_[y][x]) block(dl, o + ImVec2(x * cs, y * cs), cs, tint(board_[y][x]));

        if (!dead_) {
            if (ghost_.b) {
                int gy = y_;
                while (!collides(x_, gy + 1, rot_)) gy++;
                for (auto& c : shapes[kind_][rot_]) block(dl, o + ImVec2((x_ + c[0]) * cs, (gy + c[1]) * cs), cs, tint(kind_ + 1), 0.25f);
            }
            for (auto& c : shapes[kind_][rot_]) block(dl, o + ImVec2((x_ + c[0]) * cs, (y_ + c[1]) * cs), cs, tint(kind_ + 1));
        }

        dl->AddText(fonts::bold(), 15 * s, o + ImVec2(size.x + 14 * s, 0), theme::col(t.textDim), i18n::tr("Next"));
        mini(dl, o + ImVec2(size.x + 14 * s, 24 * s), cs * 0.7f, next_);
        dl->AddText(fonts::bold(), 15 * s, o + ImVec2(-118 * s, 0), theme::col(t.textDim), i18n::tr("Hold"));
        mini(dl, o + ImVec2(-118 * s, 24 * s), cs * 0.7f, held_);

        if (dead_) draw::textCentered(dl, fonts::bold(), 22 * s, o + size * 0.5f, theme::col(t.text), i18n::tr("Space = again, ESC = quit"));
        else if (paused_) draw::textCentered(dl, fonts::bold(), 22 * s, o + size * 0.5f, theme::col(t.text), "Pause");
    }

    Setting& startLevel_ = intSlider("level", "Start level", 1, 1, 15);
    Setting& cell_ = slider("cell", "Block size", 26.f, 16.f, 40.f, "%.0f");
    Setting& ghost_ = toggleSetting("ghost", "Block shadow", true);
    Setting& grid_ = toggleSetting("grid", "Show grid", true);
    Setting& hue_ = slider("hue", "Shift hue", 0.5f, 0.f, 1.f, "%.2f");

    std::array<std::array<int, W>, H> board_{};
    std::vector<int> bag_;
    int kind_ = 0, next_ = 0, held_ = -1, rot_ = 0, x_ = 3, y_ = 0;
    int score_ = 0, best_ = 0, lines_ = 0;
    bool dead_ = false, paused_ = false, soft_ = false, canHold_ = true;
    float fall_ = 0.f;
    std::mt19937 rng_{std::random_device{}()};
};
