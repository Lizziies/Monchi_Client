#include "I18n.hpp"
#include "Widgets.hpp"
#include "Theme.hpp"
#include "core/Config.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <imgui_internal.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace widgets {

static ImGuiID capturing = 0;
static bool capturedThisFrame = false;

struct Drop {
    float open = 0.f;
    float scroll = 0.f;
    float scrollTo = 0.f;
    float viewH = 0.f;
    float fullH = 0.f;
    int kb = -1;
    int sel = 0;
    bool swallow = false;
    bool flip = false;
    ImVec2 min, max;
    std::vector<std::string> items;
    std::vector<float> hv;
};

static std::map<ImGuiID, Drop> drops;
static int escapeFrame = -1;

static float itemH() { return 26 * ui::scale(); }
static float edge() { return 4 * ui::scale(); }

static void shadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float r) {
    float s = ui::scale();
    for (int i = 5; i >= 1; i--) {
        float g = i * 2.2f * s;
        dl->AddRectFilled(a + ImVec2(-g, 3 * s - g), b + ImVec2(g, 3 * s + g), theme::col({0, 0, 0, 1}, 0.07f), r + g);
    }
}

static int paintDrop(ImDrawList* dl, Drop& d, bool live) {
    auto& t = theme::current();
    auto& io = ImGui::GetIO();
    float s = ui::scale();
    float e = draw::easeOutCubic(d.open);
    float r = std::min(t.rounding, 12.f) * 0.75f * s;

    ImVec2 vmin = d.min, vmax = d.max;
    if (d.flip) vmin.y = vmax.y - d.viewH * e;
    else vmax.y = vmin.y + d.viewH * e;
    shadow(dl, vmin, vmax, r);
    dl->AddRectFilled(vmin, vmax, theme::col(theme::mix(t.surface, t.bg, 0.35f), 0.98f), r);
    dl->AddRect(vmin, vmax, theme::col(theme::border(), 0.9f), r, 0, 1.f);

    bool inside = live && ImGui::IsMouseHoveringRect(vmin, vmax, false);
    float maxScroll = std::max(0.f, d.fullH - d.viewH);
    if (live) {
        if (inside && maxScroll > 0.f) d.scrollTo = std::clamp(d.scrollTo - io.MouseWheel * itemH() * 2.f, 0.f, maxScroll);
        d.scroll = draw::approach(d.scroll, d.scrollTo, 18.f * t.animSpeed);
    }

    int n = (int)d.items.size();
    int picked = -1;
    bool moved = io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f;
    if (live) {
        int step = (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : 0) - (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ? 1 : 0);
        if (step) {
            d.kb = std::clamp((d.kb < 0 ? d.sel : d.kb) + step, 0, n - 1);
            float top = edge() + d.kb * itemH();
            if (top < d.scrollTo) d.scrollTo = top - edge();
            else if (top + itemH() > d.scrollTo + d.viewH) d.scrollTo = top + itemH() + edge() - d.viewH;
            d.scrollTo = std::clamp(d.scrollTo, 0.f, maxScroll);
        }
        if (d.kb >= 0 && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) picked = d.kb;
    }

    dl->PushClipRect({vmin.x, vmin.y + 1}, {vmax.x, vmax.y - 1}, true);
    float slide = (1.f - e) * (d.flip ? 8 : -8) * s;
    d.hv.resize(n, 0.f);
    for (int i = 0; i < n; i++) {
        ImVec2 a{d.min.x + edge(), d.min.y + edge() + i * itemH() - d.scroll + slide};
        ImVec2 b{d.max.x - edge(), a.y + itemH()};
        float ia = std::clamp(d.open * 1.6f - i * 0.04f, 0.f, 1.f);
        bool hov = inside && ImGui::IsMouseHoveringRect(a, b, false);
        if (hov && moved) d.kb = i;
        if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) picked = i;
        d.hv[i] = draw::approach(d.hv[i], live && d.kb == i ? 1.f : 0.f, 26.f * t.animSpeed);

        bool on = i == d.sel;
        if (d.hv[i] > 0.01f) dl->AddRectFilled(a, b, theme::col(theme::mix(t.surfaceHover, t.accent, 0.3f), d.hv[i] * ia), r * 0.6f);
        float cy = (a.y + b.y) * 0.5f;
        if (on) dl->AddRectFilled({a.x + 1 * s, cy - 5 * s}, {a.x + 3.5f * s, cy + 5 * s}, theme::col(t.accent2, ia), 2 * s);
        float fs = 13.f * s;
        ImVec4 tc = on ? t.accent2 : theme::mix(t.textDim, t.text, 0.55f + 0.45f * d.hv[i]);
        dl->AddText(fonts::regular(), fs, {a.x + 11 * s, cy - fs * 0.53f}, theme::col(tc, ia), d.items[i].c_str());
        if (on) {
            ImVec2 c{b.x - 12 * s, cy};
            ImVec2 pts[3] = {c + ImVec2(-3.5f * s, 0.f), c + ImVec2(-1.2f * s, 2.6f * s), c + ImVec2(3.6f * s, -2.8f * s)};
            dl->AddPolyline(pts, 3, theme::col(t.accent2, ia), 0, 1.6f * s);
        }
    }
    dl->PopClipRect();

    if (maxScroll > 0.f && e > 0.5f) {
        float track = d.viewH - 2 * edge();
        float len = std::max(14 * s, track * d.viewH / d.fullH);
        float y = d.min.y + edge() + (track - len) * (d.scroll / maxScroll);
        dl->AddRectFilled({d.max.x - 5 * s, y}, {d.max.x - 2.5f * s, y + len}, theme::col(t.textDim, 0.5f), 2 * s);
    }
    return picked;
}

float dropdownOpen(const char* id) {
    auto it = drops.find(ImGui::GetID(id));
    return it == drops.end() ? 0.f : draw::easeOutCubic(it->second.open);
}

bool dropdownEscaped() { return escapeFrame == ImGui::GetFrameCount(); }

void closePopups() {
    if (ImGui::GetCurrentContext() && ImGui::GetCurrentContext()->OpenPopupStack.Size > 0) ImGui::ClosePopupToLevel(0, true);
    for (auto& [id, d] : drops) d.open = 0.f;
}

bool dropdown(const char* id, ImVec2 amin, ImVec2 amax, const std::vector<std::string>& choices, int& sel, bool hovered, bool clicked) {
    float s = ui::scale();
    Drop& d = drops[ImGui::GetID(id)];
    bool isOpen = ImGui::IsPopupOpen(id);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isOpen) d.swallow = true;
    if (clicked) {
        if (d.swallow) {
            d.swallow = false;
        } else {
            ImGui::OpenPopup(id);
            isOpen = true;
            d.kb = sel;
            d.scrollTo = d.scroll = 0.f;
        }
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        d.swallow = false;
    }

    float step = draw::motion() ? ui::dt() / (isOpen ? 0.12f + d.viewH * 0.0003f / s : 0.09f + d.viewH * 0.0002f / s) : 1.f;
    d.open = std::clamp(d.open + (isOpen ? step : -step), 0.f, 1.f);
    if (!isOpen && d.open <= 0.f) return false;

    if (isOpen) {
        d.sel = sel;
        d.items.clear();
        for (auto& c : choices) d.items.push_back(i18n::tr(c.c_str()));
    }
    int n = (int)d.items.size();
    auto ds = ImGui::GetIO().DisplaySize;
    float gap = 4 * s, pad = 8 * s;
    float widest = 0.f;
    for (auto& it : d.items) widest = std::max(widest, fonts::regular()->CalcTextSizeA(13.f * s, FLT_MAX, 0.f, it.c_str()).x);
    float width = std::max({amax.x - amin.x, widest + 2 * edge() + 38 * s, 110 * s});
    d.fullH = n * itemH() + 2 * edge();
    float below = ds.y - amax.y - gap - pad, above = amin.y - gap - pad;
    d.flip = d.fullH > below && above > below;
    d.viewH = std::min({d.fullH, std::max(d.flip ? above : below, 3 * itemH()), ds.y * 0.55f});
    float x1 = std::min(amax.x, ds.x - pad);
    float x0 = std::max(pad, x1 - width);
    d.min = {x0, d.flip ? amin.y - gap - d.viewH : amax.y + gap};
    d.max = {x0 + width, d.min.y + d.viewH};

    if (!isOpen) {
        paintDrop(ImGui::GetForegroundDrawList(), d, false);
        return false;
    }

    float m = 16 * s;
    ImGui::SetNextWindowPos(d.min - ImVec2(m, m));
    ImGui::SetNextWindowSize(d.max - d.min + ImVec2(2 * m, 2 * m));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
    bool changed = false;
    if (ImGui::BeginPopup(id, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove)) {
        int picked = paintDrop(ImGui::GetWindowDrawList(), d, true);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            escapeFrame = ImGui::GetFrameCount();
            ImGui::CloseCurrentPopup();
        } else if (picked >= 0) {
            changed = picked != sel;
            sel = picked;
            d.sel = picked;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    return changed;
}

void drawSwitch(ImDrawList* dl, ImVec2 p, float a, float alpha) {
    auto& t = theme::current();
    float s = ui::scale();
    ImVec2 size = switchSize();
    float r = size.y * 0.5f;
    ImVec4 on = theme::mix(t.accent, t.accent2, 0.35f);
    if (a > 0.02f) dl->AddRectFilled(p - ImVec2(2 * s, 2 * s), p + size + ImVec2(2 * s, 2 * s), theme::col(on, 0.2f * a * alpha), r + 2 * s);
    dl->AddRectFilled(p, p + size, theme::col(theme::mix(t.off, on, a), (0.55f + 0.45f * a) * alpha), r);
    float kr = 3.7f * s;
    float kw = kr * (1.f + 0.5f * std::sin(a * 3.14159f));
    float x = p.x + r + (size.x - size.y) * a;
    ImVec2 c{x, p.y + r};
    dl->AddRectFilled(c - ImVec2(kw, kr) + ImVec2(0, 0.8f * s), c + ImVec2(kw, kr) + ImVec2(0, 0.8f * s), theme::col({0, 0, 0, 1}, 0.25f * alpha), kr);
    dl->AddRectFilled(c - ImVec2(kw, kr), c + ImVec2(kw, kr), theme::col(theme::mix(ImVec4(1, 1, 1, 1), t.accent2, 0.35f * a), alpha), kr);
}

ImVec2 switchSize() {
    float s = ui::scale();
    return {24 * s, 13 * s};
}

bool toggle(const char* id, bool& value, bool enabled) {
    auto& t = theme::current();
    ImVec2 size = switchSize();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##toggle", size) && enabled;
    ImGuiID key = ImGui::GetID("anim");
    ImGui::PopID();
    if (clicked) {
        value = !value;
        config::markDirty();
    }
    float& a = *ImGui::GetStateStorage()->GetFloatRef(key, value ? 1.f : 0.f);
    a = draw::approach(a, value ? 1.f : 0.f, 18.f * t.animSpeed);
    drawSwitch(ImGui::GetWindowDrawList(), p, a, enabled ? 1.f : 0.4f);
    return clicked;
}

static bool pill(const char* id, const char* label, bool primary, ImVec2 at, float h, float& width) {
    auto& t = theme::current();
    float s = ui::scale();
    ImVec2 ts = fonts::regular()->CalcTextSizeA(12 * s, FLT_MAX, 0.f, label);
    width = ts.x + 18 * s;
    ImGui::SetCursorScreenPos(at);
    bool clicked = ImGui::InvisibleButton(id, {width, h});
    bool hov = ImGui::IsItemHovered();
    auto* dl = ImGui::GetWindowDrawList();
    ImVec4 bg = primary ? (hov ? theme::mix(t.accent, t.accent2, 0.3f) : t.accent) : (hov ? t.surfaceHover : t.surface);
    dl->AddRectFilled(at, at + ImVec2(width, h), theme::col(bg), 4 * s);
    dl->AddText(fonts::regular(), 12 * s, at + ImVec2(9 * s, (h - ts.y) * 0.5f), theme::col(primary ? ImVec4(1, 1, 1, 1) : t.text), label);
    return clicked;
}

bool button(const char* id, ImVec2 size, bool primary) {
    auto& t = theme::current();
    float s = ui::scale();
    const char* label = i18n::tr(id);
    const char* end = ImGui::FindRenderedTextEnd(label);
    ImVec2 ts = fonts::regular()->CalcTextSizeA(13.5f * s, FLT_MAX, 0.f, label, end);
    if (size.x <= 0) size.x = ts.x + 24 * s;
    if (size.y <= 0) size.y = 28 * s;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    ImGuiID key = ImGui::GetItemID();
    static std::map<ImGuiID, float> hover, press;
    float& hv = hover[key];
    float& pr = press[key];
    hv = draw::approach(hv, hovered ? 1.f : 0.f, 20.f * t.animSpeed);
    pr = draw::approach(pr, held ? 1.f : 0.f, 30.f * t.animSpeed);
    auto* dl = ImGui::GetWindowDrawList();
    float r = 5 * s;
    ImVec2 a = p + ImVec2(0, 0.6f * s * pr), b = p + size - ImVec2(0, 0) + ImVec2(0, 0.6f * s * pr);
    ImVec4 fill = primary ? theme::mix(theme::mix(t.accent, t.accent2, 0.3f * hv), t.bg, 0.2f * pr) : theme::mix(theme::mix(t.surface, t.surfaceHover, hv), t.bg, 0.25f * pr);
    dl->AddRectFilled(a, b, theme::col(fill, primary ? 1.f : 0.7f + 0.3f * hv), r);
    if (hv > 0.01f) dl->AddRect(a, b, theme::col(primary ? t.accent2 : theme::border(), 0.5f * hv), r, 0, 1.f);
    dl->AddText(fonts::regular(), 13.5f * s, a + (size - ts) * 0.5f, theme::col(primary ? ImVec4(1, 1, 1, 1) : t.text), label, end);
    return clicked;
}

void sectionTitle(const char* text) {
    auto& t = theme::current();
    float s = ui::scale();
    ImGui::Dummy({0, 6 * s});
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(fonts::bold(), 13.5f * s, p + ImVec2(2 * s, 0), theme::col(t.text), i18n::tr(text));
    ImGui::Dummy({0, 20 * s});
}

void hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::current().textDim);
    ImGui::TextWrapped("%s", i18n::tr(text));
    ImGui::PopStyleColor();
}

std::string keyName(int vk) {
    switch (vk) {
    case 0: return i18n::tr("None");
    case VK_LBUTTON: return i18n::tr("Mouse left");
    case VK_RBUTTON: return i18n::tr("Mouse right");
    case VK_MBUTTON: return i18n::tr("Mouse wheel");
    case VK_XBUTTON1: return i18n::tr("Mouse 4");
    case VK_XBUTTON2: return i18n::tr("Mouse 5");
    case VK_RSHIFT: return i18n::tr("Right Shift");
    case VK_LSHIFT: return i18n::tr("Left Shift");
    case VK_RCONTROL: return i18n::tr("Right Ctrl");
    case VK_LCONTROL: return i18n::tr("Left Ctrl");
    case VK_LMENU: return i18n::tr("Alt");
    case VK_RMENU: return i18n::tr("Alt Gr");
    case VK_INSERT: return i18n::tr("Insert");
    case VK_DELETE: return i18n::tr("Delete");
    case VK_HOME: return i18n::tr("Home");
    case VK_END: return i18n::tr("End");
    case VK_PRIOR: return i18n::tr("Page up");
    case VK_NEXT: return i18n::tr("Page down");
    case VK_TAB: return i18n::tr("Tab");
    case VK_CAPITAL: return i18n::tr("Caps lock");
    case VK_SPACE: return i18n::tr("Space");
    }
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    switch (vk) {
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
        scan |= 0x100;
    }
    wchar_t name[64]{};
    if (GetKeyNameTextW(LONG(scan << 16), name, 64) > 0) {
        char out[128]{};
        WideCharToMultiByte(CP_UTF8, 0, name, -1, out, sizeof(out), nullptr, nullptr);
        return out;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

bool capturingKey() { return capturing != 0; }

bool keyCapture(const char* id, int& vk) {
    auto& t = theme::current();
    float s = ui::scale();
    ImGuiID gid = ImGui::GetID(id);
    bool active = capturing == gid;

    std::string label = active ? "..." : (vk ? keyName(vk) : std::string(i18n::tr("none")));
    ImVec2 ts = fonts::regular()->CalcTextSizeA(12 * s, FLT_MAX, 0.f, label.c_str());
    ImVec2 size{std::max(40 * s, ts.x + 18 * s), 19 * s};
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##key", size);
    bool rclick = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    bool hov = ImGui::IsItemHovered();
    ImGui::PopID();

    auto* dl = ImGui::GetWindowDrawList();
    if (active) dl->AddRectFilled(p, p + size, theme::col(t.accent, 0.25f), 4 * s);
    dl->AddRect(p, p + size, theme::col(active ? t.accent2 : (hov ? t.text : t.textDim), 0.9f), 4 * s, 0, 1.2f * s);
    dl->AddText(fonts::regular(), 12 * s, p + (size - ts) * 0.5f, theme::col(active ? t.accent2 : t.text), label.c_str());

    bool changed = false;
    if (clicked && !active) {
        capturing = gid;
        capturedThisFrame = true;
    } else if (rclick) {
        vk = 0;
        capturing = 0;
        changed = true;
    } else if (active && !capturedThisFrame) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            capturing = 0;
        } else {
            for (int k = 1; k < 255; k++) {
                if (k == VK_LBUTTON) continue;
                if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU) continue;
                bool pressed = (GetAsyncKeyState(k) & 0x8000) != 0;
                if (pressed) {
                    vk = k;
                    capturing = 0;
                    changed = true;
                    break;
                }
            }
        }
    }
    capturedThisFrame = false;
    if (changed) config::markDirty();
    return changed;
}

static std::string hexOf(const ImVec4& c) {
    auto b = [](float x) { return (int)std::lround(std::clamp(x, 0.f, 1.f) * 255.f); };
    char buf[16];
    if (b(c.w) < 255) snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", b(c.x), b(c.y), b(c.z), b(c.w));
    else snprintf(buf, sizeof(buf), "#%02X%02X%02X", b(c.x), b(c.y), b(c.z));
    return buf;
}

static void rowBack(ImDrawList* dl, ImVec2 p, ImVec2 size, bool hov) {
    auto& t = theme::current();
    static std::map<ImGuiID, float> hover;
    float& h = hover[ImGui::GetID("rowback")];
    h = draw::approach(h, hov ? 1.f : 0.f, 22.f * t.animSpeed);
    dl->AddRectFilled(p, p + size, theme::col(theme::mix(t.surface, t.surfaceHover, 0.45f * h), 0.6f + 0.3f * h), 6 * ui::scale());
}

static void rowLabel(ImDrawList* dl, ImVec2 p, float h, const char* label, ImU32 col) {
    float s = ui::scale();
    ImVec2 ts = fonts::regular()->CalcTextSizeA(14.5f * s, FLT_MAX, 0.f, label);
    dl->AddText(fonts::regular(), 14.5f * s, {p.x + 10 * s, p.y + (h - ts.y) * 0.5f}, col, label);
}

// the whole row is the slider, filled from the left like a progress bar
static bool sliderRow(ImVec2 p, ImVec2 size, const char* label, float& v, float lo, float hi, const char* fmt, bool isInt) {
    auto& t = theme::current();
    float s = ui::scale();
    auto* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(p);
    ImGui::InvisibleButton("slider", size);
    bool active = ImGui::IsItemActive();
    bool hov = ImGui::IsItemHovered();
    bool changed = false;
    if (active) {
        float k = std::clamp((ImGui::GetIO().MousePos.x - p.x) / std::max(1.f, size.x), 0.f, 1.f);
        float nv = lo + k * (hi - lo);
        if (isInt) nv = std::round(nv);
        if (nv != v) {
            v = nv;
            changed = true;
        }
    }
    float frac = hi > lo ? std::clamp((v - lo) / (hi - lo), 0.f, 1.f) : 0.f;
    rowBack(dl, p, size, hov);
    float& shown = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("fill"), frac);
    shown = active ? frac : draw::approach(shown, frac, 22.f * t.animSpeed);
    if (shown > 0.001f) dl->AddRectFilled(p, {p.x + std::max(12 * s, size.x * shown), p.y + size.y}, theme::col(t.accent, active ? 1.f : 0.9f), 6 * s);
    rowLabel(dl, p, size.y, label, theme::col(t.text));
    char buf[32];
    if (isInt) snprintf(buf, sizeof(buf), "%d", (int)std::lround(v));
    else snprintf(buf, sizeof(buf), fmt, v);
    ImVec2 vs = fonts::regular()->CalcTextSizeA(14 * s, FLT_MAX, 0.f, buf);
    dl->AddText(fonts::regular(), 14 * s, {p.x + size.x - vs.x - 10 * s, p.y + (size.y - vs.y) * 0.5f}, theme::col(t.text), buf);
    return changed;
}

bool setting(Setting& st) {
    if (!st.shown()) return false;
    auto& t = theme::current();
    float s = ui::scale();
    auto* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 31 * s;
    ImVec2 size{w, h};
    const char* label = i18n::tr(st.label.c_str());
    bool changed = false;
    float right = p.x + w - 10 * s;
    float cy = p.y + h * 0.5f;

    ImGui::PushID(st.id.c_str());
    switch (st.type) {
    case SettingType::Bool: {
        ImGui::SetCursorScreenPos(p);
        bool clicked = ImGui::InvisibleButton("row", size);
        rowBack(dl, p, size, ImGui::IsItemHovered());
        rowLabel(dl, p, h, label, theme::col(t.text));
        if (clicked) {
            st.b = !st.b;
            changed = true;
        }
        float& a = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("anim"), st.b ? 1.f : 0.f);
        a = draw::approach(a, st.b ? 1.f : 0.f, 18.f * t.animSpeed);
        ImVec2 sw = switchSize();
        drawSwitch(dl, {right - sw.x, cy - sw.y * 0.5f}, a, 1.f);
        break;
    }
    case SettingType::Float:
        changed = sliderRow(p, size, label, st.f, st.fmin, st.fmax, st.format, false);
        break;
    case SettingType::Int: {
        float v = (float)st.i;
        changed = sliderRow(p, size, label, v, (float)st.imin, (float)st.imax, "%d", true);
        if (changed) st.i = (int)std::lround(v);
        break;
    }
    case SettingType::Color: {
        rowBack(dl, p, size, ImGui::IsMouseHoveringRect(p, p + size));
        rowLabel(dl, p, h, label, theme::col(t.text));
        float sw = 26 * s, sh = 15 * s;
        ImVec2 swp{right - sw, cy - sh * 0.5f};
        dl->AddRectFilled(swp, swp + ImVec2(sw, sh), theme::col(st.color), 4 * s);
        dl->AddRect(swp, swp + ImVec2(sw, sh), theme::col(t.text, 0.25f), 4 * s);
        float pw = fonts::regular()->CalcTextSizeA(12 * s, FLT_MAX, 0.f, i18n::tr("Change Color")).x + 18 * s;
        ImGui::SetCursorScreenPos(swp);
        bool swClicked = ImGui::InvisibleButton("swatch", {sw, sh});
        bool pillClicked = pill("change", i18n::tr("Change Color"), true, {swp.x - pw - 8 * s, cy - 9.5f * s}, 19 * s, pw);
        if (swClicked || pillClicked) ImGui::OpenPopup("picker");
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12 * s, 12 * s});
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, std::min(t.rounding, 12.f) * 0.75f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, theme::mix(t.surface, t.bg, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Border, theme::border());
        if (ImGui::BeginPopup("picker")) {
            float c[4] = {st.color.x, st.color.y, st.color.z, st.color.w};
            if (ImGui::ColorPicker4("##pick", c, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview)) {
                st.color = {c[0], c[1], c[2], c[3]};
                changed = true;
            }
            ImGui::TextDisabled("%s", hexOf(st.color).c_str());
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        break;
    }
    case SettingType::Choice: {
        ImGui::SetCursorScreenPos(p);
        bool clicked = ImGui::InvisibleButton("row", size);
        bool hov = ImGui::IsItemHovered();
        rowBack(dl, p, size, hov);
        rowLabel(dl, p, h, label, theme::col(t.text));
        const char* value = st.choices.empty() ? "" : i18n::tr(st.choices[std::clamp(st.i, 0, (int)st.choices.size() - 1)].c_str());
        ImVec2 vs = fonts::regular()->CalcTextSizeA(12.5f * s, FLT_MAX, 0.f, value);
        ImVec2 bmin{right - vs.x - 32 * s, cy - 10.5f * s}, bmax{right, cy + 10.5f * s};
        float open = dropdownOpen("choices");
        float lit = std::max(open, hov ? 0.6f : 0.f);
        dl->AddRectFilled(bmin, bmax, theme::col(theme::mix(t.bg, t.surfaceHover, 0.5f + 0.5f * lit), 0.8f), 5 * s);
        dl->AddRect(bmin, bmax, theme::col(theme::mix(theme::border(), t.accent2, open), 0.4f + 0.6f * lit), 5 * s, 0, 1.f);
        dl->AddText(fonts::regular(), 12.5f * s, {bmin.x + 10 * s, cy - vs.y * 0.5f}, theme::col(t.text), value);
        float ang = open * 3.14159f;
        ImVec2 ac{bmax.x - 11 * s, cy};
        auto rot = [&](float x, float y) { return ac + ImVec2((x * std::cos(ang) - y * std::sin(ang)) * s, (x * std::sin(ang) + y * std::cos(ang)) * s); };
        ImVec2 pts[3] = {rot(-3.5f, -1.5f), rot(0.f, 2.f), rot(3.5f, -1.5f)};
        dl->AddPolyline(pts, 3, theme::col(theme::mix(t.textDim, t.accent2, open)), 0, 1.5f * s);
        if (dropdown("choices", bmin, bmax, st.choices, st.i, hov, clicked)) changed = true;
        break;
    }
    case SettingType::Key: {
        rowBack(dl, p, size, ImGui::IsMouseHoveringRect(p, p + size));
        rowLabel(dl, p, h, label, theme::col(t.text));
        std::string name = st.i ? keyName(st.i) : std::string(i18n::tr("none"));
        float kw = std::max(40 * s, fonts::regular()->CalcTextSizeA(12 * s, FLT_MAX, 0.f, name.c_str()).x + 18 * s);
        ImGui::SetCursorScreenPos({right - kw, cy - 9.5f * s});
        changed = keyCapture("key", st.i);
        float uw = 0.f;
        float unbindW = fonts::regular()->CalcTextSizeA(12 * s, FLT_MAX, 0.f, i18n::tr("Unbind")).x + 18 * s;
        if (pill("unbind", i18n::tr("Unbind"), true, {right - kw - 8 * s - unbindW, cy - 9.5f * s}, 19 * s, uw)) {
            st.i = 0;
            changed = true;
        }
        break;
    }
    case SettingType::Text: {
        rowBack(dl, p, size, false);
        rowLabel(dl, p, h, label, theme::col(t.text));
        char buf[2048];
        snprintf(buf, sizeof(buf), "%s", st.text.c_str());
        float iw = std::min(w * 0.5f, 240 * s);
        ImGui::SetCursorScreenPos({right - iw, cy - 11 * s});
        ImGui::SetNextItemWidth(iw);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::col(t.bg, 0.7f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * s);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8 * s, 3 * s});
        // an empty field shows what is in effect, greyed; a click into it writes that out so parts can be removed
        if (ImGui::InputTextWithHint("##v", st.prefill.empty() ? st.hint.c_str() : st.prefill.c_str(), buf, sizeof(buf))) {
            st.text = buf;
            changed = true;
        }
        if (ImGui::IsItemActivated() && st.text.empty() && !st.prefill.empty()) {
            st.text = st.prefill;
            changed = true;
            // the field is active and keeps its own copy of the text; it is told to take the new one
            if (ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetItemID())) {
                snprintf(buf, sizeof(buf), "%s", st.text.c_str());
                state->ReloadUserBufAndMoveToEnd();
            }
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        break;
    }
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({w, h});
    if (changed) config::markDirty();
    return changed;
}

bool row(const char* label, float& v, float lo, float hi, const char* fmt) {
    Setting s{label, label, SettingType::Float};
    s.f = v;
    s.fmin = lo;
    s.fmax = hi;
    s.format = fmt;
    if (!setting(s)) return false;
    v = s.f;
    return true;
}

bool row(const char* label, bool& v) {
    Setting s{label, label, SettingType::Bool};
    s.b = v;
    if (!setting(s)) return false;
    v = s.b;
    return true;
}

bool row(const char* label, ImVec4& c) {
    Setting s{label, label, SettingType::Color};
    s.color = c;
    if (!setting(s)) return false;
    c = s.color;
    return true;
}

}
