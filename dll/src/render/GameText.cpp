#include "GameText.hpp"
#include "modules/common/Text.hpp"

#include <algorithm>
#include <imgui_internal.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gameText {

namespace {

struct Key {
    std::string text;
    ImU32 color;
};

struct View {
    std::string_view text;
    ImU32 color;
};

// looked up with a view of the text, so a hit copies nothing
struct KeyHash {
    using is_transparent = void;
    size_t operator()(const View& k) const { return std::hash<std::string_view>()(k.text) ^ (size_t(k.color) * 0x9E3779B97F4A7C15ull); }
    size_t operator()(const Key& k) const { return (*this)(View{k.text, k.color}); }
};

struct KeyEqual {
    using is_transparent = void;
    bool operator()(const Key& a, const Key& b) const { return a.color == b.color && a.text == b.text; }
    bool operator()(const View& a, const Key& b) const { return a.color == b.color && a.text == b.text; }
    bool operator()(const Key& a, const View& b) const { return a.color == b.color && a.text == b.text; }
};

const std::vector<text::Segment>& parsed(const std::string& value, ImU32 color) {
    static std::unordered_map<Key, std::vector<text::Segment>, KeyHash, KeyEqual> cache;
    auto it = cache.find(View{value, color});
    if (it != cache.end()) return it->second;
    if (cache.size() > 2048) cache.clear();
    return cache.emplace(Key{value, color}, text::colored(value, color)).first->second;
}

ImVec2 layout(ImDrawList* dl, ImFont* font, float height, ImVec2 at, ImU32 color, const std::string& value, float shadow) {
    ImVec2 cursor = at;
    float width = 0.f;
    auto run = [&](std::string_view text, ImU32 ink, bool bold, bool italic) {
        size_t from = 0;
        while (from < text.size()) {
            size_t to = text.find('\n', from);
            bool newline = to != std::string_view::npos;
            if (!newline) to = text.size();
            const char* begin = text.data() + from;
            const char* end = text.data() + to;
            float advance = font->CalcTextSizeA(height, FLT_MAX, 0.f, begin, end).x;
            float weight = bold ? height / 16.f : 0.f;
            if (dl && begin != end) {
                auto paint = [&](ImVec2 p, ImU32 c) {
                    int first = dl->VtxBuffer.Size;
                    dl->AddText(font, height, p, c, begin, end);
                    if (weight > 0.f) dl->AddText(font, height, p + ImVec2(weight, 0), c, begin, end);
                    if (italic)
                        for (int v = first; v < dl->VtxBuffer.Size; v++)
                            dl->VtxBuffer[v].pos.x += (p.y + height - dl->VtxBuffer[v].pos.y) * 0.2f;
                };
                if (shadow > 0.f) paint(cursor + ImVec2(shadow, shadow), IM_COL32(0, 0, 0, ((ink >> IM_COL32_A_SHIFT) & 255) * 140 / 255));
                paint(cursor, ink);
            }
            cursor.x += advance + (advance > 0.f ? weight : 0.f);
            width = std::max(width, cursor.x - at.x);
            if (newline) {
                cursor.x = at.x;
                cursor.y += height;
            }
            from = to + 1;
        }
    };
    // most HUD text carries no colour codes: it is laid out where it stands, nothing is parsed or copied
    if (value.find("\xC2\xA7") == std::string::npos) run(value, color, false, false);
    else
        for (const auto& part : parsed(value, color)) run(part.text, part.color, part.bold, part.italic);
    return {width, cursor.y - at.y + height};
}

}

bool canDraw(ImFont* font, const std::string& value) {
    if (!font) return false;
    auto plain = text::strip(value);
    const char* cursor = plain.data();
    const char* end = cursor + plain.size();
    while (cursor < end) {
        unsigned code = 0;
        int count = ImTextCharFromUtf8(&code, cursor, end);
        if (count <= 0) return false;
        cursor += count;
        if (code == '\n' || code == '\r' || code == '\t') continue;
        if (code > IM_UNICODE_CODEPOINT_MAX || !font->GetFontBaked(font->LegacySize)->FindGlyphNoFallback(ImWchar(code))) return false;
    }
    return true;
}

ImVec2 size(ImFont* font, float height, const std::string& value) {
    return layout(nullptr, font, height, {}, 0, value, 0.f);
}

ImVec2 draw(ImDrawList* dl, ImFont* font, float height, ImVec2 at, ImU32 color, const std::string& value, float shadow) {
    return layout(dl, font, height, at, color, value, shadow);
}

}
