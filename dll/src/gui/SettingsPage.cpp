#include "GuiInternal.hpp"
#include "Gui.hpp"
#include "I18n.hpp"
#include "Theme.hpp"
#include "Widgets.hpp"
#include "core/Build.hpp"
#include "core/Config.hpp"
#include "hook/Dx.hpp"
#include "modules/Manager.hpp"
#include "modules/Tiers.hpp"
#include "modules/client/ClickGui.hpp"
#include "modules/client/ClientSettings.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "server/Rules.hpp"
#include "sig/Sigs.hpp"

#include <windows.h>

#include <functional>
#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace gui {

static void beginTab(const char* id) { beginScroll(id, {ImGui::GetContentRegionAvail().x, 0}); }

static bool sameColor(const ImVec4& a, const ImVec4& b) {
    return std::fabs(a.x - b.x) + std::fabs(a.y - b.y) + std::fabs(a.z - b.z) < 0.01f;
}

static void accentRow() {
    auto& t = theme::current();
    float s = ui::scale();
    auto* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float r = 11 * s, gap = 10 * s;
    auto& list = theme::accents();
    for (size_t i = 0; i < list.size(); i++) {
        auto& a = list[i];
        ImVec2 c = o + ImVec2(r + 4 * s + i * (2 * r + gap), r + 4 * s);
        ImGui::SetCursorScreenPos(c - ImVec2(r, r));
        ImGui::PushID(int(i));
        bool clicked = ImGui::InvisibleButton("accent", {2 * r, 2 * r});
        bool hov = ImGui::IsItemHovered();
        float& k = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("k"), 0.f);
        ImGui::PopID();
        bool on = sameColor(t.accent, a.accent);
        k = draw::approach(k, on ? 1.f : 0.f, 16.f * t.animSpeed);
        dl->AddCircleFilled(c, r * (hov && !on ? 1.f : 0.88f), theme::col(a.accent), 32);
        dl->AddCircleFilled(c, r * 0.32f * k, theme::col(a.accent2), 24);
        if (k > 0.01f) dl->AddCircle(c, r + 3 * s, theme::col(t.text, 0.85f * k), 32, 1.5f * s);
        if (hov) ImGui::SetTooltip("%s", i18n::tr(a.name));
        if (clicked) {
            t.accent = a.accent;
            t.accent2 = a.accent2;
            theme::applyStyle();
            config::markDirty();
        }
    }
    ImGui::SetCursorScreenPos(o + ImVec2(0, 2 * r + 8 * s));
}

static void drawAppearance() {
    auto& t = theme::current();
    float s = ui::scale();
    beginTab("themes");
    widgets::sectionTitle("Accent color");
    widgets::hint("Colors switches, sliders, buttons and modules that are on.");
    accentRow();

    widgets::sectionTitle("Presets");
    widgets::hint("Change the panels and background. Your accent color stays.");
    float avail = ImGui::GetContentRegionAvail().x;
    float gap = 8 * s;
    float h = 56 * s;
    int cols = std::max(1, int((avail + gap) / (150 * s + gap)));
    float w = (avail - gap * (cols - 1)) / cols;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    auto& presets = theme::presets();
    for (size_t i = 0; i < presets.size(); i++) {
        auto& p = presets[i];
        ImVec2 pos = origin + ImVec2((i % cols) * (w + gap), (i / cols) * (h + gap));
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID((int)i);
        bool clicked = ImGui::InvisibleButton("preset", {w, h});
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        auto* dl = ImGui::GetWindowDrawList();
        float r = p.rounding * 0.75f * s;
        dl->AddRectFilled(pos, pos + ImVec2(w, h), theme::col(p.bg), r);
        ImVec2 chip = pos + ImVec2(10 * s, 10 * s);
        float cw = w - 20 * s;
        dl->AddRectFilled(chip, chip + ImVec2(cw, 15 * s), theme::col(theme::mix(p.bg, t.accent, 0.6f)), 4 * s);
        dl->AddRectFilled(chip + ImVec2(cw - 24 * s, 3.5f * s), chip + ImVec2(cw - 8 * s, 11.5f * s), theme::col(theme::mix(p.bg, t.accent, 0.35f)), 4 * s);
        dl->AddCircleFilled(chip + ImVec2(cw - 12 * s, 7.5f * s), 2.6f * s, theme::col(t.accent2));
        dl->AddText(fonts::regular(), 13 * s, pos + ImVec2(10 * s, 32 * s), theme::col(p.text), p.name.c_str());
        bool active = t.name == p.name;
        ImVec4 edge = active ? t.accent : (hovered ? p.text : p.border);
        dl->AddRect(pos, pos + ImVec2(w, h), theme::col(edge, active ? 1.f : 0.6f), r, 0, active ? 1.5f * s : 1.f);
        if (clicked) {
            Theme next = p;
            next.accent = t.accent;
            next.accent2 = t.accent2;
            theme::use(next);
        }
    }
    int rows = int((presets.size() + cols - 1) / cols);
    ImGui::SetCursorScreenPos(origin + ImVec2(0, rows * (h + gap)));

    widgets::sectionTitle("Custom theme");
    bool changed = false;
    changed |= widgets::row("Background", t.bg);
    changed |= widgets::row("Surfaces", t.surface);
    changed |= widgets::row("Surfaces (hover)", t.surfaceHover);
    changed |= widgets::row("Accent", t.accent);
    changed |= widgets::row("Accent 2", t.accent2);
    changed |= widgets::row("Text", t.text);
    changed |= widgets::row("Dimmed text", t.textDim);
    changed |= widgets::row("Success", t.ok);
    changed |= widgets::row("Warning", t.warn);
    changed |= widgets::row("Off", t.off);
    if (t.border.w <= 0.f) t.border = t.surfaceHover;
    changed |= widgets::row("Border", t.border);
    changed |= widgets::row("Corner radius", t.rounding, 0.f, 24.f, "%.0f");
    changed |= widgets::row("Opacity", t.opacity, 0.5f, 1.f, "%.2f");
    changed |= widgets::row("Animation speed", t.animSpeed, 0.25f, 3.f, "%.2fx");
    if (changed) theme::applyStyle();

    ImGui::Dummy({0, 6 * s});
    if (widgets::button("Copy theme as code", {0, 0}, true)) ImGui::SetClipboardText(theme::exportCode().c_str());
    ImGui::SameLine();
    if (widgets::button("Load code from clipboard")) {
        const char* clip = ImGui::GetClipboardText();
        theme::importCode(clip ? clip : "");
    }
    ImGui::SameLine();
    // the first preset is what a fresh install starts with, colors, radius, opacity and speed included
    if (widgets::button("Reset to defaults") && !theme::presets().empty()) {
        theme::use(theme::presets().front());
        theme::applyStyle();
        config::markDirty();
    }
    endScroll("themes");
}

static void drawProfiles() {
    float s = ui::scale();
    auto& t = theme::current();
    beginTab("profiles");
    widgets::sectionTitle("Profile");
    widgets::hint("Each profile stores modules, settings, HUD positions and theme.");
    ImGui::Dummy({0, 4 * s});

    for (auto& name : config::profiles()) {
        ImGui::PushID(name.c_str());
        bool active = name == config::profile();
        ImGui::AlignTextToFramePadding();
        if (active) ImGui::TextColored(t.accent, i18n::tr("%s  (active)"), name.c_str());
        else ImGui::TextUnformatted(name.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.6f);
        if (!active) {
            if (widgets::button("Load")) config::switchProfile(name);
            ImGui::SameLine();
            if (widgets::button("Delete")) config::deleteProfile(name);
        }
        ImGui::PopID();
    }

    static char newName[32] = "";
    ImGui::Dummy({0, 8 * s});
    ImGui::SetNextItemWidth(220 * s);
    ImGui::InputTextWithHint("##new", i18n::tr("New profile …"), newName, sizeof(newName));
    ImGui::SameLine();
    if (widgets::button("Create", {0, 0}, true) && newName[0]) {
        config::switchProfile(newName);
        newName[0] = 0;
    }
    endScroll("profiles");
}

static void drawAbout() {
    auto& t = theme::current();
    beginTab("info");
    widgets::sectionTitle("Monchi");
    ImGui::Text(i18n::tr("Version %s"), build::version);
    ImGui::Text(i18n::tr("Renderer: %s"), dx::api() == dx::Api::Dx12 ? "DirectX 12" : dx::api() == dx::Api::Dx11 ? "DirectX 11" : "–");

    widgets::sectionTitle("Minecraft");
    auto st = sigs::stats();
    ImGui::Text(i18n::tr("Version: %s"), i18n::tr(st.gameVersion.c_str()));
    ImGui::Text(i18n::tr("Signatures: %d of %d found"), st.found, st.total);
    ImGui::TextColored(t.textDim, i18n::tr("Source: %s"), i18n::tr(st.source.c_str()));
    int unavailable = 0;
    for (auto& m : modules::all())
        if (!m->available() && !m->replaced()) unavailable++;
    if (unavailable) ImGui::TextColored(t.warn, i18n::tr("%d modules are waiting for signatures for this version."), unavailable);

    widgets::sectionTitle("Server");
    auto rs = rules::status();
    ImGui::Text(i18n::tr("Connected: %s"), rs.server.empty() ? "–" : rs.server.c_str());
    if (!rs.host.empty()) ImGui::TextColored(t.textDim, "%s", rs.host.c_str());
    ImGui::TextColored(t.textDim, i18n::tr("Rules: %s"), i18n::tr(rules::source().c_str()));

    endScroll("info");
}


static void settingRows(std::initializer_list<const char*> ids) {
    auto* cs = modules::get<ClientSettings>();
    if (!cs) return;
    for (auto id : ids)
        for (auto& st : cs->settings())
            if (st.id == id) widgets::setting(st);
}

// one button under a tab: the settings of that tab go back to what a fresh install has
static void resetRows(std::initializer_list<const char*> ids, const std::function<void()>& more = {}) {
    ImGui::Dummy({0, 8 * ui::scale()});
    if (!widgets::button("Reset to defaults")) return;
    if (auto* cs = modules::get<ClientSettings>())
        cs->resetSettings([&](const Setting& st) {
            for (auto id : ids)
                if (st.id == id) return true;
            return false;
        });
    if (more) more();
    config::markDirty();
}

static void drawGeneral() {
    float s = ui::scale();
    beginTab("general");
    widgets::sectionTitle("Language");
    widgets::hint("Auto follows your Windows language.");
    const char* names[] = {"Auto", "English", "Deutsch"};
    for (int i = 0; i < 3; i++) {
        if (i) ImGui::SameLine();
        if (widgets::button(names[i], {0, 0}, int(i18n::chosen()) == i)) i18n::choose(i18n::Lang(i));
    }
    ImGui::Dummy({0, 6 * s});

    widgets::sectionTitle("Menu");
    settingRows({"motion", "menuBlur", "notifications"});
    if (auto* menu = modules::get<ClickGui>()) widgets::setting(menu->keybind());
    widgets::hint("Ctrl+L: unload client · F1: hide HUD · ESC: back · right-click a key: clear");
    resetRows({"motion", "menuBlur", "notifications"}, [] {
        if (auto* menu = modules::get<ClickGui>()) menu->keybind().i = VK_RSHIFT;
    });
    endScroll("general");
}

static void drawChat() {
    float s = ui::scale();
    beginTab("chat");
    widgets::sectionTitle("Client tag");
    widgets::hint("Shown at your own name in Better Chat, in the Tab List and above your head. With Monchi Online on, other Monchi users see it too; a change shows for you at once and reaches them after one to two minutes.");
    settingRows({"tag", "tagText", "tagColor", "tagPos", "brackets", "tabTag"});
    auto* chat = modules::find("Better Chat");
    auto* tab = modules::find("Tab List");
    bool chatOk = chat && chat->available(), tabOk = tab && tab->available();
    if (!chatOk && !tabOk) widgets::hint("The tag needs game data for chat and the Tab List. It shows up by itself once the data for your version is there.");
    else if (!chatOk) widgets::hint("Better Chat needs game data for this version. Until then the tag only shows in the Tab List.");
    else if (!chat->userEnabled() && widgets::button("Turn on Better Chat", {0, 0}, true)) chat->setEnabled(true);
    ImGui::Dummy({0, 6 * s});
    widgets::sectionTitle("Watermark");
    settingRows({"invMark"});
    resetRows({"tag", "tagText", "tagColor", "tagPos", "brackets", "tabTag", "invMark"});
    endScroll("chat");
}

static void applyPreset(int kind) {
    static const char* minimal[] = {"FPS", "CPS", "Keystrokes"};
    for (auto& m : modules::all()) {
        if (m->alwaysOn() || m->category() == Category::Client) continue;
        bool on = false;
        if (kind == 0) on = std::any_of(std::begin(minimal), std::end(minimal), [&](const char* n) { return m->name() == n; });
        else if (kind == 1) on = modules::tierOf(m->name()) <= 1 && !m->risky();
        m->setEnabled(on);
    }
}

static void drawModuleDefaults() {
    float s = ui::scale();
    beginTab("moddefaults");
    widgets::sectionTitle("Presets");
    widgets::hint("Switch many modules at once. Modules that are blocked on your server or need game data stay as they are.");
    if (widgets::button("Minimal", {0, 0}, false)) applyPreset(0);
    ImGui::SameLine();
    if (widgets::button("PvP essentials", {0, 0}, true)) applyPreset(1);
    ImGui::SameLine();
    if (widgets::button("Everything off", {0, 0}, false)) applyPreset(2);
    ImGui::Dummy({0, 6 * s});

    widgets::sectionTitle("HUD");
    settingRows({"hudScale"});
    if (widgets::button("Reset all HUD positions", {0, 0}, false))
        for (auto& m : modules::all())
            if (m->isHud()) m->resetSettings([](const Setting& st) { return st.id == "x" || st.id == "y" || st.id == "scale" || st.id == "placed"; });
    ImGui::Dummy({0, 6 * s});

    endScroll("moddefaults");
}

int& settingsTab() {
    static int tab = 0;
    return tab;
}

void drawSettingsPage(ImVec2 origin, ImVec2 size) {
    auto& t = theme::current();
    float s = ui::scale();
    int& tab = settingsTab();
    static const char* names[] = {"General", "Chat & watermark", "Appearance", "Modules", "Profiles", "About"};
    auto* dl = ImGui::GetWindowDrawList();
    float fs = 12.5f * s;
    float h = 24 * s;
    float limit = origin.x + size.x - 10 * s;
    ImVec2 at = origin;
    for (int i = 0; i < 6; i++) {
        const char* label = i18n::tr(names[i]);
        ImVec2 ts = fonts::regular()->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
        ImVec2 sz{ts.x + 20 * s, h};
        if (at.x > origin.x && at.x + sz.x > limit) at = {origin.x, at.y + h + 6 * s};
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(at);
        bool clicked = ImGui::InvisibleButton("tab", sz);
        bool hov = ImGui::IsItemHovered();
        float& a = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("a"), tab == i ? 1.f : 0.f);
        a = draw::approach(a, tab == i ? 1.f : 0.f, 16.f * t.animSpeed);
        ImVec4 fill = theme::mix(t.surface, t.accent, a);
        if (hov && tab != i) fill = theme::mix(fill, t.text, 0.06f);
        dl->AddRectFilled(at, at + sz, theme::col(fill, 0.95f), 5 * s);
        dl->AddText(fonts::regular(), fs, at + (sz - ts) * 0.5f, theme::col(theme::mix(t.textDim, ImVec4(1, 1, 1, 1), a)), label);
        if (clicked) tab = i;
        ImGui::PopID();
        at.x += sz.x + 6 * s;
    }
    float top = at.y + h + 12 * s;
    ImGui::SetCursorScreenPos({origin.x, top});
    ImGui::BeginChild("settingscontent", {size.x, origin.y + size.y - top}, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    switch (tab) {
    case 0: drawGeneral(); break;
    case 1: drawChat(); break;
    case 2: drawAppearance(); break;
    case 3: drawModuleDefaults(); break;
    case 4: drawProfiles(); break;
    default: drawAbout(); break;
    }
    ImGui::EndChild();
}

}
