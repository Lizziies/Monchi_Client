#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "Cosmetics.hpp"
#include "GpuPreview.hpp"
#include "RetiredTextures.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "render/Ui.hpp"

#include <imgui_internal.h>
#include <json.hpp>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace cosmetics {

namespace {

std::vector<Item> list;
unsigned gen = 1;
bool loaded = false;

std::vector<unsigned char> readFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

ImTextureData* upload(const unsigned char* rgba, int w, int h) {
    auto* td = IM_NEW(ImTextureData)();
    td->Create(ImTextureFormat_RGBA32, w, h);
    std::memcpy(td->GetPixels(), rgba, size_t(w) * size_t(h) * 4);
    ImGui::RegisterUserTexture(td);
    return td;
}


V3 vec(const nlohmann::json& j, V3 fallback = {}) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

ImVec4 hex(const std::string& s) {
    unsigned v = s.size() >= 7 ? (unsigned)std::strtoul(s.c_str() + 1, nullptr, 16) : 0xffffff;
    return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, 1.f};
}

Motion motionOf(const std::string& s) {
    static const std::pair<const char*, Motion> names[] = {{"flap", Motion::Flap},   {"sway", Motion::Sway}, {"bob", Motion::Bob},
                                                          {"wag", Motion::Wag},     {"twitch", Motion::Twitch}, {"float", Motion::Float},
                                                          {"spin", Motion::Spin},   {"sparkle", Motion::Sparkle}, {"walk", Motion::Walk},
                                                          {"hop", Motion::Hop}};
    for (auto& [n, m] : names)
        if (s == n) return m;
    return Motion::None;
}

bool readItem(const std::filesystem::path& dir, Item& out);

// a field of the wrong type makes the json library throw; that costs the one item, not the frame that asked for it
bool loadItem(const std::filesystem::path& dir, Item& out) {
    try {
        return readItem(dir, out);
    } catch (const std::exception& e) {
        logger::warn("cosmetic in {}: {}", logger::narrow(dir.filename().wstring()), e.what());
        return false;
    }
}

bool readItem(const std::filesystem::path& dir, Item& out) {
    std::ifstream in(dir / L"item.json");
    if (!in) return false;
    auto j = nlohmann::json::parse(in, nullptr, false);
    if (!j.is_object()) return false;
    out.id = j.value("id", "");
    out.name = j.value("name", out.id);
    out.slot = j.value("slot", "");
    if (out.id.empty() || !j.contains("bones")) return false;
    out.texel = std::clamp(j.value("texel", 1.f), 1.f, 8.f);

    if (j.contains("tint") && j["tint"].is_array())
        for (auto& t : j["tint"]) out.tints.push_back({t.value("name", ""), hex(t.value("default", "#ffffff"))});

    std::vector<std::string> names;
    for (auto& b : j["bones"]) {
        Bone bone;
        std::string parent = b.value("parent", "");
        for (size_t i = 0; i < names.size(); i++)
            if (!parent.empty() && names[i] == parent) bone.parent = int(i);
        names.push_back(b.value("name", ""));
        bone.pivot = vec(b.value("pivot", nlohmann::json()));
        bone.rotation = vec(b.value("rotation", nlohmann::json()));
        if (b.contains("anim") && b["anim"].is_object()) {
            auto& a = b["anim"];
            std::string axis = a.value("axis", "y");
            bone.anim = {motionOf(a.value("type", "")), axis == "x" ? 0 : axis == "z" ? 2 : 1, a.value("amplitude", 0.f), a.value("speed", 1.f), a.value("phase", 0.f)};
        }
        if (b.contains("physics")) {
            auto& ph = b["physics"];
            std::string type = ph.is_string() ? ph.get<std::string>() : ph.is_object() ? ph.value("type", "spring") : "";
            Physics& p = bone.physics;
            p.cloth = type == "cloth";
            p.spring = type == "spring";
            if (ph.is_object()) {
                p.stiffness = std::clamp(ph.value("stiffness", p.cloth ? 38.f : 60.f), 1.f, 400.f);
                p.damping = std::clamp(ph.value("damping", p.cloth ? 2.4f : 7.f), 0.1f, 60.f);
                p.inertia = std::clamp(ph.value("inertia", 1.f), 0.f, 5.f);
                p.wind = std::clamp(ph.value("wind", 1.f), 0.f, 4.f);
                if (ph.contains("drive") && ph["drive"].is_object()) {
                    auto& d = ph["drive"];
                    p.air = vec(d.value("air", nlohmann::json()));
                    p.sprint = vec(d.value("sprint", nlohmann::json()));
                    p.sneak = vec(d.value("sneak", nlohmann::json()));
                    p.speed = vec(d.value("speed", nlohmann::json()));
                }
            }
        }
        for (auto& c : b.value("cubes", nlohmann::json::array())) {
            Cube cube;
            cube.origin = vec(c.value("origin", nlohmann::json()));
            cube.size = vec(c.value("size", nlohmann::json()), {1, 1, 1});
            float k = out.texel;
            cube.uvSize = vec(c.value("uvsize", nlohmann::json()), {std::round(cube.size.x * k), std::round(cube.size.y * k), std::round(cube.size.z * k)});
            cube.flat = c.value("flat", false);
            cube.mirror = c.value("mirror", false);
            auto uv = c.value("uv", std::vector<int>{0, 0});
            cube.u = uv.size() > 0 ? uv[0] : 0;
            cube.v = uv.size() > 1 ? uv[1] : 0;
            std::string tint = c.value("tint", ""), tint2 = c.value("tint2", "");
            for (size_t i = 0; i < out.tints.size(); i++) {
                if (out.tints[i].name == tint) cube.tint = int(i);
                if (out.tints[i].name == tint2) cube.tint2 = int(i);
            }
            cube.mix = std::clamp(c.value("mix", 0.f), 0.f, 1.f);
            bone.cubes.push_back(cube);
        }
        for (auto& t : b.value("tubes", nlohmann::json::array())) {
            Tube tube;
            tube.sides = std::clamp(t.value("sides", 16), 6, 32);
            tube.power = std::clamp(t.value("power", 2.f), 1.f, 12.f);
            tube.capped = t.value("capped", true);
            tube.twoSided = t.value("two_sided", false);
            auto tintIndex = [&](const char* key) {
                std::string name = t.value(key, "");
                for (size_t i = 0; i < out.tints.size(); i++)
                    if (out.tints[i].name == name) return int(i);
                return -1;
            };
            tube.tint = tintIndex("tint");
            tube.tint2 = tintIndex("tint2");
            if (t.contains("wave") && t["wave"].is_object()) {
                auto& w = t["wave"];
                std::string axis = w.value("axis", "x");
                tube.wave = {axis == "y" ? 1 : axis == "z" ? 2 : 0, w.value("amplitude", 0.f), w.value("speed", 1.f), w.value("freq", 1.f)};
            }
            auto path = t.value("path", nlohmann::json::array());
            for (size_t i = 0; i < path.size(); i++) {
                auto& e = path[i];
                if (!e.is_array() || e.size() < 4) continue;
                PathPoint pt;
                pt.pos = {e[0].get<float>(), e[1].get<float>(), e[2].get<float>()};
                pt.rx = e[3].get<float>();
                pt.rz = e.size() > 4 ? e[4].get<float>() : pt.rx;
                pt.mix = e.size() > 5 ? e[5].get<float>() : float(i) / float(std::max<size_t>(1, path.size() - 1));
                tube.path.push_back(pt);
            }
            if (tube.path.size() >= 2) bone.tubes.push_back(std::move(tube));
        }
        out.bones.push_back(std::move(bone));
    }

    auto png = readFile(dir / std::filesystem::path(j.value("texture", "tex.png")));
    int w = 0, h = 0, n = 0;
    unsigned char* pixels = png.empty() ? nullptr : stbi_load_from_memory(png.data(), int(png.size()), &w, &h, &n, 4);
    if (!pixels) {
        for (auto& b : out.bones)
            if (!b.cubes.empty()) return false;
        return true;
    }
    out.texW = w;
    out.texH = h;
    out.texture = upload(pixels, w, h);
    out.texturePath = logger::narrow((dir / std::filesystem::path(j.value("texture", "tex.png"))).wstring());
    stbi_image_free(pixels);
    return true;
}

}

const std::vector<Item>& items() { return list; }

unsigned generation() { return gen; }

const Item* find(const std::string& id) {
    for (auto& i : list)
        if (i.id == id) return &i;
    return nullptr;
}

void retire(ImTextureData* td) {
    if (!td) return;
    gpu::forget(td);
    textures::retire(td);
}

void ensureLoaded() {
    textures::collect();
    if (!loaded) reload();
}

void reload() {
    loaded = true;
    gen++;
    for (auto& i : list) retire(i.texture);
    list.clear();
    auto root = paths::root() / L"cosmetics";
    std::ifstream in(root / L"index.json");
    auto j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    if (!j.is_object() || !j.contains("items")) j = nlohmann::json{{"items", nlohmann::json::array()}};
    if (!j["items"].is_array()) j["items"] = nlohmann::json::array();
    for (auto& e : j["items"]) {
        const std::string id = e.is_object() && e.contains("id") && e["id"].is_string() ? e["id"].get<std::string>() : "";
        if (id.empty() || id.find_first_of("/\\:") != std::string::npos || id == "." || id == "..") continue;
        Item item;
        if (loadItem(root / std::filesystem::path(id), item)) list.push_back(std::move(item));
        else logger::warn("cosmetic {} could not be loaded", id);
    }
    auto line = root / L"line";
    std::ifstream lineIndex(line / L"index.json");
    auto catalog = lineIndex ? nlohmann::json::parse(lineIndex, nullptr, false) : nlohmann::json();
    if (catalog.is_object() && catalog.contains("items") && catalog["items"].is_array()) {
        for (const auto& entry : catalog["items"]) {
            const std::string id = entry.is_object() && entry.contains("id") && entry["id"].is_string() ? entry["id"].get<std::string>() : "";
            if (id.empty() || id.find_first_of("/\\:") != std::string::npos || id == "." || id == "..") continue;
            Item item;
            if (!loadItem(line / id, item)) {
                logger::warn("cosmetic {} could not be loaded", id);
                continue;
            }
            auto previous = std::find_if(list.begin(), list.end(), [&](const Item& old) { return old.id == item.id; });
            if (previous == list.end()) list.push_back(std::move(item));
            else {
                retire(previous->texture);
                *previous = std::move(item);
            }
        }
    }
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(root, ec)) {
        if (!e.is_directory() || find(e.path().filename().string())) continue;
        Item item;
        if (loadItem(e.path(), item) && !find(item.id)) list.push_back(std::move(item));
    }
}


}
