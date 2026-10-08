#include "ItemIcons.hpp"
#include "Image.hpp"
#include "core/Bg.hpp"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <deque>

namespace itemicon {
namespace {
using Bytes = std::vector<uint8_t>;
std::unordered_map<std::string, Bytes> files;
std::unordered_map<std::string, std::shared_ptr<const img::Pixels>> icons;
std::mutex cacheLock;
std::deque<std::string> pending;
bool busy = false;
bool loaded = false;

uint32_t number(const Bytes& data, size_t at) {
    uint32_t value = 0;
    std::memcpy(&value, data.data() + at, sizeof(value));
    return value;
}

void archive(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return;
    auto length = f.tellg();
    if (length < 16 || length > 64 * 1024 * 1024) return;
    Bytes data(static_cast<size_t>(length));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size()))) return;
    if (number(data, 0) != 0xb125277d || number(data, 4) != 0x267052a0 || number(data, 12) != 1) return;
    size_t count = number(data, 8);
    if (count > (data.size() - 16) / 256) return;
    size_t payload = 16 + count * 256;
    // Bedrock archives have 256-byte entries, with payload-relative offset and length in the last eight bytes.
    for (size_t i = 0; i < count; ++i) {
        size_t entry = 16 + i * 256;
        size_t nameLength = data[entry];
        if (!nameLength || nameLength > 247) continue;
        std::string name(reinterpret_cast<char*>(data.data() + entry + 1), nameLength);
        size_t offset = number(data, entry + 248), size = number(data, entry + 252);
        if (offset > data.size() - payload || size > data.size() - payload - offset) continue;
        if (!name.ends_with(".png") && !name.ends_with(".tga")) continue;
        files.try_emplace(name, data.begin() + payload + offset, data.begin() + payload + offset + size);
    }
}

void load() {
    loaded = true;
    wchar_t exe[32768]{};
    DWORD length = GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
    if (!length || length >= std::size(exe)) return;
    auto root = std::filesystem::path(exe).parent_path() / L"data/resource_packs";
    archive(root / L"vanilla/__brarchive/textures/items.brarchive");
    archive(root / L"vanilla/__brarchive/textures/blocks.brarchive");
    std::error_code error;
    for (const auto& pack : std::filesystem::directory_iterator(root, error)) {
        if (!pack.path().filename().wstring().starts_with(L"vanilla_")) continue;
        archive(pack.path() / L"__brarchive/textures/items.brarchive");
        archive(pack.path() / L"__brarchive/textures/blocks.brarchive");
    }
}

bool tga(const Bytes& data, img::Pixels& out) {
    if (data.size() < 18 || data[1] != 0 || (data[2] != 2 && data[2] != 10)) return false;
    int w = data[12] | data[13] << 8, h = data[14] | data[15] << 8;
    int stride = data[16] / 8;
    if (w < 1 || h < 1 || w > 64 || h > 64 || (stride != 3 && stride != 4)) return false;
    out = {w, h, std::vector<uint32_t>(size_t(w) * h)};
    size_t at = 18 + data[0], index = 0;
    auto pixel = [&](uint32_t& color) {
        if (at > data.size() || size_t(stride) > data.size() - at) return false;
        color = IM_COL32(data[at + 2], data[at + 1], data[at], stride == 4 ? data[at + 3] : 255);
        at += stride;
        return true;
    };
    auto put = [&](uint32_t color) {
        size_t x = index % w, y = index / w;
        if (!(data[17] & 0x20)) y = h - 1 - y;
        if (data[17] & 0x10) x = w - 1 - x;
        out.rgba[y * w + x] = color;
        ++index;
    };
    while (index < out.rgba.size()) {
        unsigned packet = 0;
        size_t count = 1;
        if (data[2] == 10) {
            if (at >= data.size()) return false;
            packet = data[at++];
            count = (packet & 127) + 1;
        }
        if (count > out.rgba.size() - index) return false;
        uint32_t color = 0;
        if (packet & 128) {
            if (!pixel(color)) return false;
            while (count--) put(color);
        } else while (count--) {
            if (!pixel(color)) return false;
            put(color);
        }
    }
    return true;
}

img::Pixels decode(std::string name) {
    if (!loaded) load();
    if (auto at = name.find(':'); at != std::string::npos) name.erase(0, at + 1);
    img::Pixels result;
    if (name.starts_with("golden_")) name.replace(0, 7, "gold_");
    if (name.starts_with("wooden_")) name.replace(0, 7, "wood_");
    if (name == "bow" || name == "crossbow") name += "_standby";
    if (name == "totem_of_undying") name = "totem";
    for (const char* extension : {".png", ".tga"}) {
        auto it = files.find(name + extension);
        if (it == files.end()) continue;
        if (it->first.ends_with(".tga")) { if (!tga(it->second, result)) result = {}; }
        else img::load(it->second, 64, result);
        if (!result.rgba.empty()) break;
    }
    return result;
}

std::shared_ptr<const img::Pixels> icon(const std::string& name) {
    bool start = false;
    {
        std::scoped_lock lock(cacheLock);
        auto [it, inserted] = icons.try_emplace(name, nullptr);
        if (!inserted) return it->second;
        pending.push_back(name);
        if (!busy) busy = start = true;
    }
    if (start) bg::run([] {
        for (;;) {
            std::string name;
            {
                std::scoped_lock lock(cacheLock);
                if (pending.empty()) { busy = false; return; }
                name = std::move(pending.front());
                pending.pop_front();
            }
            auto image = std::make_shared<img::Pixels>(decode(name));
            std::scoped_lock lock(cacheLock);
            icons[name] = std::move(image);
        }
    });
    return {};
}
}

bool draw(ImDrawList* dl, ImVec2 at, float size, const std::string& name) {
    auto cached = icon(name);
    if (!cached || cached->rgba.empty()) return false;
    const auto& image = *cached;
    float pixel = size / float(std::max(image.w, image.h));
    at += (ImVec2(size, size) - ImVec2(float(image.w), float(image.h)) * pixel) * 0.5f;
    for (int y = 0; y < image.h; ++y) {
        for (int x = 0; x < image.w;) {
            ImU32 color = image.rgba[size_t(y) * image.w + x];
            int end = x + 1;
            while (end < image.w && image.rgba[size_t(y) * image.w + end] == color) ++end;
            if (color & IM_COL32_A_MASK)
                dl->AddRectFilled(at + ImVec2(float(x), float(y)) * pixel,
                                  at + ImVec2(float(end), float(y + 1)) * pixel, color);
            x = end;
        }
    }
    return true;
}
}
