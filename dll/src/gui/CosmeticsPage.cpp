#include "cosmetics/GpuPreview.hpp"
#include "GuiInternal.hpp"
#include "I18n.hpp"
#include "Theme.hpp"
#include "Widgets.hpp"
#include "core/Config.hpp"
#include "core/Paths.hpp"
#include "cosmetics/Cosmetics.hpp"
#include "modules/Manager.hpp"
#include "modules/client/ClientSettings.hpp"
#include "modules/online/MonchiOnline.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>
#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>
#include <unordered_map>

namespace gui {

namespace {

int slotFilter = 0;
float yaw = 25.f;
int motion = 0;
bool held = false;
float devZoom = 1.f, devFocus = 20.f;
cosmetics::Rig rig;
const char* motions[] = {"Auto", "Idle", "Walk", "Sprint", "Jump"};
const char* slots[] = {"All", "Wings", "Capes", "Head", "Face", "Back", "Body", "Tails", "Feet", "Auras", "Pets"};
const char* slotIds[] = {"", "wings", "cape", "head", "face", "back", "body", "waist", "feet", "aura", "pet"};

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) {
        if (!part.empty()) out.push_back(part);
    }
    return out;
}

bool isEquipped(const std::string& list, const std::string& id) {
    auto v = split(list);
    return std::find(v.begin(), v.end(), id) != v.end();
}

std::string toHex(ImVec4 c) {
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", int(c.x * 255.f + 0.5f), int(c.y * 255.f + 0.5f), int(c.z * 255.f + 0.5f));
    return buf;
}

ImVec4 fromHex(const std::string& s, ImVec4 fallback) {
    if (s.size() != 7 || s[0] != '#' || s.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos) return fallback;
    unsigned v = (unsigned)std::strtoul(s.c_str() + 1, nullptr, 16);
    return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, 1.f};
}

std::vector<ImVec4> savedTints(ClientSettings* cs, const cosmetics::Item& item) {
    std::vector<ImVec4> out;
    for (auto& t : item.tints) out.push_back(t.color);
    if (!cs) return out;
    static std::string savedText;
    static nlohmann::json saved;
    if (savedText != cs->tints().text) {
        savedText = cs->tints().text;
        saved = nlohmann::json::parse(savedText, nullptr, false);
    }
    const auto& j = saved;
    if (!j.is_object() || !j.contains(item.id) || !j[item.id].is_array()) return out;
    for (size_t i = 0; i < out.size() && i < j[item.id].size(); i++)
        if (j[item.id][i].is_string()) out[i] = fromHex(j[item.id][i].get<std::string>(), out[i]);
    return out;
}

void saveTints(ClientSettings* cs, const cosmetics::Item& item, const std::vector<ImVec4>& colors) {
    if (!cs) return;
    auto j = nlohmann::json::parse(cs->tints().text, nullptr, false);
    if (!j.is_object()) j = nlohmann::json::object();
    auto arr = nlohmann::json::array();
    for (auto& c : colors) arr.push_back(toHex(c));
    j[item.id] = arr;
    cs->tints().text = j.dump();
    config::markDirty();
}

cosmetics::Moving demoMotion() {
    cosmetics::Moving m;
    int mode = motion;
    float t = float(ui::time());
    if (mode == 0) {
        float cycle = std::fmod(t, 14.f);
        mode = cycle < 3.f ? 1 : cycle < 7.f ? 2 : cycle < 11.f ? 3 : 4;
    }
    if (mode == 2) m.fwd = 4.3f;
    if (mode == 3) {
        m.fwd = 5.6f;
        m.sprint = true;
    }
    if (mode == 4) {
        float k = std::fmod(t, 1.4f);
        m.fwd = 1.5f;
        m.air = k < 0.8f;
        m.up = m.air ? 7.f * (1.f - k / 0.4f) : 0.f;
    }
    m.turn = 18.f * std::sin(t * 0.7f) * (mode == 1 ? 0.f : 1.f);
    return m;
}

struct Frame {
    float focus, zoom, yaw;
};

Frame cardFrame(const std::string& slot) {
    if (slot == "head" || slot == "face") return {30.f, 4.3f, 25.f};
    if (slot == "feet") return {3.f, 5.6f, 30.f};
    if (slot == "wings") return {17.f, 2.5f, 155.f};
    if (slot == "body" || slot == "back") return {16.f, 2.6f, 155.f};
    if (slot == "pet") return {16.f, 2.1f, 30.f};
    if (slot == "waist") return {14.f, 3.6f, 150.f};
    if (slot == "aura") return {7.f, 3.2f, 30.f};
    return {19.f, 3.5f, 155.f};
}

void toggleEquipped(Setting& st, const cosmetics::Item& item) {
    auto v = split(st.text);
    auto same = std::find(v.begin(), v.end(), item.id);
    if (same != v.end()) {
        v.erase(same);
    } else {
        std::erase_if(v, [&](const std::string& other) {
            auto* it = cosmetics::find(other);
            return it && it->slot == item.slot;
        });
        v.push_back(item.id);
    }
    std::string out;
    for (auto& e : v) out += (out.empty() ? "" : ",") + e;
    st.text = out;
    config::markDirty();
}


}

void cosmeticsDev(const std::string& args) {
    std::istringstream in(args);
    std::string ids;
    devZoom = 1.f;
    devFocus = 20.f;
    in >> ids >> motion >> yaw >> devZoom >> devFocus;
    motion = std::clamp(motion, 0, 4);
    held = true;
    if (auto* cs = modules::get<ClientSettings>()) cs->equipped().text = ids == "-" ? "" : ids;
}

void drawCosmeticsPage(ImVec2 origin, ImVec2 size) {
    auto& t = theme::current();
    float s = ui::scale();
    auto* cs = modules::get<ClientSettings>();
    auto* dl = ImGui::GetWindowDrawList();

    ImGui::SetCursorScreenPos(origin);
    widgets::hint("Cosmetics are still in beta and may have bugs.");
    if (auto* online = modules::get<MonchiOnline>()) {
        if (widgets::row("Show cosmetics in game", online->worldCosmetics())) config::markDirty();
        if (!online->enabled()) widgets::hint("Enable Monchi Online to show cosmetics on players.");
    }
    const float headerH = ImGui::GetCursorScreenPos().y - origin.y + 8 * s;
    origin.y += headerH;
    size.y = std::max(1.f, size.y - headerH);

    float previewW = std::min(420 * s, size.x * 0.34f);
    float gap = 22 * s;
    float listW = size.x - previewW - gap;

    rig.step(ui::dt() * (cs ? cs->animSpeed().f : 1.f), demoMotion());

    ImGui::SetCursorScreenPos(origin);
    for (int i = 0; i < int(std::size(slots)); i++) {
        if (i) {
            float next = ImGui::CalcTextSize(i18n::tr(slots[i])).x + 28 * s;
            if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + next <= origin.x + listW) ImGui::SameLine();
            // a new row starts under the first chip, not at the window's own left edge
            else ImGui::SetCursorScreenPos({origin.x, ImGui::GetCursorScreenPos().y});
        }
        if (widgets::button(slots[i], {0, 0}, slotFilter == i)) slotFilter = i;
    }
    ImGui::Dummy({0, 10 * s});
    float filtersH = ImGui::GetCursorScreenPos().y - origin.y;
    ImGui::SetCursorScreenPos({origin.x, origin.y + filtersH});
    ImGui::BeginChild("cosmetics", {listW, std::max(1.f, size.y - filtersH)}, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    smoothScroll();

    std::vector<const cosmetics::Item*> shown;
    for (auto& it : cosmetics::items())
        if (!slotFilter || it.slot == slotIds[slotFilter]) shown.push_back(&it);

    if (shown.empty()) {
        widgets::hint(cosmetics::items().empty() ? "No cosmetics installed yet. Wings, capes and bandanas arrive with the first release and will show up here."
                                     : "Nothing in this category.");
    }

    float avail = ImGui::GetContentRegionAvail().x;
    float cg = 14 * s;
    int cols = std::max(1, int((avail + cg) / (210 * s + cg)));
    float w = (avail - cg * (cols - 1)) / cols;
    ImVec2 o = ImGui::GetCursorScreenPos();
    struct Thumbnail { std::vector<cosmetics::PreviewFace> faces; unsigned seen = 0; };
    static std::unordered_map<std::string, Thumbnail> thumbnails;
    static unsigned thumbnailFrame = 0;
    ++thumbnailFrame;
    static std::string thumbnailTints;
    static unsigned thumbnailGeneration = 0;
    static ImVec4 thumbnailAccent{};
    static bool thumbnailSlim = false;
    static float thumbnailScale = 0.f;
    const bool slim = cs && cs->slim().b;
    const std::string tintText = cs ? cs->tints().text : "";
    const auto generation = cosmetics::generation();
    if (thumbnailGeneration != generation || thumbnailTints != tintText || thumbnailSlim != slim || thumbnailScale != s ||
        thumbnailAccent.x != t.accent.x || thumbnailAccent.y != t.accent.y || thumbnailAccent.z != t.accent.z || thumbnailAccent.w != t.accent.w) {
        thumbnails.clear();
        thumbnailGeneration = generation;
        thumbnailTints = tintText;
        thumbnailSlim = slim;
        thumbnailScale = s;
        thumbnailAccent = t.accent;
    }
    ImGuiListClipper clipper;
    unsigned prepared = 0;
    clipper.Begin(int((shown.size() + cols - 1) / cols), 176 * s + cg);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            for (int col = 0; col < cols; ++col) {
                const size_t i = size_t(row) * cols + col;
                if (i >= shown.size()) break;
                const cosmetics::Item& it = *shown[i];
                ImVec2 p = o + ImVec2((i % cols) * (w + cg), (i / cols) * (176 * s + cg));
                ImGui::SetCursorScreenPos(p);
                ImGui::PushID(it.id.c_str());
                bool clicked = ImGui::InvisibleButton("c", {w, 176 * s});
                bool hov = ImGui::IsItemHovered();
                ImGui::PopID();
                if (!ImGui::IsItemVisible()) continue;
                bool eq = cs && isEquipped(cs->equipped().text, it.id);
                auto* cdl = ImGui::GetWindowDrawList();
                float r = t.rounding * 0.75f * s;
                cdl->AddRectFilled(p, p + ImVec2(w, 176 * s), theme::col(theme::mix(t.surface, t.text, hov ? 0.05f : 0.f), 0.92f), r);
                cdl->AddRect(p, p + ImVec2(w, 176 * s), theme::col(eq ? t.accent : theme::border(), eq ? 1.f : 0.5f), r, 0, eq ? 1.5f * s : 1.f);
                cdl->AddRectFilled(p + ImVec2(8 * s, 8 * s), p + ImVec2(w - 8 * s, 104 * s), theme::col(t.bg, 0.55f), r * 0.8f);
                auto fr = cardFrame(it.slot);
                cdl->PushClipRect(p + ImVec2(10 * s, 10 * s), p + ImVec2(w - 10 * s, 104 * s), true);
                if (auto cached = thumbnails.find(it.id); cached != thumbnails.end()) cached->second.seen = thumbnailFrame;
                if (hov) {
                    cosmetics::drawPreview(cdl, p + ImVec2(w * 0.5f, 57 * s), fr.zoom * s, fr.yaw + std::sin(float(ui::time()) * 0.8f) * 12.f, 10.f,
                                           {{&it, savedTints(cs, it)}}, t.accent, {slim, fr.focus, &rig});
                } else {
                    auto [cached, added] = thumbnails.try_emplace(it.id);
                    if (cached->second.faces.empty() && prepared < 2) {
                        ++prepared;
                        cosmetics::Look look{slim, fr.focus};
                        look.frame = &cached->second.faces;
                        cosmetics::drawPreview(nullptr, {}, fr.zoom * s, fr.yaw, 10.f, {{&it, savedTints(cs, it)}}, t.accent, look);
                    }
                    cached->second.seen = thumbnailFrame;
                    cosmetics::drawFrame(cdl, cached->second.faces, p + ImVec2(w * 0.5f, 57 * s), 1.f);
                }
                cdl->PopClipRect();
                cdl->AddText(fonts::regular(), 13.5f * s, p + ImVec2(12 * s, 114 * s), theme::col(t.text), it.name.c_str());
                cdl->AddText(fonts::regular(), 11.5f * s, p + ImVec2(12 * s, 134 * s), theme::col(t.textDim), it.slot.c_str());
                const char* label = i18n::tr(eq ? "Equipped" : "Equip");
                ImVec2 ls = fonts::regular()->CalcTextSizeA(12 * s, FLT_MAX, 0.f, label);
                ImVec2 bmin{p.x + w - ls.x - 30 * s, p.y + 142 * s}, bmax{p.x + w - 10 * s, p.y + 164 * s};
                cdl->AddRectFilled(bmin, bmax, theme::col(eq ? t.accent : t.surfaceHover), 5 * s);
                cdl->AddText(fonts::regular(), 12 * s, (bmin + bmax - ls) * 0.5f, theme::col(eq ? ImVec4(1, 1, 1, 1) : t.textDim), label);
                if (clicked && cs) toggleEquipped(cs->equipped(), it);
            }
        }
    }
    std::erase_if(thumbnails, [&](const auto& entry) { return thumbnailFrame - entry.second.seen > 300; });
    ImGui::EndChild();

    ImVec2 po{origin.x + listW + gap, origin.y};
    float stageH = size.y * 0.52f;
    dl->AddRectFilled(po, po + ImVec2(previewW, size.y), theme::col(t.surface, 0.6f), t.rounding * 0.75f * s);
    ImGui::SetCursorScreenPos(po);
    ImGui::InvisibleButton("preview", {previewW, stageH});
    float spin = cs ? cs->spin().f : 18.f;
    if (ImGui::IsItemActive()) yaw -= ImGui::GetIO().MouseDelta.x * 0.7f;
    else if (!held) yaw += ui::dt() * spin;
    std::vector<cosmetics::Worn> worn;
    if (cs)
        for (auto& id : split(cs->equipped().text))
            if (auto* it = cosmetics::find(id)) worn.push_back({it, savedTints(cs, *it)});
    static cosmetics::Item playerSkin;
    static std::vector<unsigned char> lastSkin;
    const auto& skin = game::state().skin;
    if (!skin.rgba.empty() && (lastSkin != skin.rgba || playerSkin.texW != skin.width || playerSkin.texH != skin.height)) {
        cosmetics::retire(playerSkin.texture);
        playerSkin.texture = IM_NEW(ImTextureData)();
        playerSkin.texture->Create(ImTextureFormat_RGBA32, skin.width, skin.height);
        std::memcpy(playerSkin.texture->GetPixels(), skin.rgba.data(), skin.rgba.size());
        ImGui::RegisterUserTexture(playerSkin.texture);
        playerSkin.texW = skin.width;
        playerSkin.texH = skin.height;
        lastSkin = skin.rgba;
    }
    cosmetics::Look look{(cs && cs->slim().b) || (!skin.rgba.empty() && skin.slim), devFocus, &rig, skin.rgba.empty() ? nullptr : &playerSkin};
    if (!held) look.fitSize = {std::max(1.f, previewW - 32 * s), std::max(1.f, stageH - 32 * s)};
    dl->PushClipRect(po, po + ImVec2(previewW, stageH), true);
    cosmetics::drawPreview(dl, {po.x + previewW * 0.5f, po.y + stageH * 0.5f}, stageH / 54.f * devZoom, yaw, 12.f, worn, t.accent, look);
    dl->PopClipRect();

    ImGui::SetCursorScreenPos({po.x + 14 * s, po.y + stageH + 6 * s});
    ImGui::BeginChild("cosmeticLook", {previewW - 28 * s, size.y - stageH - 12 * s}, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    smoothScroll();
    float limit = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for (int i = 0; i < 5; i++) {
        if (i) {
            float next = ImGui::CalcTextSize(i18n::tr(motions[i])).x + 28 * s;
            if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + next <= limit) ImGui::SameLine();
        }
        if (widgets::button(motions[i], {0, 0}, motion == i)) motion = i;
    }
    if (cs) {
        widgets::setting(cs->slim());
        widgets::setting(cs->spin());
        widgets::setting(cs->animSpeed());
        for (auto& w : worn) {
            ImGui::PushID(w.item->id.c_str());
            ImGui::Dummy({0, 4 * s});
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextUnformatted(w.item->name.c_str());
            ImGui::PopTextWrapPos();
            bool changed = false;
            auto colors = w.tints;
            for (size_t i = 0; i < colors.size(); i++) {
                float swatch = ImGui::GetFrameHeight();
                if (i > 0 && ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + swatch <= limit)
                    ImGui::SameLine();
                float c4[4] = {colors[i].x, colors[i].y, colors[i].z, 1.f};
                ImGui::PushID(int(i));
                if (ImGui::ColorEdit3("##tint", c4, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                    colors[i] = {c4[0], c4[1], c4[2], 1.f};
                    changed = true;
                }
                if (ImGui::IsItemHovered() && i < w.item->tints.size()) ImGui::SetTooltip("%s", w.item->tints[i].name.c_str());
                ImGui::PopID();
            }
            if (changed) saveTints(cs, *w.item, colors);
            ImGui::PopID();
        }
        if (worn.empty()) widgets::hint("Equip a cosmetic to change its colors here.");
    }
    ImGui::EndChild();
}

void reloadCosmetics() {
    if (cosmetics::items().empty()) cosmetics::reload();
}

}
