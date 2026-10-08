#include "Gui.hpp"
#include "core/Guard.hpp"
#include "GuiInternal.hpp"
#include "I18n.hpp"
#include "HudEditor.hpp"
#include "Preview.hpp"
#include "Theme.hpp"
#include "Profile.hpp"
#include "Widgets.hpp"
#include "core/Config.hpp"
#include "hook/Input.hpp"
#include "modules/Manager.hpp"
#include "modules/client/ClientSettings.hpp"
#include "modules/post/PostFx.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "server/Rules.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

namespace gui {

static std::atomic<bool> isOpen{false};
static std::atomic<bool> hudEdit{false};
static std::atomic<bool> keyboardClaim{false};
static bool keyboardClaimNext = false;
static float openAnim = 0.f;
static float openT = 0.f;
static Page current = Page::Settings;
static Module* selected = nullptr;
static char search[64] = "";
static bool onlyFavorites = false;

char* searchText() { return search; }
bool& favoritesOnly() { return onlyFavorites; }
Page page() { return current; }
Module*& selectedModule() { return selected; }

void go(Page p) {
    if (p == Page::Hub) p = Page::Modules;
    if (p == current) return;
    current = p;
    if (p == Page::Cosmetics) reloadCosmetics();
}

void beginFrame() {
    keyboardClaim = keyboardClaimNext;
    keyboardClaimNext = false;
}

bool open() { return isOpen; }

// setOpen can run on the window thread; the active field is let go on the render thread
static std::atomic<bool> dropFocus{false};

void setOpen(bool on) {
    if (on && !isOpen) {
        input::releaseHeld();
        i18n::load();
    }
    isOpen = on;
    if (on) {
        hudEdit = false;
    } else {
        // this can be the window thread; the snapshot of every module's settings is taken on the render thread
        config::requestSave();
        dropFocus = true;
    }
}

void toggle() {
    if (hudEdit) {
        hudEdit = false;
        setOpen(true);
        return;
    }
    if (isOpen) {
        setOpen(false);
        return;
    }
    search[0] = 0;
    setOpen(true);
}

void showModule(Module* m) {
    current = Page::Modules;
    setOpen(true);
    selected = m;
}

bool editingHud() { return hudEdit; }

void setEditingHud(bool on) {
    if (on && !hudEdit) input::releaseHeld();
    hudEdit = on;
    if (on) isOpen = false;
}

bool wantsInput() { return isOpen || hudEdit || keyboardClaim; }
bool wantsCursor() { return isOpen || hudEdit; }
// a text field left active when the menu closed must not keep eating the game's keys
bool capturesKeyboard() { return isOpen || widgets::capturingKey() || keyboardClaim || (hudEdit && ImGui::GetIO().WantTextInput); }
void claimKeyboard() { keyboardClaimNext = true; }

// Every panel floats on its own: blurred game behind it, a translucent fill and a thin outline.
static void beginPanel(const char* id, ImVec2 pos, ImVec2 size, float fade) {
    auto& t = theme::current();
    float s = ui::scale();
    float r = t.rounding * s;
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    theme::setFade(fade);
    auto* dl = ImGui::GetWindowDrawList();
    auto* cs = modules::get<ClientSettings>();
    float blur = cs ? cs->menuBlur() : 0.f;
    if (blur > 0.01f) post::blur(dl, pos, pos + size, r, blur * 16.f * s * fade, {0, 0, 0, 0});
    dl->AddRectFilled(pos, pos + size, theme::col(t.bg, t.opacity * (1.f - 0.15f * blur)), r);
    dl->AddRect(pos, pos + size, theme::col(theme::border(), 0.85f), r, 0, 1.f);
    ImGui::PushFont(fonts::regular(), 14.f);
}

static void endPanel() {
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
    theme::setFade(1.f);
}

static void searchBar(ImVec2 at, ImVec2 size) {
    auto& t = theme::current();
    float s = ui::scale();
    float sw = size.y;
    ImGui::SetCursorScreenPos(at + ImVec2(4 * s, 0));
    ImGui::SetNextItemWidth(size.x - sw - 6 * s);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, t.textDim);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8 * s, (size.y - ImGui::GetFontSize()) * 0.5f});
    // Typing anywhere in the menu goes into the search. The field only takes characters once it is active,
    // so the ones that activated it are queued again for the next frame.
    static std::vector<ImWchar> pending;
    static int pendingFrame = 0;
    auto& io = ImGui::GetIO();
    bool typed = std::any_of(io.InputQueueCharacters.begin(), io.InputQueueCharacters.end(), [](ImWchar c) { return c > 32; });
    if (typed && !ImGui::IsAnyItemActive() && !widgets::capturingKey()) {
        ImGui::SetKeyboardFocusHere();
        pending.insert(pending.end(), io.InputQueueCharacters.begin(), io.InputQueueCharacters.end());
        pendingFrame = ImGui::GetFrameCount();
    }
    ImGui::InputTextWithHint("##search", i18n::tr("Search..."), search, sizeof(search), ImGuiInputTextFlags_EscapeClearsAll);
    if (ImGui::IsItemActive() && !pending.empty()) {
        for (ImWchar c : pending) io.AddInputCharacter(c);
        pending.clear();
    } else if (ImGui::GetFrameCount() - pendingFrame > 10) {
        pending.clear();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImVec2 sp{at.x + size.x - sw, at.y};
    ImGui::SetCursorScreenPos(sp);
    bool clicked = ImGui::InvisibleButton("favs", {sw, sw});
    bool hov = ImGui::IsItemHovered();
    ImU32 c = theme::col(onlyFavorites ? ImVec4{1.f, 0.82f, 0.4f, 1.f} : (hov ? t.text : t.textDim));
    star(ImGui::GetWindowDrawList(), sp + ImVec2(sw, sw) * 0.5f, 6.5f * s, c, onlyFavorites);
    if (hov) ImGui::SetTooltip("%s", i18n::tr("Favorites"));
    if (clicked) onlyFavorites = !onlyFavorites;
}

static void footer(ImVec2 at, float w) {
    auto& t = theme::current();
    float s = ui::scale();
    auto* dl = ImGui::GetWindowDrawList();
    auto info = rules::status();
    std::string label = info.server.empty() ? std::string(i18n::tr("No server")) : info.server;
    if (info.blocked) label += "  ·  " + i18n::fmt("{} blocked", info.blocked);
    ImVec4 dot = info.server.empty() ? t.off : (info.blocked ? t.warn : t.ok);
    float cy = at.y + 13 * s;
    float fs = 11.5f * s;
    dl->AddCircleFilled({at.x + 4 * s, cy}, 3 * s, theme::col(dot));
    std::string shown = fitText(fonts::regular(), fs, label, w - 14 * s);
    dl->AddText(fonts::regular(), fs, {at.x + 12 * s, cy - fs * 0.53f}, theme::col(t.textDim), shown.c_str());
}

static void drawMenu() {
    auto& t = theme::current();
    float s = ui::scale();
    float step = draw::motion() ? ui::dt() * t.animSpeed / (isOpen ? 0.30f : 0.16f) : 1.f;
    openT = std::clamp(openT + (isOpen ? step : -step), 0.f, 1.f);
    openAnim = draw::easeOutCubic(openT);
    if (!isOpen) widgets::closePopups();
    if (openT <= 0.f) return;

    auto ds = ImGui::GetIO().DisplaySize;
    float margin = 24 * s;
    float top = std::max(margin, ds.y * 0.08f);
    float bottom = ds.y - std::max(margin, ds.y * 0.07f);
    float listW = std::clamp(ds.x * 0.26f, 220 * s, 320 * s);
    float gap = 10 * s;
    float x = std::max(margin, (ds.x - (listW + gap + 700 * s)) * 0.5f);
    float dx = x + listW + gap;
    static float detailsW = 0.f;
    float wantW = std::max(200 * s, std::min(detailsWidth(), ds.x - dx - margin));
    detailsW = detailsW <= 0.f || !draw::motion() ? wantW : draw::approach(detailsW, wantW, 16.f * t.animSpeed);

    auto stage = [&](int i) { return std::clamp(openT * 1.45f - i * 0.17f, 0.f, 1.f); };
    auto lift = [&](float a) { return (1.f - draw::easeOutBack(a)) * (isOpen ? 22 : 10) * s; };
    auto dim = [&](int alpha) { return IM_COL32(6, 8, 14, int(alpha * openAnim)); };
    ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, ds, dim(70));
    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor({0, 0}, ds, dim(0), dim(0), dim(40), dim(40));

    float a = stage(0);
    ImVec2 sp{x, top + lift(a)}, ss{listW, 30 * s};
    beginPanel("##monchi_search", sp, ss, draw::easeOutCubic(a));
    searchBar(sp, ss);
    endPanel();

    a = stage(1);
    float foot = 26 * s;
    ImVec2 lp{x, top + 38 * s + lift(a)}, ls{listW, bottom - top - 38 * s};
    beginPanel("##monchi", lp, ls, draw::easeOutCubic(a));
    double t0 = profile::stamp();
    drawModulesPage(lp + ImVec2(8 * s, 8 * s), {ls.x - 10 * s, ls.y - 8 * s - foot});
    double listUs = profile::since(t0);
    footer({lp.x + 12 * s, lp.y + ls.y - foot}, ls.x - 24 * s);
    endPanel();

    a = stage(2);
    ImVec2 dp{dx, top + lift(a)}, dsz{detailsW, bottom - top};
    beginPanel("##monchi_details", dp, dsz, draw::easeOutCubic(a));
    t0 = profile::stamp();
    drawDetails(dp, dsz);
    profile::menu(float(listUs), float(profile::since(t0)));
    endPanel();

    bool typing = ImGui::GetIO().WantTextInput;
    if (isOpen && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !widgets::capturingKey() && !typing && !widgets::dropdownEscaped()) setOpen(false);
}

void hideOverlays() {
    if (isOpen) setOpen(false);
    hudEdit = false;
    keyboardClaim = false;
    keyboardClaimNext = false;
    openT = openAnim = 0.f;
    dropFocus = true;
}

void draw() {
    guard::beat();
    preview::mark();
    if (!input::focused() || game::state().screen != game::Screen::None) hideOverlays();
    if (dropFocus.exchange(false)) ImGui::ClearActiveID();
    pollDevCommands();
    profileDrive();
    if (hudEdit) {
        hudeditor::draw();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            hudEdit = false;
            setOpen(true);
        }
    }
    drawMenu();
}

}
