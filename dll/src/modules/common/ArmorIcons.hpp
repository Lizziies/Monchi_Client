#pragma once

#include "imgui.h"

#include <string>

// The four armor pieces as 8x8 pixel shapes, tinted by material: the game's own item icons are packed inside its
// archives and not reachable from the overlay.
namespace armoricon {

inline constexpr const char* shapes[4][8] = {
    {"..####..", ".######.", ".######.", ".##..##.", ".##..##.", "........", "........", "........"},
    {"##....##", "###..###", "########", ".######.", ".######.", ".######.", ".######.", "..####.."},
    {".######.", ".######.", ".######.", ".##..##.", ".##..##.", ".##..##.", ".##..##.", ".##..##."},
    {"........", "........", ".##..##.", ".##..##.", ".##..##.", ".##..##.", "###..###", "###..###"},
};

inline ImU32 shade(ImU32 c, float k, float alpha) {
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    v.x = v.x * k > 1.f ? 1.f : v.x * k;
    v.y = v.y * k > 1.f ? 1.f : v.y * k;
    v.z = v.z * k > 1.f ? 1.f : v.z * k;
    v.w *= alpha;
    return ImGui::GetColorU32(v);
}

// slot: 0 helmet, 1 chestplate, 2 leggings, 3 boots
inline void draw(ImDrawList* dl, ImVec2 at, float size, int slot, ImU32 color, float alpha = 1.f) {
    const auto& rows = shapes[slot & 3];
    float p = size / 8.f;
    auto set = [&](int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8 && rows[y][x] == '#'; };
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            if (!set(x, y)) continue;
            float k = !set(x, y - 1) || !set(x - 1, y) ? 1.25f : !set(x, y + 1) || !set(x + 1, y) ? 0.72f : 1.f;
            ImVec2 a = at + ImVec2(float(x) * p, float(y) * p);
            dl->AddRectFilled(a, a + ImVec2(p, p), shade(color, k, alpha));
        }
}

// swords, pickaxe-like tools and a shield shape for everything else, drawn the same way
inline constexpr const char* tools[3][8] = {
    {"......##", ".....###", "....###.", "...###..", "#.###...", ".###....", ".##.....", "#.#....."},
    {".####...", "#....#..", ".....##.", "..#...#.", "..#....#", "..#.....", "..#.....", "..#....."},
    {".######.", "########", "########", "########", ".######.", "..####..", "..####..", "...##..."},
};

inline int toolKind(const std::string& name) {
    if (name.find("sword") != std::string::npos) return 0;
    for (const char* t : {"pickaxe", "axe", "shovel", "hoe"})
        if (name.find(t) != std::string::npos) return 1;
    return 2;
}

inline void drawTool(ImDrawList* dl, ImVec2 at, float size, int kind, ImU32 color, float alpha = 1.f) {
    const auto& rows = tools[kind % 3];
    float p = size / 8.f;
    auto set = [&](int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8 && rows[y][x] == '#'; };
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            if (!set(x, y)) continue;
            float k = !set(x, y - 1) || !set(x - 1, y) ? 1.25f : !set(x, y + 1) || !set(x + 1, y) ? 0.72f : 1.f;
            ImVec2 a = at + ImVec2(float(x) * p, float(y) * p);
            dl->AddRectFilled(a, a + ImVec2(p, p), shade(color, k, alpha));
        }
}

}
