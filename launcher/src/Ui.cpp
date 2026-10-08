#include "Look.hpp"
#include "Ui.hpp"

#include "Build.hpp"
#include "I18n.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace ui {

namespace {

using i18n::tr;

ImVec4 hex(unsigned rgb, float a = 1.f) {
    return {((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, a};
}

// the Slate theme of the client menu
const ImVec4 bg = hex(0x24252A);
const ImVec4 side = hex(0x1D1E22);
const ImVec4 surface = hex(0x393A40);
const ImVec4 surfaceHover = hex(0x4A4C53);
const ImVec4 field = hex(0x1B1C20);
const ImVec4 border = hex(0x5E6068);
ImVec4 accent = hex(0x1E7CB5);
ImVec4 accent2 = hex(0x3BA7EC);

// the same choices as in the client menu
struct Accent {
    const char* name;
    unsigned a, b;
};
const Accent accents[] = {
    {"Blue", 0x1E7CB5, 0x3BA7EC},   {"Cyan", 0x13899A, 0x45CFE0},
    {"Green", 0x23905A, 0x52D68C},  {"Purple", 0x6E4FC4, 0xA58CF2},
    {"Pink", 0xB83D80, 0xF27BBD},   {"Red", 0xB8342D, 0xF2675E},
    {"Orange", 0xC2702A, 0xF5A35C}, {"Gray", 0x5F626B, 0xDADBE0},
};
const ImVec4 text = hex(0xF3F3F5);
const ImVec4 dim = hex(0xA6A8AF);
const ImVec4 ok = hex(0x3DDC84);
const ImVec4 warn = hex(0xFFB547);
const ImVec4 off = hex(0x8D8F96);

ImFont* regular = nullptr;
ImFont* bold = nullptr;
float fade = 1.f;

ImU32 col(ImVec4 c, float a = 1.f) {
    c.w *= a * fade;
    return ImGui::ColorConvertFloat4ToU32(c);
}

ImVec4 mix(ImVec4 a, ImVec4 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

float approach(float cur, float target, float speed) {
    float v = cur + (target - cur) * (1.f - std::exp(-speed * ImGui::GetIO().DeltaTime));
    return std::fabs(v - target) < 0.001f ? target : v;
}

float& anim(ImGuiID id, float target, float speed = 14.f) {
    float& v = *ImGui::GetStateStorage()->GetFloatRef(id, target);
    v = approach(v, target, speed);
    return v;
}

float easeOut(float t) {
    t = 1.f - std::clamp(t, 0.f, 1.f);
    return 1.f - t * t * t;
}

ImVec2 measure(ImFont* f, float size, const char* s) { return f->CalcTextSizeA(size, FLT_MAX, 0.f, s); }

void label(ImDrawList* dl, ImFont* f, float size, ImVec2 at, ImU32 c, const char* s) {
    dl->AddText(f, size, at, c, s);
}

void centered(ImDrawList* dl, ImFont* f, float size, ImVec2 mid, ImU32 c, const char* s) {
    ImVec2 ts = measure(f, size, s);
    dl->AddText(f, size, {mid.x - ts.x * 0.5f, mid.y - ts.y * 0.5f}, c, s);
}

void gradient(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, ImVec4 left, ImVec4 right) {
    int start = dl->VtxBuffer.Size;
    dl->AddRectFilled(min, max, IM_COL32_WHITE, rounding);
    float w = std::max(1.f, max.x - min.x);
    for (int i = start; i < dl->VtxBuffer.Size; i++) {
        auto& v = dl->VtxBuffer[i];
        ImVec4 c = mix(left, right, std::clamp((v.pos.x - min.x) / w, 0.f, 1.f));
        v.col = col(c, ImGui::ColorConvertU32ToFloat4(v.col).w);
    }
}

void glow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, ImVec4 c, float spread, float strength) {
    for (int i = 5; i >= 1; i--) {
        float g = spread * i / 5.f;
        dl->AddRectFilled({min.x - g, min.y - g}, {max.x + g, max.y + g}, col(c, strength * (1.f - i / 5.f) * 0.5f), rounding + g);
    }
}

void pixelHeart(ImDrawList* dl, ImVec2 pos, float px) {
    static const char* rows[] = {
        "..ee...ee..", ".eHHe.eeee.", "eHHeeeeeeee", "eHeeeeeeeee", "eeeeeeeeeed",
        ".eeeeeeeed.", "..eeeeeed..", "...eeeed...", "....eed....", ".....d.....",
    };
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 11; x++) {
            char c = rows[y][x];
            if (c == '.') continue;
            ImVec4 k = c == 'H' ? mix(accent2, text, 0.75f) : c == 'd' ? accent : accent2;
            ImVec2 p{pos.x + x * px, pos.y + y * px};
            dl->AddRectFilled(p, {p.x + px, p.y + px}, col(k));
        }
    }
}

void chip(ImDrawList* dl, ImVec2 at, const char* s, ImVec4 c) {
    ImVec2 ts = measure(bold, 13.f, s);
    ImVec2 max{at.x + ts.x + 22.f, at.y + 24.f};
    dl->AddRectFilled(at, max, col(c, 0.14f), 6.f);
    label(dl, bold, 13.f, {at.x + 11.f, at.y + 12.f - ts.y * 0.5f}, col(c), s);
}

float chipWidth(const char* s) { return measure(bold, 13.f, s).x + 22.f; }

bool region(const char* id, ImVec2 min, ImVec2 max, bool& hovered, bool& held) {
    ImGui::SetCursorScreenPos(min);
    bool clicked = ImGui::InvisibleButton(id, {max.x - min.x, max.y - min.y});
    hovered = ImGui::IsItemHovered();
    held = ImGui::IsItemActive();
    return clicked;
}

bool button(const char* id, ImVec2 min, ImVec2 max, const char* s, bool primary, bool enabled = true) {
    bool hovered, held;
    bool clicked = region(id, min, max, hovered, held) && enabled;
    ImGuiID key = ImGui::GetItemID();
    float& h = anim(key, hovered && enabled ? 1.f : 0.f, 20.f);
    float& p = anim(key ^ 0x9E37, held && enabled ? 1.f : 0.f, 30.f);
    ImVec2 a{min.x, min.y + 0.8f * p}, b{max.x, max.y + 0.8f * p};
    float r = primary ? 8.f : 6.f;
    auto* dl = ImGui::GetWindowDrawList();
    ImVec2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};

    if (primary && enabled) {
        if (h > 0.01f) glow(dl, a, b, r, accent2, 10.f, h * 0.9f);
        dl->AddRectFilled(a, b, col(mix(mix(accent, accent2, 0.3f * h), bg, 0.2f * p)), r);
        centered(dl, bold, 19.f, mid, col(hex(0xFFFFFF)), s);
    } else if (primary) {
        dl->AddRectFilled(a, b, col(surface, 0.7f), r);
        centered(dl, bold, 19.f, mid, col(dim), s);
    } else {
        dl->AddRectFilled(a, b, col(mix(mix(surface, surfaceHover, h), bg, 0.25f * p), 0.7f + 0.3f * h), r);
        if (h > 0.01f) dl->AddRect(a, b, col(border, 0.5f * h), r);
        centered(dl, bold, 14.f, mid, col(enabled ? text : off), s);
    }
    return clicked;
}

bool toggle(const char* id, ImVec2 pos, bool& value) {
    ImVec2 size{40.f, 22.f};
    bool hovered, held;
    bool clicked = region(id, pos, {pos.x + size.x, pos.y + size.y}, hovered, held);
    if (clicked) value = !value;
    float& t = anim(ImGui::GetItemID(), value ? 1.f : 0.f, 18.f);
    auto* dl = ImGui::GetWindowDrawList();
    ImVec4 on = mix(accent, accent2, 0.35f);
    if (t > 0.02f) dl->AddRectFilled({pos.x - 3.f, pos.y - 3.f}, {pos.x + size.x + 3.f, pos.y + size.y + 3.f}, col(on, 0.2f * t), 14.f);
    dl->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, col(mix(off, on, t), 0.55f + 0.45f * t), 11.f);
    float kr = 6.6f + (hovered ? 0.5f : 0.f);
    float kw = kr * (1.f + 0.5f * std::sin(t * 3.14159f));
    ImVec2 c{pos.x + 11.f + (size.x - 22.f) * t, pos.y + 11.f};
    dl->AddRectFilled({c.x - kw, c.y - kr + 1.f}, {c.x + kw, c.y + kr + 1.f}, col(hex(0x000000), 0.25f), kr);
    dl->AddRectFilled({c.x - kw, c.y - kr}, {c.x + kw, c.y + kr}, col(mix(hex(0xFFFFFF), accent2, 0.35f * t)), kr);
    return clicked;
}

enum class Icon { Play, Figure, Layers, Sliders, Info };

void icon(ImDrawList* dl, Icon kind, ImVec2 c, ImU32 color) {
    switch (kind) {
    case Icon::Play:
        dl->AddTriangleFilled({c.x - 5.f, c.y - 8.f}, {c.x - 5.f, c.y + 8.f}, {c.x + 9.f, c.y}, color);
        break;
    case Icon::Figure:
        dl->AddRectFilled({c.x - 4.f, c.y - 10.f}, {c.x + 4.f, c.y - 3.f}, color, 1.5f);
        dl->AddRectFilled({c.x - 8.f, c.y - 1.f}, {c.x + 8.f, c.y + 4.f}, color, 1.5f);
        dl->AddRectFilled({c.x - 4.f, c.y + 4.f}, {c.x - 0.5f, c.y + 10.f}, color, 1.f);
        dl->AddRectFilled({c.x + 0.5f, c.y + 4.f}, {c.x + 4.f, c.y + 10.f}, color, 1.f);
        break;
    case Icon::Layers:
        for (int i = 0; i < 3; i++)
            dl->AddRectFilled({c.x - 9.f, c.y - 9.f + i * 6.5f}, {c.x + 9.f, c.y - 5.f + i * 6.5f}, color, 2.f);
        break;
    case Icon::Sliders:
        for (int i = 0; i < 3; i++) {
            float y = c.y - 7.f + i * 7.f;
            dl->AddLine({c.x - 9.f, y}, {c.x + 9.f, y}, color, 2.f);
            dl->AddCircleFilled({c.x - 4.f + i * 4.f, y}, 3.2f, color, 12);
        }
        break;
    case Icon::Info:
        dl->AddCircle(c, 9.f, color, 24, 2.f);
        dl->AddRectFilled({c.x - 1.f, c.y - 1.f}, {c.x + 1.f, c.y + 5.f}, color);
        dl->AddCircleFilled({c.x, c.y - 4.f}, 1.4f, color, 8);
        break;
    }
}

ImVec2 cardMin(float x, float y) { return {x, y}; }

void card(ImDrawList* dl, ImVec2 min, ImVec2 max, ImVec4 fill = surface) {
    dl->AddRectFilled(min, max, col(fill, 0.92f), 8.f);
    dl->AddRect(min, max, col(border, 0.85f), 8.f);
}

Page lastPage = Page::Start;
float pageAge = 10.f;

struct Reveal {
    ImDrawList* dl;
    int start;
    float shift;
    float before;

    Reveal(ImDrawList* list, int index) : dl(list), start(list->VtxBuffer.Size), before(fade) {
        float e = easeOut((pageAge - index * 0.07f) / 0.38f);
        shift = (1.f - e) * 18.f;
        fade = before * e;
    }

    ~Reveal() {
        for (int i = start; i < dl->VtxBuffer.Size; i++) dl->VtxBuffer[i].pos.y += shift;
        fade = before;
    }
};

const char* playLabel(Phase p) {
    switch (p) {
    case Phase::Idle: return tr("Play");
    case Phase::Updating: return tr("Updating");
    case Phase::Starting: return tr("Starting Minecraft");
    case Phase::Waiting: return tr("Waiting for the game");
    case Phase::Injecting: return tr("Connecting");
    case Phase::Done: return tr("Running");
    case Phase::Failed: return tr("Try again");
    }
    return "";
}

bool busy(Phase p) {
    return p == Phase::Updating || p == Phase::Starting || p == Phase::Waiting || p == Phase::Injecting;
}

void sidebar(ImDrawList* dl, State& s, Events& ev) {
    dl->AddRectFilled({0, 0}, {232.f, height}, col(side));
    dl->AddLine({232.f, 0.f}, {232.f, height}, col(border, 0.7f));
    pixelHeart(dl, {28.f, 26.f}, 4.f);
    label(dl, bold, 28.f, {84.f, 28.f}, col(text), "Monchi");

    struct Item { const char* name; Icon icon; Page page; };
    const Item items[] = {
        {tr("Home"), Icon::Play, Page::Start},
        {tr("Cosmetics"), Icon::Figure, Page::Cosmetics},
        {tr("Versions"), Icon::Layers, Page::Versions},
        {tr("Settings"), Icon::Sliders, Page::Settings},
    };

    float rowH = 48.f, top = 118.f;
    float& marker = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("marker"), top);
    int active = 0;
    for (int i = 0; i < 4; i++)
        if (items[i].page == s.page) active = i;
    marker = approach(marker, top + active * (rowH + 4.f), 14.f);

    dl->AddRectFilled({14.f, marker}, {218.f, marker + rowH}, col(mix(surface, text, 0.05f), 0.92f), 8.f);
    dl->AddRect({14.f, marker}, {218.f, marker + rowH}, col(border, 0.85f), 8.f);
    dl->AddRectFilled({14.f, marker + 14.f}, {17.f, marker + rowH - 14.f}, col(accent2), 2.f);

    for (int i = 0; i < 4; i++) {
        float y = top + i * (rowH + 4.f);
        bool hovered, held;
        if (region(items[i].name, {14.f, y}, {218.f, y + rowH}, hovered, held)) s.page = items[i].page;
        float& h = anim(ImGui::GetItemID(), hovered ? 1.f : 0.f);
        bool on = items[i].page == s.page;
        if (!on && h > 0.f) dl->AddRectFilled({14.f, y}, {218.f, y + rowH}, col(surface, 0.7f * h), 8.f);
        ImVec4 c = on ? accent2 : mix(dim, text, h);
        icon(dl, items[i].icon, {44.f, y + rowH * 0.5f}, col(c));
        label(dl, on ? bold : regular, 17.f, {68.f + 2.f * h, y + rowH * 0.5f - 10.f}, col(on ? text : c), items[i].name);
    }

    float by = height - 74.f;
    label(dl, regular, 14.f, {28.f, by + 8.f}, col(dim), (std::string(tr("Version")) + " " + s.clientVersion).c_str());
    if (s.updateAvailable) {
        dl->AddCircleFilled({32.f, by + 38.f}, 4.f, col(warn), 12);
        label(dl, bold, 13.f, {44.f, by + 30.f}, col(warn), tr("Update available"));
    } else {
        dl->AddCircleFilled({32.f, by + 38.f}, 4.f, col(s.updateKnown ? ok : dim), 12);
        label(dl, regular, 13.f, {44.f, by + 30.f}, col(s.updateKnown ? ok : dim), tr(s.updateKnown ? "Up to date" : "Not checked"));
    }
}

void titlebar(ImDrawList* dl, Events& ev) {
    float x = width - controlsWidth;
    bool hovered, held;
    if (region("min", {x, 0.f}, {x + 48.f, titleHeight}, hovered, held)) ev.minimize = true;
    float& hm = anim(ImGui::GetItemID(), hovered ? 1.f : 0.f);
    dl->AddRectFilled({x + 8.f, 8.f}, {x + 40.f, titleHeight - 8.f}, col(surfaceHover, hm), 6.f);
    dl->AddLine({x + 17.f, titleHeight * 0.5f}, {x + 31.f, titleHeight * 0.5f}, col(mix(dim, text, hm)), 2.f);

    if (region("close", {x + 48.f, 0.f}, {x + 96.f, titleHeight}, hovered, held)) ev.close = true;
    float& hc = anim(ImGui::GetItemID(), hovered ? 1.f : 0.f);
    dl->AddRectFilled({x + 56.f, 8.f}, {x + 88.f, titleHeight - 8.f}, col(hex(0xC0392B), hc), 6.f);
    ImVec2 c{x + 72.f, titleHeight * 0.5f};
    ImU32 xc = col(mix(dim, hex(0xFFFFFF), hc));
    dl->AddLine({c.x - 6.f, c.y - 6.f}, {c.x + 6.f, c.y + 6.f}, xc, 2.f);
    dl->AddLine({c.x + 6.f, c.y - 6.f}, {c.x - 6.f, c.y + 6.f}, xc, 2.f);
}

void progressBar(ImDrawList* dl, ImVec2 min, ImVec2 max, float value, bool indeterminate) {
    float r = (max.y - min.y) * 0.5f;
    dl->AddRectFilled(min, max, col(field), r);
    float w = max.x - min.x;
    float t = float(ImGui::GetTime());
    if (indeterminate) {
        float seg = w * 0.32f;
        float x = min.x + (w + seg) * std::fmod(t * 0.55f, 1.f) - seg;
        dl->PushClipRect(min, max, true);
        gradient(dl, {x, min.y}, {x + seg, max.y}, r, accent, accent2);
        dl->PopClipRect();
        return;
    }
    static float shown = 0.f;
    shown = approach(shown, std::clamp(value, 0.f, 1.f), 12.f);
    float fill = std::max(r * 2.f, w * shown);
    gradient(dl, min, {min.x + fill, max.y}, r, accent, accent2);
}

const char* skinHint() {
    if (look::skinFromGameFiles()) return tr("The skin picked in Minecraft");
    if (!look::usingStandIn()) return tr("The skin you wore last in the game");
    if (look::skinLocked()) return tr("Marketplace skin: yours shows after one game");
    return tr("Your skin shows here after one game with Monchi");
}

// the two built-in figures, offered while the player's own skin is not known
void figurePick(State& s, Events& ev, ImVec2 at, bool stacked) {
    if (!look::usingStandIn()) return;
    const char* names[] = {tr("Boy"), tr("Girl")};
    for (int i = 0; i < 2; i++) {
        ImVec2 min = stacked ? ImVec2{at.x, at.y + i * 32.f} : ImVec2{at.x + i * 70.f, at.y};
        ImGui::PushID(i);
        if (button("figure", min, {min.x + 64.f, min.y + 26.f}, names[i], false) && s.settings.figure != i) {
            s.settings.figure = i;
            ev.settingsChanged = true;
        }
        ImGui::PopID();
        if (s.settings.figure == i) ImGui::GetWindowDrawList()->AddRect(min, {min.x + 64.f, min.y + 26.f}, col(accent2), 6.f);
    }
}

void hero(ImDrawList* dl, State& s, Events& ev) {
    Reveal reveal(dl, 0);
    ImVec2 min{264.f, 62.f}, max{928.f, 292.f};
    card(dl, min, max);
    dl->AddRectFilled({min.x, min.y + 22.f}, {min.x + 3.f, max.y - 22.f}, col(accent), 2.f);

    bool found = !s.gameVersion.empty();
    label(dl, regular, 34.f, {min.x + 36.f, min.y + 30.f}, col(text), found ? tr("Ready to play") : tr("Minecraft not found"));
    std::string sub = found ? "Minecraft Bedrock " + s.gameVersion : std::string(tr("Install it from the Microsoft Store or add a folder under Versions."));
    label(dl, regular, 16.f, {min.x + 38.f, min.y + 78.f}, col(dim), sub.c_str());

    bool working = busy(s.phase);
    bool playable = !working && !s.versionBusy && s.phase != Phase::Done;
    ImVec2 bmin{min.x + 36.f, min.y + 118.f}, bmax{min.x + 300.f, min.y + 170.f};
    if (button("play", bmin, bmax, playLabel(s.phase), true, playable)) ev.play = true;

    if (working || !s.status.empty() || s.phase == Phase::Failed || s.phase == Phase::Done) {
        label(dl, regular, 15.f, {min.x + 36.f, max.y - 34.f}, col(dim), s.status.c_str());
    }
    if (working) progressBar(dl, {min.x + 36.f, max.y - 16.f}, {max.x - 36.f, max.y - 10.f}, s.progress, s.progress <= 0.f);

}

void header(ImDrawList* dl, const char* title, const char* subtitle);

void cosmeticsPage(ImDrawList* dl, State& s, Events& ev) {
    {
        Reveal r(dl, 0);
        header(dl, tr("Cosmetics"), tr("What you wear in the game. Drag the figure to turn it."));
    }
    Reveal r(dl, 1);
    ImVec2 min{264.f, 130.f}, max{544.f, 572.f};
    card(dl, min, max);
    figurePick(s, ev, {min.x + 12.f, min.y + 12.f}, false);
    look::standIn(s.settings.figure);
    look::figure(dl, "lookFigure", {min.x + 8.f, min.y + 46.f}, {max.x - 8.f, max.y - 30.f}, accent);
    const char* under = skinHint();
    centered(dl, regular, 12.f, {(min.x + max.x) * 0.5f, max.y - 18.f}, col(dim), under);

    auto& list = look::entries();
    bool locked = look::gameOwnsSettings() || look::settingsMissing();
    float top = 130.f;
    if (locked) {
        const char* why = look::gameOwnsSettings() ? tr("Minecraft is running: change cosmetics in the game menu, or close the game.")
                                                   : tr("Start the game once with Monchi, then cosmetics can be picked here.");
        label(dl, regular, 13.f, {560.f, top + 2.f}, col(warn), why);
        top += 26.f;
    }
    ImGui::SetCursorScreenPos({560.f, top});
    ImGui::BeginChild("cosmeticList", {368.f, 572.f - top}, 0, ImGuiWindowFlags_NoBackground);
    auto* rows = ImGui::GetWindowDrawList();
    if (list.empty()) ImGui::TextDisabled("%s", tr("No cosmetics found."));
    std::string pick;
    for (auto& e : list) {
        ImGui::PushID(e.id.c_str());
        ImVec2 at = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImVec2 end{at.x + w, at.y + 46.f};
        rows->AddRectFilled(at, end, col(surface, e.on ? 1.f : 0.8f), 8.f);
        rows->AddRect(at, end, col(e.on ? accent2 : border, e.on ? 0.8f : 0.5f), 8.f);
        label(rows, bold, 15.f, {at.x + 14.f, at.y + 6.f}, col(text), e.name.c_str());
        label(rows, regular, 12.f, {at.x + 14.f, at.y + 26.f}, col(dim), tr(e.slot.c_str()));
        bool on = e.on;
        ImGui::BeginDisabled(locked);
        if (toggle("on", {end.x - 54.f, at.y + 12.f}, on) && !locked) pick = e.id;
        ImGui::EndDisabled();
        ImGui::SetCursorScreenPos({at.x, end.y + 6.f});
        ImGui::Dummy({w, 0.f});
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (!pick.empty()) look::toggle(pick);
}

void infoCard(ImDrawList* dl, State& s, Events& ev) {
    Reveal reveal(dl, 2);
    ImVec2 min{264.f, 312.f}, max{540.f, 576.f};
    card(dl, min, max);
    label(dl, bold, 18.f, {min.x + 22.f, min.y + 18.f}, col(text), "Status");

    float y = min.y + 62.f;
    label(dl, regular, 14.f, {min.x + 22.f, y}, col(dim), "Monchi");
    label(dl, bold, 18.f, {min.x + 22.f, y + 20.f}, col(text), s.clientVersion.c_str());
    if (s.updateAvailable) chip(dl, {max.x - 22.f - chipWidth("Update"), y + 8.f}, "Update", warn);
    else {
        const char* status = tr(s.updateKnown ? "Current" : "Not checked");
        chip(dl, {max.x - 22.f - chipWidth(status), y + 8.f}, status, s.updateKnown ? ok : dim);
    }

    y += 78.f;
    label(dl, regular, 14.f, {min.x + 22.f, y}, col(dim), "Minecraft");
    label(dl, bold, 18.f, {min.x + 22.f, y + 20.f}, col(text), s.gameVersion.empty() ? tr("not found") : s.gameVersion.c_str());
    const char* tag = s.gameVersion.empty() ? tr("Missing") : s.gameSupported ? tr("Supported") : tr("Untested");
    ImVec4 tc = s.gameVersion.empty() ? off : s.gameSupported ? ok : warn;
    chip(dl, {max.x - 22.f - chipWidth(tag), y + 8.f}, tag, tc);

    std::string t = s.updateAvailable ? i18n::fmt("Update to {}", s.latestVersion) : std::string(tr("Check for updates"));
    if (button("update", {min.x + 22.f, max.y - 56.f}, {max.x - 22.f, max.y - 18.f}, t.c_str(), false, !busy(s.phase) && !s.versionBusy)) {
        if (s.updateAvailable) ev.update = true;
        else ev.checkUpdate = true;
    }
}

void changelogCard(ImDrawList* dl, State& s) {
    Reveal reveal(dl, 3);
    ImVec2 min{556.f, 312.f}, max{928.f, 576.f};
    card(dl, min, max);
    std::string title = i18n::fmt("What's new in {}", s.latestVersion.empty() ? s.clientVersion : s.latestVersion);
    label(dl, bold, 18.f, {min.x + 22.f, min.y + 18.f}, col(text), title.c_str());

    dl->PushClipRect({min.x, min.y + 52.f}, {max.x, max.y - 10.f}, true);
    std::istringstream in(s.changelog.empty() ? std::string(tr("No release notes yet.")) : s.changelog);
    std::string line;
    float y = min.y + 62.f;
    while (std::getline(in, line) && y < max.y - 24.f) {
        if (line.empty()) continue;
        dl->AddCircleFilled({min.x + 28.f, y + 10.f}, 3.f, col(accent2), 10);
        label(dl, regular, 15.f, {min.x + 42.f, y}, col(dim), line.c_str());
        y += 30.f;
    }
    dl->PopClipRect();
}

void header(ImDrawList* dl, const char* title, const char* subtitle) {
    label(dl, bold, 30.f, {264.f, 54.f}, col(text), title);
    label(dl, regular, 16.f, {264.f, 94.f}, col(dim), subtitle);
}

std::string shortPath(const std::string& path, float width) {
    if (measure(regular, 12.f, path.c_str()).x <= width) return path;
    std::string tail = path;
    while (tail.size() > 8 && measure(regular, 12.f, ("..." + tail).c_str()).x > width) tail.erase(0, 1);
    return "..." + tail;
}

void versions(ImDrawList* dl, State& s, Events& ev) {
    {
        Reveal r(dl, 0);
        header(dl, tr("Versions"), tr("Choose a version to install or play."));
    }

    {
        Reveal r(dl, 1);
        if (button("downloadTab", {264.f, 126.f}, {424.f, 164.f}, tr("Downloads"), s.downloadsOpen, !s.versionBusy)) {
            s.downloadsOpen = true;
            if (s.downloads.empty()) ev.loadVersions = true;
        }
        if (button("installedTab", {432.f, 126.f}, {628.f, 164.f}, tr("Installed versions"), !s.downloadsOpen, !s.versionBusy)) s.downloadsOpen = false;
    }

    if (s.downloadsOpen) {
        label(dl, regular, 13.f, {286.f, 181.f}, col(dim), s.versionStatus.c_str());
        if (s.versionBusy) {
            progressBar(dl, {286.f, 205.f}, {748.f, 211.f}, s.versionProgress, s.versionProgress <= 0.f);
            if (!s.versionInstalling && button("cancelVersion", {772.f, 180.f}, {906.f, 215.f}, tr("Cancel"), false)) ev.cancelVersion = true;
        } else if (button("refreshVersions", {772.f, 180.f}, {906.f, 215.f}, tr("Refresh"), false)) ev.loadVersions = true;
        ImGui::SetCursorScreenPos({286.f, 231.f});
        ImGui::BeginChild("versionDownloads", {620.f, 336.f}, 0, ImGuiWindowFlags_NoBackground);
        auto* rows = ImGui::GetWindowDrawList();
        for (size_t i = 0; i < s.downloads.size(); ++i) {
            auto& v = s.downloads[i];
            ImGui::PushID(int(i));
            ImVec2 at = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            ImVec2 end{at.x + width, at.y + 72.f};
            rows->AddRectFilled(at, end, col(surface, 0.8f), 10.f);
            rows->AddRect(at, end, col(border, 0.55f), 10.f);
            label(rows, bold, 18.f, {at.x + 16.f, at.y + 13.f}, col(text), v.name.c_str());
            const char* status = tr(v.supported ? "Supported" : "Not supported");
            ImVec4 tint = v.supported ? ok : hex(0xF27986);
            chip(rows, {at.x + 16.f, at.y + 40.f}, status, tint);
            float x = at.x + 24.f + chipWidth(status);
            chip(rows, {x, at.y + 40.f}, tr("Release"), dim);
            ImGui::SetCursorScreenPos({end.x - 126.f, at.y + 18.f});
            ImGui::PushStyleColor(ImGuiCol_Button, mix(accent, surface, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent2);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
            ImGui::BeginDisabled(s.versionBusy || v.installed);
            if (ImGui::Button(tr(v.installed ? "Installed" : "Install"), {110.f, 36.f})) ev.installVersion = int(i);
            ImGui::EndDisabled();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            ImGui::SetCursorScreenPos({at.x, end.y + 8.f});
            ImGui::Dummy({width, 0.f});
            ImGui::PopID();
        }
        ImGui::EndChild();
        return;
    }

    {
        Reveal r(dl, 2);
        label(dl, bold, 17.f, {264.f, 187.f}, col(text), tr("Installed on this PC"));
        if (button("rescan", {736.f, 181.f}, {824.f, 211.f}, tr("Rescan"), false)) ev.rescan = true;
        if (button("addFolder", {832.f, 181.f}, {928.f, 211.f}, tr("Add folder"), false)) ev.addFolder = true;
    }

    float y = 231.f;
    if (s.versions.empty()) {
        Reveal r(dl, 3);
        ImVec2 min{264.f, y}, max{928.f, y + 54.f};
        card(dl, min, max);
        label(dl, regular, 15.f, {min.x + 22.f, min.y + 17.f}, col(dim), tr("No Minecraft installation found."));
    }
    for (size_t i = 0; i < s.versions.size() && y < 580.f; i++, y += 62.f) {
        Reveal r(dl, int(i) + 3);
        auto& v = s.versions[i];
        ImVec2 min{264.f, y}, max{928.f, y + 54.f};
        card(dl, min, max, v.active ? mix(surface, accent, 0.18f) : surface);
        std::string name = v.name.empty() ? std::string(tr("Not installed")) : v.name;
        label(dl, bold, 18.f, {min.x + 22.f, min.y + 6.f}, col(text), name.c_str());
        float cx = min.x + 22.f;
        const char* kind = v.store ? tr("Microsoft Store") : tr("Own copy");
        chip(dl, {cx, min.y + 28.f}, kind, dim);
        cx += chipWidth(kind) + 6.f;
        if (v.preview) {
            chip(dl, {cx, min.y + 28.f}, "Preview", accent2);
            cx += chipWidth("Preview") + 6.f;
        }
        if (v.supported) chip(dl, {cx, min.y + 28.f}, tr("Monchi compatible"), ok);
        else chip(dl, {cx, min.y + 28.f}, tr("Not supported"), hex(0xF27986));
        if (!v.path.empty()) {
            float x = min.x + 22.f + measure(bold, 18.f, name.c_str()).x + 14.f;
            label(dl, regular, 12.f, {x, min.y + 11.f}, col(dim), shortPath(v.path, max.x - 170.f - x).c_str());
        }
        if (v.active) {
            chip(dl, {max.x - 22.f - chipWidth(tr("In use")), min.y + 15.f}, tr("In use"), ok);
        } else {
            std::string id = "use" + std::to_string(i);
            bool can = !v.store || !v.name.empty();
            if (button(id.c_str(), {max.x - 22.f - 130.f, min.y + 10.f}, {max.x - 22.f, min.y + 44.f}, tr("Use this one"), false, can)) ev.pick = int(i);
        }
    }
}

int segment(const char* id, ImVec2 pos, const char* const* names, int count, int current) {
    float w = 92.f, h = 32.f;
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, {pos.x + w * count, pos.y + h}, col(field), 8.f);
    float& slot = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID(id), float(current));
    slot = approach(slot, float(current), 16.f);
    dl->AddRectFilled({pos.x + 3.f + slot * w, pos.y + 3.f}, {pos.x + w * (slot + 1.f) - 3.f, pos.y + h - 3.f},
                      col(accent), 6.f);
    int result = current;
    for (int i = 0; i < count; i++) {
        bool hovered, held;
        std::string item = std::string(id) + std::to_string(i);
        if (region(item.c_str(), {pos.x + i * w, pos.y}, {pos.x + (i + 1) * w, pos.y + h}, hovered, held)) result = i;
        float on = std::clamp(1.f - std::fabs(slot - i), 0.f, 1.f);
        centered(dl, bold, 14.f, {pos.x + (i + 0.5f) * w, pos.y + h * 0.5f}, col(mix(hovered ? text : dim, hex(0xFFFFFF), on)), names[i]);
    }
    return result;
}

bool settingRow(ImDrawList* dl, const char* id, float y, int index, const char* title, const char* desc, bool& value) {
    Reveal r(dl, index);
    ImVec2 min{264.f, y}, max{928.f, y + 50.f};
    card(dl, min, max);
    label(dl, bold, 16.f, {min.x + 22.f, min.y + 7.f}, col(text), title);
    label(dl, regular, 14.f, {min.x + 22.f, min.y + 28.f}, col(dim), desc);
    return toggle(id, {max.x - 22.f - 40.f, min.y + 14.f}, value);
}

bool accentRow(ImDrawList* dl, float y, int index, int& current) {
    Reveal r(dl, index);
    ImVec2 min{264.f, y}, max{928.f, y + 50.f};
    card(dl, min, max);
    label(dl, bold, 16.f, {min.x + 22.f, min.y + 7.f}, col(text), tr("Accent color"));
    label(dl, regular, 14.f, {min.x + 22.f, min.y + 28.f}, col(dim), tr("Same choice as in the client menu."));
    bool changed = false;
    float cy = min.y + 25.f;
    for (int i = 0; i < 8; i++) {
        ImVec2 c{max.x - 22.f - 10.f - (7 - i) * 28.f, cy};
        bool hovered, held;
        std::string id = std::string("accent") + std::to_string(i);
        if (region(id.c_str(), {c.x - 11.f, c.y - 11.f}, {c.x + 11.f, c.y + 11.f}, hovered, held)) {
            current = i;
            changed = true;
        }
        bool on = current == i;
        dl->AddCircleFilled(c, hovered || on ? 10.f : 9.f, col(hex(accents[i].a)), 24);
        if (on) {
            dl->AddCircle(c, 13.f, col(text, 0.85f), 24, 1.5f);
            dl->AddCircleFilled(c, 3.2f, col(hex(accents[i].b)), 12);
        }
        if (hovered) ImGui::SetTooltip("%s", tr(accents[i].name));
    }
    return changed;
}

void settings(ImDrawList* dl, State& s, Events& ev) {
    ImGui::PushID("settings");
    {
        Reveal r(dl, 0);
        header(dl, tr("Settings"), tr("How the launcher should behave."));
    }
    bool changed = false;

    {
        Reveal r(dl, 1);
        ImVec2 lmin{264.f, 130.f}, lmax{928.f, 180.f};
        card(dl, lmin, lmax);
        label(dl, bold, 16.f, {lmin.x + 22.f, lmin.y + 7.f}, col(text), tr("Language"));
        label(dl, regular, 14.f, {lmin.x + 22.f, lmin.y + 28.f}, col(dim), tr("Auto follows your Windows language."));
        const char* names[] = {tr("Auto"), "English", "Deutsch"};
        int now = int(i18n::chosen());
        int pick = segment("lang", {lmax.x - 22.f - 276.f, lmin.y + 9.f}, names, 3, now);
        if (pick != now) i18n::choose(i18n::Lang(pick));
    }

    int accentNow = s.settings.accent >= 0 ? s.settings.accent : s.clientAccent;
    if (accentRow(dl, 186.f, 2, accentNow)) {
        s.settings.accent = accentNow;
        changed = true;
    }
    changed |= settingRow(dl, "beta", 242.f, 3, tr("Beta updates"), tr("Get new versions earlier, even if they may still have bugs."), s.settings.beta);
    changed |= settingRow(dl, "auto", 298.f, 4, tr("Connect automatically"), tr("Connects the client as soon as Minecraft has started."), s.settings.autoInject);
    changed |= settingRow(dl, "close", 354.f, 5, tr("Close launcher afterwards"), tr("Closes this window once the client has loaded."), s.settings.closeAfterInject);

    {
        Reveal r(dl, 6);
        ImVec2 min{264.f, 410.f}, max{928.f, 490.f};
        card(dl, min, max);
        label(dl, bold, 16.f, {min.x + 22.f, min.y + 7.f}, col(text), tr("Custom DLL (for developers)"));
        label(dl, regular, 14.f, {min.x + 22.f, min.y + 28.f}, col(dim), tr("Leave empty for the normal version."));
        ImVec2 fmin{min.x + 22.f, min.y + 50.f}, fmax{max.x - 150.f, min.y + 76.f};
        dl->AddRectFilled(fmin, fmax, col(field), 6.f);
        dl->AddRect(fmin, fmax, col(border, 0.8f), 6.f);
        ImGui::SetCursorScreenPos({fmin.x + 10.f, fmin.y + 3.f});
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        ImGui::SetNextItemWidth(fmax.x - fmin.x - 20.f);
        if (ImGui::InputText("##dll", s.dllPath, sizeof(s.dllPath))) {
            s.settings.customDll = s.dllPath;
            changed = true;
        }
        ImGui::PopStyleColor(2);
        if (button("browse", {max.x - 134.f, min.y + 48.f}, {max.x - 22.f, min.y + 78.f}, tr("Browse"), false)) ev.browseDll = true;
    }

    {
        Reveal r(dl, 7);
        ImVec2 min{264.f, 496.f}, max{928.f, 580.f};
        card(dl, min, max);
        pixelHeart(dl, {min.x + 20.f, min.y + 14.f}, 3.f);
        label(dl, bold, 16.f, {min.x + 62.f, min.y + 10.f}, col(text), (std::string("Monchi Launcher ") + build::version).c_str());
        label(dl, regular, 13.f, {min.x + 62.f, min.y + 31.f}, col(dim), tr("Monchi is an independent project and is not affiliated with Mojang or Microsoft."));
        if (button("logs", {min.x + 22.f, max.y - 32.f}, {min.x + 162.f, max.y - 8.f}, tr("Open logs"), false)) ev.openLogs = true;
        if (button("folder", {min.x + 172.f, max.y - 32.f}, {min.x + 312.f, max.y - 8.f}, tr("Open folder"), false)) ev.openFolder = true;
    }
    ev.settingsChanged |= changed;
    ImGui::PopID();
}

}

int nearestAccent(float r, float g, float b) {
    int best = 0;
    float bestDist = 1e9f;
    for (int i = 0; i < 8; i++) {
        ImVec4 c = hex(accents[i].a);
        float d = (c.x - r) * (c.x - r) + (c.y - g) * (c.y - g) + (c.z - b) * (c.z - b);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void setFonts(ImFont* r, ImFont* b) {
    regular = r;
    bold = b ? b : r;
}

void draw(State& s, Events& ev) {
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    ImGui::Begin("launcher", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);
    auto* dl = ImGui::GetWindowDrawList();

    if (s.page != lastPage) {
        lastPage = s.page;
        pageAge = 0.f;
    }
    pageAge += ImGui::GetIO().DeltaTime;

    int pick = std::clamp(s.settings.accent >= 0 ? s.settings.accent : s.clientAccent, 0, 7);
    accent = hex(accents[pick].a);
    accent2 = hex(accents[pick].b);

    fade = 1.f;
    sidebar(dl, s, ev);
    titlebar(dl, ev);

    ImGui::SetCursorScreenPos({0, 0});
    switch (s.page) {
    case Page::Start:
        hero(dl, s, ev);
        infoCard(dl, s, ev);
        changelogCard(dl, s);
        break;
    case Page::Cosmetics: cosmeticsPage(dl, s, ev); break;
    case Page::Versions: versions(dl, s, ev); break;
    case Page::Settings: settings(dl, s, ev); break;
    case Page::About: break;
    }

    if (s.updatePrompt && !busy(s.phase) && !s.versionBusy) ImGui::OpenPopup("updateOffer");
    ImGui::SetNextWindowPos({width * 0.5f, height * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({440.f, 190.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {24.f, 22.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Button, accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent2);
    if (ImGui::BeginPopupModal("updateOffer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushFont(bold);
        ImGui::TextUnformatted(tr("Update available"));
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", i18n::fmt("Version {} is available.", s.latestVersion).c_str());
        ImGui::TextWrapped("%s", tr("Your settings will stay saved."));
        ImGui::Dummy({0.f, 18.f});
        if (ImGui::Button(tr("Download"), {188.f, 38.f})) {
            ev.update = true;
            s.updatePrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, surface);
        if (ImGui::Button(tr("Later"), {188.f, 38.f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ev.dismissUpdate = true;
            s.updatePrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

}
