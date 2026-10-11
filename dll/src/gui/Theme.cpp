#include "Theme.hpp"
#include "core/Config.hpp"
#include "render/Ui.hpp"

#include <algorithm>
#include <cstring>
#include <string>

using nlohmann::json;

namespace theme {

static ImVec4 hex(unsigned rgb, float a = 1.f) {
    return {((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, a};
}

static std::vector<Theme> builtins = {
    {"Slate", hex(0x24252A), hex(0x393A40), hex(0x4A4C53), hex(0x1E7CB5), hex(0x3BA7EC), hex(0xF3F3F5),
     hex(0xA6A8AF), hex(0x3DDC84), hex(0xFFB547), hex(0x8D8F96), 8.f, 0.86f, 1.f, false, false, false, hex(0x5E6068)},
    {"Onyx", hex(0x141518), hex(0x232428), hex(0x303137), hex(0x1E7CB5), hex(0x3BA7EC), hex(0xEDEDF0),
     hex(0x8F9199), hex(0x3DDC84), hex(0xFFB547), hex(0x6E7078), 8.f, 0.92f, 1.f, false, false, false, hex(0x3A3C42)},
    {"Steel", hex(0x1D222B), hex(0x2E3542), hex(0x3B4352), hex(0x1E7CB5), hex(0x3BA7EC), hex(0xEEF1F6),
     hex(0x9AA3B2), hex(0x3DDC84), hex(0xFFB547), hex(0x7D8696), 8.f, 0.88f, 1.f, false, false, false, hex(0x4E5869)},
    {"Glass", hex(0x1C1D21), hex(0x34353B), hex(0x45474E), hex(0x1E7CB5), hex(0x3BA7EC), hex(0xF5F5F7),
     hex(0xB0B2B9), hex(0x3DDC84), hex(0xFFB547), hex(0x8D8F96), 12.f, 0.62f, 1.f, false, false, false, hex(0x6A6C74)},
};

static const std::vector<Accent> accentList = {
    {"Blue", hex(0x1E7CB5), hex(0x3BA7EC)},   {"Cyan", hex(0x13899A), hex(0x45CFE0)},
    {"Green", hex(0x23905A), hex(0x52D68C)},  {"Purple", hex(0x6E4FC4), hex(0xA58CF2)},
    {"Pink", hex(0xB83D80), hex(0xF27BBD)},   {"Red", hex(0xB8342D), hex(0xF2675E)},
    {"Orange", hex(0xC2702A), hex(0xF5A35C)}, {"Gray", hex(0x5F626B), hex(0xDADBE0)},
};

static Theme active = builtins[0];

Theme& current() { return active; }

ImVec4 border() { return active.border.w > 0.f ? active.border : active.surfaceHover; }
const std::vector<Theme>& presets() { return builtins; }
const std::vector<Accent>& accents() { return accentList; }

void use(const Theme& t) {
    active = t;
    applyStyle();
    config::markDirty();
}

static float fadeLevel = 1.f;

void setFade(float f) { fadeLevel = f; }
float fade() { return fadeLevel; }

ImU32 col(const ImVec4& c, float alpha) {
    return ImGui::ColorConvertFloat4ToU32({c.x, c.y, c.z, c.w * alpha * fadeLevel});
}

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

void applyStyle() {
    if (!ImGui::GetCurrentContext()) return;
    auto& s = ImGui::GetStyle();
    s = ImGuiStyle();
    auto& t = active;
    s.WindowRounding = t.rounding;
    s.ChildRounding = t.rounding * 0.75f;
    s.FrameRounding = t.rounding * 0.6f;
    s.PopupRounding = t.rounding * 0.6f;
    s.GrabRounding = 99.f;
    s.ScrollbarRounding = 99.f;
    s.TabRounding = t.rounding * 0.6f;
    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 0.f;
    s.PopupBorderSize = 0.f;
    s.FrameBorderSize = 0.f;
    s.WindowPadding = {16, 16};
    s.FramePadding = {10, 6};
    s.ItemSpacing = {10, 8};
    s.ScrollbarSize = 10.f;
    s.ScrollbarPadding = 3.f;
    s.GrabMinSize = 12.f;

    auto* c = s.Colors;
    ImVec4 bg = t.bg;
    bg.w = t.opacity;
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg] = t.surface;
    c[ImGuiCol_Text] = t.text;
    c[ImGuiCol_TextDisabled] = t.textDim;
    c[ImGuiCol_FrameBg] = t.surface;
    c[ImGuiCol_FrameBgHovered] = t.surfaceHover;
    c[ImGuiCol_FrameBgActive] = t.surfaceHover;
    c[ImGuiCol_Button] = t.surface;
    c[ImGuiCol_ButtonHovered] = t.surfaceHover;
    c[ImGuiCol_ButtonActive] = mix(t.surfaceHover, t.accent, 0.3f);
    c[ImGuiCol_Header] = t.surface;
    c[ImGuiCol_HeaderHovered] = t.surfaceHover;
    c[ImGuiCol_HeaderActive] = mix(t.surfaceHover, t.accent, 0.3f);
    c[ImGuiCol_SliderGrab] = t.accent;
    c[ImGuiCol_SliderGrabActive] = t.accent2;
    c[ImGuiCol_CheckMark] = t.accent;
    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab] = mix(t.surfaceHover, t.text, 0.25f);
    c[ImGuiCol_ScrollbarGrabHovered] = mix(t.surfaceHover, t.text, 0.45f);
    c[ImGuiCol_ScrollbarGrabActive] = t.accent2;
    c[ImGuiCol_Separator] = t.surfaceHover;
    c[ImGuiCol_TextSelectedBg] = mix(t.accent, t.bg, 0.5f);
    c[ImGuiCol_Border] = t.surfaceHover;
    c[ImGuiCol_NavCursor] = t.accent;

    s.ScaleAllSizes(ui::scale());
    s.FontScaleMain = ui::scale();
}

static json color(const ImVec4& v) { return json::array({v.x, v.y, v.z, v.w}); }

static void color(const json& j, const char* key, ImVec4& out) {
    if (j.contains(key) && j[key].is_array() && j[key].size() == 4)
        out = {j[key][0], j[key][1], j[key][2], j[key][3]};
}

json save() {
    auto& t = active;
    return {
        {"name", t.name},
        {"bg", color(t.bg)},
        {"surface", color(t.surface)},
        {"surfaceHover", color(t.surfaceHover)},
        {"accent", color(t.accent)},
        {"accent2", color(t.accent2)},
        {"text", color(t.text)},
        {"textDim", color(t.textDim)},
        {"ok", color(t.ok)},
        {"warn", color(t.warn)},
        {"off", color(t.off)},
        {"rounding", t.rounding},
        {"opacity", t.opacity},
        {"animSpeed", t.animSpeed},
        {"gradient", t.gradient},
        {"sparkles", t.sparkles},
        {"hearts", t.hearts},
        {"border", color(t.border)},
    };
}

void load(const json& j) {
    if (!j.is_object()) return;
    // themes from the old pastel menu do not fit the panels; whoever still has one gets the default look
    static const char* retired[] = {"Carbon", "Graphite", "Bubblegum", "Sakura", "Lavender", "Strawberry Milk", "Midnight Pink"};
    std::string name = j.value("name", "");
    if (std::find(std::begin(retired), std::end(retired), name) != std::end(retired)) {
        active = builtins[0];
        return;
    }
    Theme t = builtins[0];
    t.name = j.value("name", t.name);
    color(j, "bg", t.bg);
    color(j, "surface", t.surface);
    color(j, "surfaceHover", t.surfaceHover);
    color(j, "accent", t.accent);
    color(j, "accent2", t.accent2);
    color(j, "text", t.text);
    color(j, "textDim", t.textDim);
    color(j, "ok", t.ok);
    color(j, "warn", t.warn);
    color(j, "off", t.off);
    t.rounding = std::clamp(j.value("rounding", t.rounding), 0.f, 30.f);
    t.opacity = std::clamp(j.value("opacity", t.opacity), 0.3f, 1.f);
    t.animSpeed = std::clamp(j.value("animSpeed", t.animSpeed), 0.25f, 3.f);
    t.gradient = j.value("gradient", t.gradient);
    t.sparkles = j.value("sparkles", t.sparkles);
    t.hearts = j.value("hearts", t.hearts);
    color(j, "border", t.border);
    active = t;
}

static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string exportCode() {
    std::string in = save().dump();
    std::string out = "monchi:";
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(b64[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(b64[((val << 8) >> (bits + 8)) & 0x3F]);
    return out;
}

bool importCode(const std::string& code) {
    std::string_view s = code;
    if (s.rfind("monchi:", 0) != 0) return false;
    s.remove_prefix(6);

    std::string out;
    int val = 0, bits = -8;
    for (char c : s) {
        const char* p = std::strchr(b64, c);
        if (!p || !c) break;
        val = (val << 6) + int(p - b64);
        bits += 6;
        if (bits >= 0) {
            out.push_back(char((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    auto j = json::parse(out, nullptr, false);
    if (j.is_discarded()) return false;
    load(j);
    applyStyle();
    config::markDirty();
    return true;
}

}
