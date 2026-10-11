#include "Look.hpp"
#include "Embedded.hpp"
#include "Files.hpp"
#include "Game.hpp"
#include "Versions.hpp"
#include "../res/resource.h"

#include "Cosmetics.hpp"
#include "GpuPreview.hpp"

#include <imgui_internal.h>
#include <json.hpp>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace look {

namespace {

using json = nlohmann::json;
namespace fs = files::fs;

cosmetics::Rig* rig = nullptr;
cosmetics::Item skin;
bool slimSkin = false, slimSetting = false;
std::string worn, tintText;
json tints;
std::vector<Entry> list;
unsigned listedFor = ~0u;
float yaw = 25.f;
double checked = -10.0;
fs::file_time_type skinTime{}, configTime{};
bool gameRuns = false, noConfig = true;
bool fromGameFiles = false, lockedSkin = false;
int standInWanted = 1, standInShown = -1;
bool known = false;

fs::path configFile() {
    auto settings = json::parse(files::read(files::root() / L"settings.json"), nullptr, false);
    std::string profile = settings.is_object() && settings.contains("profile") && settings["profile"].is_string() ? settings["profile"].get<std::string>() : "default";
    if (profile.empty() || profile.find_first_of("/\\:.") != std::string::npos) profile = "default";
    return files::root() / L"configs" / files::widen(profile + ".json");
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ','))
        if (!part.empty()) out.push_back(part);
    return out;
}

ImVec4 fromHex(const std::string& s, ImVec4 fallback) {
    if (s.size() != 7 || s[0] != '#' || s.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos) return fallback;
    unsigned v = unsigned(std::strtoul(s.c_str() + 1, nullptr, 16));
    return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, 1.f};
}

std::vector<ImVec4> tintsOf(const cosmetics::Item& item) {
    std::vector<ImVec4> out;
    for (auto& t : item.tints) out.push_back(t.color);
    if (!tints.is_object() || !tints.contains(item.id) || !tints[item.id].is_array()) return out;
    auto& saved = tints[item.id];
    for (size_t i = 0; i < out.size() && i < saved.size(); i++)
        if (saved[i].is_string()) out[i] = fromHex(saved[i].get<std::string>(), out[i]);
    return out;
}

bool show(const unsigned char* rgba, uint32_t w, uint32_t h, bool slim) {
    if ((w != 64 && w != 128 && w != 256) || (h != w && h * 2 != w)) return false;
    cosmetics::retire(skin.texture);
    skin.texture = IM_NEW(ImTextureData)();
    skin.texture->Create(ImTextureFormat_RGBA32, int(w), int(h));
    std::memcpy(skin.texture->GetPixels(), rgba, size_t(w) * h * 4);
    ImGui::RegisterUserTexture(skin.texture);
    skin.texW = int(w);
    skin.texH = int(h);
    slimSkin = slim;
    return true;
}

bool readSkin() {
    std::string data = files::read(files::root() / L"cache" / L"skin.bin");
    uint32_t head[3]{};
    if (data.size() < 16 || data.compare(0, 4, "MSKN") != 0) return false;
    std::memcpy(head, data.data() + 4, sizeof(head));
    if (data.size() != 16 + size_t(head[0]) * head[1] * 4) return false;
    return show(reinterpret_cast<const unsigned char*>(data.data()) + 16, head[0], head[1], head[2] != 0);
}

void showStandIn() {
    standInShown = standInWanted;
    HRSRC found = FindResourceW(nullptr, MAKEINTRESOURCEW(standInWanted == 1 ? IDR_SKIN_GIRL : IDR_SKIN_BOY), RT_RCDATA);
    HGLOBAL loaded = found ? LoadResource(nullptr, found) : nullptr;
    if (!loaded) return;
    int w = 0, h = 0, channels = 0;
    auto* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(LockResource(loaded)), int(SizeofResource(nullptr, found)), &w, &h, &channels, 4);
    if (!pixels) return;
    show(pixels, uint32_t(w), uint32_t(h), true);
    stbi_image_free(pixels);
}

// Before the client has ever seen the skin in the game, the game's own files say which one is picked: options.txt
// names it as "<skin pack uuid>_<skin name>". The skins that ship with the game lie in its folder as plain pictures
// and are shown from there. Skins from the Marketplace and from the character creator are kept encrypted by the game;
// those are left alone and show once the client has seen them in a game.
fs::path pickedOptions() {
    wchar_t roaming[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"APPDATA", roaming, MAX_PATH)) return {};
    fs::path best;
    fs::file_time_type newest{};
    std::error_code ec;
    for (auto& user : fs::directory_iterator(fs::path(roaming) / L"Minecraft Bedrock" / L"Users", ec)) {
        auto file = user.path() / L"games" / L"com.mojang" / L"minecraftpe" / L"options.txt";
        auto time = fs::last_write_time(file, ec);
        if (ec || (!best.empty() && time <= newest)) continue;
        best = file;
        newest = time;
    }
    return best;
}

void readGameSkin() {
    lockedSkin = false;
    std::string options = files::read(pickedOptions());
    const std::string key = "game_skintypefull:";
    size_t at = options.find(key);
    if (at == std::string::npos) return;
    std::string picked = options.substr(at + key.size(), options.find_first_of("\r\n", at) - at - key.size());
    size_t cut = picked.find('_');
    if (cut == std::string::npos) return;
    std::string pack = picked.substr(0, cut), name = picked.substr(cut + 1);
    std::vector<fs::path> folders{game::installFolder()};
    for (auto& install : versions::scan({})) folders.push_back(install.exe.parent_path());
    for (auto& folder : folders) {
        auto dir = folder / L"data" / L"skin_packs" / L"vanilla";
        auto manifest = json::parse(files::read(dir / L"manifest.json"), nullptr, false);
        if (!manifest.is_object() || !manifest.contains("header") || manifest["header"].value("uuid", "") != pack) continue;
        auto skins = json::parse(files::read(dir / L"skins.json"), nullptr, false, true);
        if (!skins.is_object() || !skins.contains("skins") || !skins["skins"].is_array()) return;
        for (auto& entry : skins["skins"]) {
            if (!entry.is_object() || entry.value("localization_name", "") != name) continue;
            std::string texture = entry.value("texture", "");
            if (texture.empty() || texture.find_first_of("/\\:") != std::string::npos) return;
            std::string png = files::read(dir / files::widen(texture));
            int w = 0, h = 0, channels = 0;
            auto* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(png.data()), int(png.size()), &w, &h, &channels, 4);
            if (!pixels) return;
            fromGameFiles = show(pixels, uint32_t(w), uint32_t(h), entry.value("geometry", "").find("Slim") != std::string::npos);
            stbi_image_free(pixels);
            return;
        }
        return;
    }
    lockedSkin = true;
}

json* clientSettings(json& config) {
    if (!config.is_object() || !config.contains("modules") || !config["modules"].is_object()) return nullptr;
    auto& modules = config["modules"];
    if (!modules.contains("Client Settings") || !modules["Client Settings"].is_object()) return nullptr;
    auto& module = modules["Client Settings"];
    if (!module.contains("settings") || !module["settings"].is_object()) return nullptr;
    return &module["settings"];
}

void readConfig() {
    auto config = json::parse(files::read(configFile()), nullptr, false);
    auto* settings = clientSettings(config);
    noConfig = !settings;
    worn.clear();
    tintText.clear();
    slimSetting = false;
    if (!settings) return;
    auto& s = *settings;
    if (s.contains("cosmetics") && s["cosmetics"].is_string()) worn = s["cosmetics"].get<std::string>();
    if (s.contains("cosmeticTints") && s["cosmeticTints"].is_string()) tintText = s["cosmeticTints"].get<std::string>();
    if (s.contains("slimArms") && s["slimArms"].is_boolean()) slimSetting = s["slimArms"].get<bool>();
    tints = json::parse(tintText, nullptr, false);
    listedFor = ~0u;
}

// files are looked at once a second: the client rewrites both while the game runs
void refresh() {
    if (!known && standInShown != standInWanted) showStandIn();
    double now = ImGui::GetTime();
    if (now - checked < 1.0) return;
    checked = now;
    gameRuns = game::running().has_value();
    std::error_code ec;
    auto st = fs::last_write_time(files::root() / L"cache" / L"skin.bin", ec);
    static bool seen = false;
    if (!ec && st != skinTime) {
        skinTime = st;
        if (readSkin()) seen = true, fromGameFiles = false;
    }
    static bool asked = false;
    if (!seen && !asked) {
        asked = true;
        readGameSkin();
    }
    known = seen || fromGameFiles;
    auto ct = fs::last_write_time(configFile(), ec);
    if (ec) noConfig = true;
    else if (ct != configTime) {
        configTime = ct;
        readConfig();
    }
}

void relist() {
    auto ids = split(worn);
    list.clear();
    for (auto& item : cosmetics::items()) list.push_back({item.id, item.name, item.slot, std::find(ids.begin(), ids.end(), item.id) != ids.end()});
    std::stable_sort(list.begin(), list.end(), [](const Entry& a, const Entry& b) {
        return a.slot != b.slot ? a.slot < b.slot : a.name < b.name;
    });
    listedFor = cosmetics::generation();
}

cosmetics::Moving idle(float t) {
    cosmetics::Moving m;
    float cycle = std::fmod(t, 12.f);
    if (cycle > 5.f && cycle < 9.f) m.fwd = 4.3f;
    return m;
}

}

void init(ID3D11Device* device, ID3D11DeviceContext* context) {
    embedded::cosmetics();
    cosmetics::gpu::init(device, context);
    rig = new cosmetics::Rig();
}

void stop() {
    delete rig;
    rig = nullptr;
    cosmetics::gpu::stop();
}

bool hasSkin() { return skin.texture != nullptr; }
bool skinFromGameFiles() { return fromGameFiles; }
bool skinLocked() { return lockedSkin && !known; }
void standIn(int which) { standInWanted = which == 1 ? 1 : 0; }
bool usingStandIn() { return !known; }
bool gameOwnsSettings() { return gameRuns; }
bool settingsMissing() { return noConfig; }

const std::vector<Entry>& entries() {
    cosmetics::ensureLoaded();
    refresh();
    if (listedFor != cosmetics::generation()) relist();
    return list;
}

void toggle(const std::string& id) {
    if (gameRuns || noConfig) return;
    auto* item = cosmetics::find(id);
    if (!item) return;
    auto file = configFile();
    auto config = json::parse(files::read(file), nullptr, false);
    auto* settings = clientSettings(config);
    if (!settings) return;
    auto& s = *settings;
    auto ids = split(s.contains("cosmetics") && s["cosmetics"].is_string() ? s["cosmetics"].get<std::string>() : "");
    auto same = std::find(ids.begin(), ids.end(), id);
    if (same != ids.end()) ids.erase(same);
    else {
        // one item per slot, as in the client
        std::erase_if(ids, [&](const std::string& other) {
            auto* it = cosmetics::find(other);
            return it && it->slot == item->slot;
        });
        ids.push_back(id);
    }
    std::string out;
    for (auto& e : ids) out += (out.empty() ? "" : ",") + e;
    s["cosmetics"] = out;
    auto part = file;
    part += L".part";
    if (!files::write(part, config.dump(2))) return;
    std::error_code ec;
    if (!MoveFileExW(part.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(part, ec);
        return;
    }
    worn = out;
    configTime = fs::last_write_time(file, ec);
    listedFor = ~0u;
}

void figure(ImDrawList* dl, const char* id, ImVec2 min, ImVec2 max, ImVec4 body) {
    cosmetics::ensureLoaded();
    refresh();
    if (!rig) return;
    ImVec2 size{max.x - min.x, max.y - min.y};
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(id, size);
    float dt = ImGui::GetIO().DeltaTime;
    if (ImGui::IsItemActive()) yaw -= ImGui::GetIO().MouseDelta.x * 0.7f;
    else yaw += dt * 14.f;
    rig->step(dt, idle(float(ImGui::GetTime())));
    std::vector<cosmetics::Worn> on;
    for (auto& wornId : split(worn))
        if (auto* item = cosmetics::find(wornId)) on.push_back({item, tintsOf(*item)});
    cosmetics::Look how{slimSetting || (skin.texture && slimSkin), 20.f, rig, skin.texture ? &skin : nullptr};
    how.fitSize = {std::max(1.f, size.x - 24.f), std::max(1.f, size.y - 24.f)};
    dl->PushClipRect(min, max, true);
    cosmetics::drawPreview(dl, {min.x + size.x * 0.5f, min.y + size.y * 0.5f}, size.y / 54.f, yaw, 12.f, on, body, how);
    dl->PopClipRect();
}

}
