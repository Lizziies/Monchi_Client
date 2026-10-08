#include "Catalog.hpp"
#include "GuiInternal.hpp"
#include "Gui.hpp"
#include "Preview.hpp"
#include "I18n.hpp"
#include "Profile.hpp"
#include "Theme.hpp"
#include "Widgets.hpp"
#include "modules/HudModule.hpp"
#include "modules/Manager.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

namespace gui {

static const Entry* pickedEntry = nullptr;
static Module* pickedModule = nullptr;
static Module* focusPart = nullptr;
static bool scrollList = false;
static float profileScroll = -1.f;
static bool foldingInView = false;
static bool anyFolding = false;
static Module* lastHeaderInView = nullptr;
static size_t probeAt = 4;
static bool scrollPart = false;
static std::map<const void*, bool> partOpen;
static std::map<const void*, float> partT;
static std::map<const void*, float> partH;
static std::map<const void*, float> partShown;
static double shownAt = 0.0;
static bool wasOpen = false;

static const ImVec4 gold{1.f, 0.82f, 0.4f, 1.f};


static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string upper(std::string s) {
    for (auto& c : s)
        if ((unsigned char)c < 128) c = (char)std::toupper((unsigned char)c);
    return s;
}

static bool matches(const Module& m) {
    if (favoritesOnly() && !m.favorite()) return false;
    if (!searchText()[0]) return true;
    std::string q = lower(searchText());
    if (lower(i18n::tr(m.name().c_str())).find(q) != std::string::npos || lower(m.name()).find(q) != std::string::npos) return true;
    if (lower(i18n::tr(m.description().c_str())).find(q) != std::string::npos) return true;
    if (const Entry* e = entryOf(m); e && e->group && lower(i18n::tr(e->name.c_str())).find(q) != std::string::npos) return true;
    for (auto& tag : m.tags())
        if (lower(tag).find(q) != std::string::npos) return true;
    return false;
}

static void chevron(ImDrawList* dl, ImVec2 c, float open, ImU32 col) {
    float s = ui::scale();
    float a = open * 1.5708f;
    auto rot = [&](float x, float y) { return ImVec2(c.x + (x * std::cos(a) - y * std::sin(a)) * s, c.y + (x * std::sin(a) + y * std::cos(a)) * s); };
    ImVec2 pts[3] = {rot(-1.5f, -3.5f), rot(2.f, 0.f), rot(-1.5f, 3.5f)};
    dl->AddPolyline(pts, 3, col, 0, 1.4f * s);
}

static float textY(float cy, float size) { return cy - size * 0.53f; }

struct Row {
    std::string name;
    std::string extra;
    const char* lockText = "no data";
    Module* fav = nullptr;
    float fold = -1.f;
    bool on = false;
    bool lock = false;
    bool warn = false;
    bool canToggle = true;
    bool selected = false;
    bool hasSwitch = true;
    bool scrollHere = false;
};

enum class Hit { None, Row, Switch };

static Hit listRow(const void* id, const Row& row, float appear = 1.f) {
    auto& t = theme::current();
    float s = ui::scale();
    profile::row();
    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 29 * s;
    ImGui::SetNextItemAllowOverlap();
    bool clicked = ImGui::InvisibleButton("row", {w, h});
    bool hov = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
    if (row.scrollHere) ImGui::SetScrollHereY(0.3f);

    auto* store = ImGui::GetStateStorage();
    float speed = 16.f * t.animSpeed;
    float& hv = *store->GetFloatRef(ImGui::GetID("hv"), 0.f);
    float& lit = *store->GetFloatRef(ImGui::GetID("lit"), row.on ? 1.f : 0.f);
    float& sel = *store->GetFloatRef(ImGui::GetID("sel"), row.selected ? 1.f : 0.f);
    hv = draw::approach(hv, hov ? 1.f : 0.f, speed * 1.3f);
    lit = draw::approach(lit, row.on ? 1.f : 0.f, speed);
    sel = draw::approach(sel, row.selected ? 1.f : 0.f, speed);

    float base = theme::fade();
    theme::setFade(base * appear);
    auto* dl = ImGui::GetWindowDrawList();
    ImVec2 a = p + ImVec2((1.f - appear) * 10 * s, 0), b = a + ImVec2(w, h);
    float r = t.rounding * 0.75f * s;
    ImVec4 fill = theme::mix(t.surface, theme::mix(t.bg, t.accent, 0.6f), lit);
    fill = theme::mix(fill, t.text, 0.05f * hv + 0.07f * sel);
    if (ImGui::IsItemActive()) fill = theme::mix(fill, t.bg, 0.2f);
    dl->AddRectFilled(a, b, theme::col(fill, 0.92f), r);
    if (sel > 0.01f) dl->AddRect(a, b, theme::col(theme::border(), sel), r, 0, 1.f);

    float dim = row.lock ? 0.45f : 1.f;
    float cy = a.y + h * 0.5f;
    float tx = a.x + 10 * s + 2 * s * hv * (row.lock ? 0.f : 1.f);
    if (row.fold >= 0.f) {
        chevron(dl, {a.x + 12 * s, cy}, row.fold, theme::col(theme::mix(t.textDim, t.text, 0.6f * hv), dim));
        tx = a.x + 23 * s + 2 * s * hv;
    }
    ImVec2 sw = widgets::switchSize();
    bool showSwitch = row.hasSwitch && !row.lock;
    float x = b.x - 10 * s - (showSwitch ? sw.x + 8 * s : 0.f);
    float small = 11.5f * s;

    if (row.lock) {
        const char* text = i18n::tr(row.lockText);
        x -= fonts::regular()->CalcTextSizeA(small, FLT_MAX, 0.f, text).x;
        dl->AddText(fonts::regular(), small, {x, textY(cy, small)}, theme::col(t.textDim, 0.7f), text);
        x -= 8 * s;
    } else if (!row.hasSwitch) {
        chevron(dl, {b.x - 13 * s, cy}, 0.f, theme::col(t.textDim, 0.7f));
    }
    if (row.fav) {
        ImVec2 sc{x - 7 * s, cy};
        ImGui::SetCursorScreenPos(sc - ImVec2(9 * s, 9 * s));
        bool starClicked = ImGui::InvisibleButton("fav", {18 * s, 18 * s});
        bool starHov = ImGui::IsItemHovered();
        if (row.fav->favorite()) star(dl, sc, 5.5f * s, theme::col(gold, dim), true);
        else if (hov || starHov) star(dl, sc, 5.5f * s, theme::col(starHov ? t.text : t.textDim, 0.7f), false);
        if (starClicked) row.fav->setFavorite(!row.fav->favorite());
        x -= 20 * s;
    }
    if (row.warn) {
        dl->AddCircleFilled({x - 3 * s, cy}, 2.6f * s, theme::col(t.warn, dim));
        x -= 12 * s;
    }

    float size = 13.5f * s;
    std::string label = fitText(fonts::regular(), size, row.name, x - tx - 4 * s);
    dl->AddText(fonts::regular(), size, {tx, textY(cy, size)}, theme::col(t.text, dim), label.c_str());
    if (!row.extra.empty()) {
        float after = tx + fonts::regular()->CalcTextSizeA(size, FLT_MAX, 0.f, label.c_str()).x + 6 * s;
        if (after + fonts::regular()->CalcTextSizeA(small, FLT_MAX, 0.f, row.extra.c_str()).x < x)
            dl->AddText(fonts::regular(), small, {after, textY(cy, small)}, theme::col(t.textDim, 0.85f * dim), row.extra.c_str());
    }

    Hit hit = clicked ? Hit::Row : Hit::None;
    if (showSwitch) {
        ImVec2 sp{b.x - 10 * s - sw.x, cy - sw.y * 0.5f};
        ImGui::SetCursorScreenPos({sp.x - 8 * s, a.y});
        if (ImGui::InvisibleButton("switch", {sw.x + 16 * s, h}) && row.canToggle) hit = Hit::Switch;
        float& knob = *store->GetFloatRef(ImGui::GetID("knob"), row.on ? 1.f : 0.f);
        knob = draw::approach(knob, row.on ? 1.f : 0.f, 18.f * t.animSpeed);
        widgets::drawSwitch(dl, sp, knob, row.canToggle ? 1.f : 0.45f);
    }

    theme::setFade(base);
    ImGui::SetCursorScreenPos(p + ImVec2(0, h + 3 * s));
    ImGui::PopID();
    return hit;
}

static void sectionLabel(const char* text) {
    auto& t = theme::current();
    float s = ui::scale();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(fonts::bold(), 11 * s, p + ImVec2(4 * s, 9 * s), theme::col(t.textDim, 0.75f), upper(text).c_str());
    ImGui::SetCursorScreenPos(p + ImVec2(0, 27 * s));
}

static Row moduleRow(Module& m) {
    Row row;
    bool lock = locked(m);
    row.name = i18n::tr(m.name() == "Auto GG" ? "Auto GG (Beta)" : m.name().c_str());
    row.on = (m.userEnabled() && !lock) || m.alwaysOn();
    row.lock = lock;
    row.lockText = m.rule() == RuleLevel::Block ? "blocked" : "no data";
    row.warn = m.risky() || m.rule() == RuleLevel::Warn;
    row.canToggle = !lock && !m.alwaysOn();
    row.hasSwitch = !m.alwaysOn();
    return row;
}

static Row entryRow(const Entry& e) {
    if (!e.group) return moduleRow(*e.members.front());
    Row row;
    row.name = i18n::tr(e.name.c_str());
    row.on = e.on();
    row.lock = e.usable() == 0;
    row.warn = e.risky();
    row.canToggle = !row.lock;
    if (!row.lock) row.extra = i18n::fmt("{}/{}", e.enabled(), e.members.size());
    if (std::any_of(e.members.begin(), e.members.end(), [](Module* m) { return m->rule() == RuleLevel::Block; })) row.lockText = "blocked";
    return row;
}

static Row pinned(const char* name, bool selected) {
    Row row;
    row.name = i18n::tr(name);
    row.hasSwitch = false;
    row.selected = selected;
    return row;
}

static void pickEntry(const Entry& e) {
    focusPart = nullptr;
    pickedEntry = e.group ? &e : nullptr;
    pickedModule = e.group ? nullptr : e.members.front();
    go(Page::Modules);
}

static void reveal(Module* m) {
    const Entry* e = entryOf(*m);
    focusPart = nullptr;
    if (e && e->group) {
        pickedEntry = e;
        pickedModule = nullptr;
        partOpen[m] = true;
        focusPart = m;
        scrollPart = true;
    } else {
        pickedEntry = nullptr;
        pickedModule = m;
    }
    go(Page::Modules);
}

static Page view() {
    Page p = page();
    return p == Page::Modules && !pickedEntry && !pickedModule ? Page::Settings : p;
}

static bool isPicked(Module* m) { return page() == Page::Modules && (pickedModule == m || (focusPart == m && pickedEntry == entryOf(*m))); }

static float appearAt(int index) {
    if (!draw::motion()) return 1.f;
    float t = float(ui::time() - shownAt) - std::min(index, 30) * 0.012f;
    return draw::easeOutCubic(std::clamp(t / 0.2f, 0.f, 1.f));
}

static void moduleItem(Module& m, std::string extra, int index) {
    Row row = moduleRow(m);
    row.extra = std::move(extra);
    row.selected = isPicked(&m);
    row.scrollHere = scrollList && row.selected;
    Hit hit = listRow(&m, row, appearAt(index));
    if (row.scrollHere) scrollList = false;
    if (hit == Hit::Switch) m.setEnabled(!m.userEnabled());
    else if (hit == Hit::Row) reveal(&m);
}

void drawModulesPage(ImVec2 origin, ImVec2 size) {
    float s = ui::scale();
    if (open() && !wasOpen) shownAt = ui::time();
    wasOpen = open();
    if (Module*& want = selectedModule()) {
        reveal(want);
        scrollList = true;
        want = nullptr;
    }

    static std::string lastQuery;
    std::string query = std::string(searchText()) + (favoritesOnly() ? "*" : "");
    bool jumpTop = query != lastQuery;
    lastQuery = query;

    ImGui::SetCursorScreenPos(origin);
    beginScroll("list", size, jumpTop);
    int index = 0;
    Page pg = page();
    if (searchText()[0] || favoritesOnly()) {
        bool any = false;
        for (auto& e : catalog())
            for (auto* m : e.members) {
                if (!matches(*m)) continue;
                any = true;
                moduleItem(*m, e.group ? std::string(i18n::tr(e.name.c_str())) : std::string(), index++);
            }
        if (!any) widgets::hint(favoritesOnly() && !searchText()[0] ? "No favorites yet. Click the star next to a module." : "Nothing found.");
    } else {
        if (listRow("global", pinned("Global Settings", view() == Page::Settings), appearAt(index++)) == Hit::Row) go(Page::Settings);
        if (listRow("cosmetics", pinned("Cosmetics (Beta)", pg == Page::Cosmetics), appearAt(index++)) == Hit::Row) go(Page::Cosmetics);
        if (listRow("hud", pinned("Edit HUD", false), appearAt(index++)) == Hit::Row) setEditingHud(true);

        bool favs = std::any_of(modules::all().begin(), modules::all().end(),
                                [](auto& m) { return m->favorite() && !m->replaced() && m->category() != Category::Client; });
        if (favs) {
            sectionLabel(i18n::tr("Favorites"));
            for (auto& m : modules::all())
                if (m->favorite() && !m->replaced() && m->category() != Category::Client) moduleItem(*m, "", index++);
        }
        int section = -1;
        for (auto& e : catalog()) {
            if (int(e.section) != section) {
                section = int(e.section);
                sectionLabel(sectionName(e.section));
            }
            Row row = entryRow(e);
            row.selected = pg == Page::Modules && (pickedEntry == &e || (!e.group && pickedModule == e.members.front()));
            row.scrollHere = scrollList && row.selected;
            Hit hit = listRow(&e, row, appearAt(index++));
            if (row.scrollHere) scrollList = false;
            if (hit == Hit::Switch) e.toggle();
            else if (hit == Hit::Row) pickEntry(e);
        }
    }
    ImGui::Dummy({0, 6 * s});
    endScroll("list");
}

float detailsWidth() {
    float s = ui::scale();
    switch (view()) {
    case Page::Cosmetics: return 900 * s;
    case Page::Settings: return 720 * s;
    default: return 560 * s;
    }
}

static void note(const std::string& text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushFont(fonts::regular(), 12.5f);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

static void groupLabel(const char* text) {
    auto& t = theme::current();
    float s = ui::scale();
    ImGui::Dummy({0, 4 * s});
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(fonts::bold(), 11 * s, p + ImVec2(2 * s, 0), theme::col(t.textDim, 0.8f), upper(i18n::tr(text)).c_str());
    ImGui::Dummy({0, 13 * s});
}

static void settingList(Module& m) {
    auto& t = theme::current();
    float s = ui::scale();
    if (!m.ruleNote().empty()) note(m.ruleNote(), t.warn);
    if (locked(m) && m.rule() != RuleLevel::Block)
        note(i18n::tr("This one needs game data for your Minecraft version. It turns on by itself once the data is there."), t.textDim);
    if (!m.alwaysOn()) {
        widgets::setting(m.keybind());
        widgets::setting(m.hold());
    }

    auto core = [&](const Setting& st) { return &st == &m.keybind() || &st == &m.hold(); };
    auto isStyle = [](const Setting& st) { return st.style && st.type != SettingType::Color; };
    auto isColor = [](const Setting& st) { return st.type == SettingType::Color; };
    auto isGeneral = [&](const Setting& st) { return !isColor(st) && !isStyle(st); };
    auto list = [&](auto&& belongs, const char* title) {
        bool any = false;
        for (auto& st : m.settings())
            if (!core(st) && belongs(st) && st.shown()) any = true;
        if (!any) return;
        if (title) groupLabel(title);
        for (auto& st : m.settings())
            if (!core(st) && belongs(st)) widgets::setting(st);
    };
    preview::draw(m);
    m.drawSettingsTop();
    list(isGeneral, nullptr);
    m.drawSettings();
    list(isStyle, "Style");
    list(isColor, "Colors");

    ImGui::Dummy({0, 4 * s});
    if (widgets::button("Reset all")) m.resetSettings([](const Setting& st) { return !st.hidden && st.id != "x" && st.id != "y" && st.id != "key"; });
    if (!m.isHud()) return;
    ImGui::SameLine();
    if (widgets::button("Reset position")) m.resetSettings([](const Setting& st) { return st.id == "x" || st.id == "y" || st.id == "scale" || st.id == "placed"; });
    ImGui::SameLine();
    if (widgets::button("Edit HUD", {0, 0}, true)) setEditingHud(true);
}

// The body under a part slides open. Nothing is rebuilt and no child window is involved: the body is laid out
// at its natural size in the list itself, a clip rect cuts it at the animated height, and the cursor moves on by
// that height. Child windows snap to whole pixels and rebuild their layout; this keeps every row at its exact
// float position. The measured height is eased too, so settings that appear or disappear inside an open part do
// not make the list jump. The content fades in over the second half of the motion and out over the first half
// of closing, so it is never squeezed visibly.
template <class F>
static void expander(const void* id, float a, F&& body) {
    if (a < 0.002f) return;
    float s = ui::scale();
    auto it = partH.try_emplace(id, 0.f).first;
    auto shown = partShown.try_emplace(id, 0.f).first;
    if (shown->second <= 0.f || it->second <= 0.f) shown->second = it->second;
    else shown->second = draw::approach(shown->second, it->second, 20.f * theme::current().animSpeed);
    float h = std::max(1.f, shown->second * draw::easeInOutSine(a));
    if (profile::recording() && a > 0.f && a < 1.f) profile::height(h);

    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 reach = win->DC.CursorMaxPos;
    float width = ImGui::GetContentRegionAvail().x;
    float base = theme::fade();
    float k = draw::easeOutCubic(std::clamp((a - 0.3f) / 0.6f, 0.f, 1.f));
    float lift = (1.f - k) * 6 * s;

    ImGui::PushID(id);
    ImGui::PushClipRect(start, start + ImVec2(width, h), true);
    theme::setFade(base * k);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * k);
    ImGui::SetCursorScreenPos(start + ImVec2(10 * s, -lift));
    ImGui::Indent(10 * s);
    ImGui::Dummy({0, 1 * s});
    body();
    ImGui::Dummy({0, 6 * s});
    ImGui::Unindent(10 * s);
    ImGui::PopStyleVar();
    theme::setFade(base);
    it->second = ImGui::GetCursorScreenPos().y - start.y + lift;
    ImGui::PopClipRect();
    ImGui::PopID();

    win->DC.CursorMaxPos = reach;
    ImGui::SetCursorScreenPos(start + ImVec2(0, h));
    ImGui::Dummy({0, 0});
}

static void part(Module& m) {
    auto& t = theme::current();
    bool& isOpen = partOpen[&m];
    float& a = partT[&m];
    float dir = isOpen ? 1.f : -1.f;
    // a tall body gets more time, so no frame moves the list by more than a few dozen pixels
    float full = partShown.count(&m) ? partShown[&m] : 0.f;
    float duration = std::clamp(0.2f + full * 0.0004f, 0.2f, 0.8f) * (isOpen ? 1.f : 0.8f);
    a = draw::motion() ? std::clamp(a + dir * ui::dt() * t.animSpeed / duration, 0.f, 1.f) : (isOpen ? 1.f : 0.f);
    {
        auto* win = ImGui::GetCurrentWindow();
        float y = ImGui::GetCursorScreenPos().y;
        bool inView = y > win->InnerClipRect.Min.y && y < win->InnerClipRect.Max.y;
        if (!isOpen && a > 0.f) {
            foldingInView |= inView;
            anyFolding = true;
        }
        if (inView && profile::recording()) lastHeaderInView = &m;
    }
    Row row = moduleRow(m);
    row.fold = draw::easeInOutSine(a);
    row.fav = &m;
    row.scrollHere = scrollPart && focusPart == &m;
    if (row.scrollHere) scrollPart = false;
    Hit hit = listRow(&m, row);
    if (hit == Hit::Switch) m.setEnabled(!m.userEnabled());
    else if (hit == Hit::Row) isOpen = !isOpen;
    expander(&m, a, [&] {
        note(i18n::tr(m.description().c_str()), t.textDim);
        ImGui::Dummy({0, 2 * ui::scale()});
        settingList(m);
    });
}

static bool statePill(ImVec2 topRight, bool on, bool lock, bool canToggle, float& width) {
    auto& t = theme::current();
    float s = ui::scale();
    const char* label = i18n::tr(lock ? "Unavailable" : on ? "Enabled" : "Disabled");
    float fs = 12.5f * s;
    ImVec2 ts = fonts::regular()->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
    ImVec2 size{ts.x + 26 * s, 24 * s};
    ImVec2 p{topRight.x - size.x, topRight.y};
    width = size.x;
    ImGui::SetCursorScreenPos(p);
    bool clicked = ImGui::InvisibleButton("state", size) && canToggle;
    bool hov = ImGui::IsItemHovered() && canToggle;
    float& a = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("stateAnim"), on ? 1.f : 0.f);
    a = draw::approach(a, on && !lock ? 1.f : 0.f, 16.f * t.animSpeed);
    ImVec4 fill = theme::mix(t.surface, t.accent, a);
    if (hov) fill = theme::mix(fill, t.text, 0.08f);
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + size, theme::col(fill, lock ? 0.5f : 1.f), 5 * s);
    dl->AddText(fonts::regular(), fs, p + (size - ts) * 0.5f, theme::col(theme::mix(t.textDim, ImVec4(1, 1, 1, 1), a)), label);
    return clicked;
}

static void favButton(Module& m, ImVec2 p, float size) {
    auto& t = theme::current();
    ImGui::SetCursorScreenPos(p);
    bool clicked = ImGui::InvisibleButton("fav", {size, size});
    bool hov = ImGui::IsItemHovered();
    auto* dl = ImGui::GetWindowDrawList();
    if (hov) dl->AddRectFilled(p, p + ImVec2(size, size), theme::col(t.surface), 5 * ui::scale());
    star(dl, p + ImVec2(size, size) * 0.5f, 6.5f * ui::scale(), theme::col(m.favorite() ? gold : (hov ? t.text : t.textDim)), m.favorite());
    if (hov) ImGui::SetTooltip("%s", i18n::tr("Favorite"));
    if (clicked) m.setFavorite(!m.favorite());
}

void drawDetails(ImVec2 origin, ImVec2 size) {
    auto& t = theme::current();
    float s = ui::scale();
    float pad = 18 * s;
    Page v = view();

    static Page lastView = Page::Hub;
    static const void* lastPick = nullptr;
    static float swap = 1.f;
    const void* pick = pickedEntry ? static_cast<const void*>(pickedEntry) : static_cast<const void*>(pickedModule);
    bool fresh = v != lastView || (v == Page::Modules && pick != lastPick);
    if (fresh) swap = 0.f;
    lastView = v;
    lastPick = pick;
    swap = draw::motion() ? draw::approach(swap, 1.f, 14.f * t.animSpeed) : 1.f;
    float e = draw::easeOutCubic(swap);

    float base = theme::fade();
    theme::setFade(base * e);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * e);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8 * s, 4 * s});

    ImVec2 o = origin + ImVec2(pad + (1.f - e) * 10 * s, pad);
    float w = size.x - pad * 2;
    std::string title, sub;
    const Entry* group = v == Page::Modules ? pickedEntry : nullptr;
    Module* single = v == Page::Modules && !group ? pickedModule : nullptr;
    if (v == Page::Settings) {
        title = i18n::tr("Global Settings");
        sub = i18n::tr("Change global settings for the client.");
    } else if (v == Page::Cosmetics) {
        title = i18n::tr("Cosmetics (Beta)");
        sub = i18n::tr("Wings, capes and more. Only you and other Monchi players see them.");
    } else if (group) {
        title = i18n::tr(group->name.c_str());
        sub = i18n::tr(group->blurb.c_str());
    } else if (single) {
        title = i18n::tr(single->name() == "Auto GG" ? "Auto GG (Beta)" : single->name().c_str());
        sub = i18n::tr(single->description().c_str());
    }

    auto* dl = ImGui::GetWindowDrawList();
    float right = o.x + w;
    if (group || (single && !single->alwaysOn())) {
        bool on = single ? (single->userEnabled() && !locked(*single)) || single->alwaysOn() : group->on();
        bool lock = single ? locked(*single) : group->usable() == 0;
        bool canToggle = !lock && !(single && single->alwaysOn());
        float pw = 0.f;
        if (statePill({right, o.y + 3 * s}, on, lock, canToggle, pw)) {
            if (single) single->setEnabled(!single->userEnabled());
            else group->toggle();
        }
        right -= pw + 6 * s;
        if (single) {
            favButton(*single, {right - 24 * s, o.y + 3 * s}, 24 * s);
            right -= 30 * s;
        }
    }
    float ts = 24 * s;
    std::string shown = fitText(fonts::regular(), ts, title, right - o.x - 8 * s);
    dl->AddText(fonts::regular(), ts, o, theme::col(t.text), shown.c_str());
    float y = o.y + 34 * s;
    if (!sub.empty()) {
        float fs = 12.5f * s;
        dl->AddText(fonts::regular(), fs, {o.x, y}, theme::col(t.textDim), sub.c_str(), nullptr, w);
        y += fonts::regular()->CalcTextSizeA(fs, FLT_MAX, w, sub.c_str()).y;
    }
    y += 14 * s;
    ImVec2 body{o.x, y};
    ImVec2 area{w + 10 * s, origin.y + size.y - y - pad * 0.6f};

    if (v == Page::Settings) {
        drawSettingsPage(body, area);
    } else if (v == Page::Cosmetics) {
        drawCosmeticsPage(body, {w, area.y});
    } else {
        ImGui::SetCursorScreenPos(body);
        beginScroll("detail", area, fresh && !scrollPart);
        if (group) {
            // While a part folds in with its header on screen, the list keeps its height and gives the room back
            // slowly afterwards, so the header the user clicked stays where it is. The scroll position is clamped
            // to the content, so without this the rows would be dragged along as the content gets shorter.
            static float lastEnd = 0.f, slack = 0.f;
            static const Entry* slackFor = nullptr;
            if (slackFor != group || fresh) {
                slackFor = group;
                lastEnd = slack = 0.f;
            }
            if (profileScroll >= 0.f) {
                ImGui::SetScrollY(profileScroll);
                profileScroll = -1.f;
            }
            float top = ImGui::GetCursorPosY();
            size_t at = 0;
            for (auto* m : group->members) {
                if (at++ == probeAt && profile::recording()) profile::probe(ImGui::GetCursorScreenPos().y, ImGui::GetScrollY());
                part(*m);
            }
            float end = ImGui::GetCursorPosY() - top;
            slack = std::max(slack, 0.f) + (foldingInView ? std::max(0.f, lastEnd - end) : 0.f);
            foldingInView = false;
            if (end > lastEnd) slack = std::max(0.f, slack - (end - lastEnd));
            lastEnd = end;
            if (!anyFolding) slack = draw::approach(slack, 0.f, 4.f * theme::current().animSpeed);
            anyFolding = false;
            if (slack > 0.5f) ImGui::Dummy({0, slack});
        } else if (single) {
            settingList(*single);
        }
        ImGui::Dummy({0, 8 * s});
        endScroll("detail");
    }

    ImGui::PopStyleVar(2);
    theme::setFade(base);
}

// MONCHI_PROFILE: opens the menu, picks a group and opens and closes its parts one after the other, so frame
// times can be compared run to run without clicking.
void profileDrive() {
    if (!profile::recording()) return;
    static double start = -1.0;
    static int step = 0;
    static const Entry* group = nullptr;
    if (start < 0.0) start = ui::time();
    double t = ui::time() - start;
    if (t < 1.0) return;
    if (step == 0) {
        setOpen(true);
        profile::phase("open");
        step = 1;
    }
    if (t < 2.5) return;
    if (!group) {
        for (auto& e : catalog())
            if (e.group && e.members.size() >= 5) {
                group = &e;
                break;
            }
        if (group) pickEntry(*group);
        profile::phase("group");
        return;
    }
    int k = int((t - 4.0) / 1.4);
    if (t >= 4.0 && k >= step - 1 && k < 14) {
        step = k + 2;
        Module* m = group->members[size_t(k / 2) % group->members.size()];
        partOpen[m] = k % 2 == 0;
        profile::phase(k % 2 == 0 ? "expand" : "collapse");
    }
    if (t >= 24.0 && step < 90) {
        step = 90;
        for (auto* m : group->members) partOpen[m] = true;
        probeAt = 2;
        profile::phase("all-open");
    }
    if (t >= 27.0 && step < 91) {
        step = 91;
        profileScroll = 1e6f;
        profile::phase("scrolled");
    }
    if (t >= 29.0 && step < 92) {
        step = 92;
        for (size_t i = 0; i < group->members.size(); i++)
            if (group->members[i] == lastHeaderInView) {
                partOpen[lastHeaderInView] = false;
                probeAt = i;
            }
        profile::phase("collapse-bottom");
    }
    if (t >= 33.0 && step < 100) {
        step = 100;
        for (auto& e : catalog())
            if (!e.group && !locked(*e.members.front())) {
                pickEntry(e);
                profile::phase("single");
                break;
            }
    }
}

}
