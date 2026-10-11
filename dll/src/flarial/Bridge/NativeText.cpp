// SPDX-License-Identifier: AGPL-3.0-only
#include "NativeText.hpp"

#include "SDK/SDK.hpp"
#include "Utils/Logger/Logger.hpp"
#include "Utils/Memory/Game/SignatureAndOffsetManager.hpp"

#include <windows.h>

#include <cmath>
#include <format>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {

struct Line {
    std::string text;
    float x, y, width, lineHeight;
    unsigned color;
};

struct Piece {
    std::string text;
    float width;
    unsigned rgb;
};

std::mutex lock;
std::vector<Line> lines;
ULONGLONG submitted = 0;
std::unordered_map<std::string, int> rowsOf;
std::unordered_map<std::string, float> widthOf;

bool inHud = false;
void *firstFont = nullptr, *glyphFont = nullptr, *font = nullptr;
ULONGLONG fontSeen = 0;

// 1.26.52, static, vtable of the ui render context at 0xe9958b0:
// slot 1 (0x662fe50) measures: (context, font, std::string const&, float size, bool showColorSymbols) -> width in gui
//   units, it hands the font's own slot 6 a view of the string;
// slot 5 (0x6630020) only appends to the context's text list at +0x80 and takes the string's buffer with it, leaving
//   the caller's string empty. A buffer from this module's heap would later be freed by the game, so every string
//   handed over stays within the fifteen bytes that live inside the string object;
// slot 6 (0x6630150) draws the list: (context, float delta, std::optional<float>).
using Measure = float (*)(void *, void *, const std::string *, float, bool);
using Draw = void (*)(void *, void *, const RectangleArea *, std::string *, const float *, float, int, const TextMeasureData *,
                      const CaretMeasureData *);
using Flush = void (*)(void *, float, unsigned long long);

float measureRaw(void *context, void *with, const std::string *text) {
    __try {
        auto table = *reinterpret_cast<void ***>(context);
        return reinterpret_cast<Measure>(table[1])(context, with, text, 1.f, false);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1.f;
    }
}

bool drawRaw(void *drawText, void *context, void *with, const RectangleArea *rect, std::string *text, const float *rgb, float alpha,
             const TextMeasureData *measure, const CaretMeasureData *caret) {
    __try {
        reinterpret_cast<Draw>(drawText)(context, with, rect, text, rgb, alpha, int(ui::TextAlignment::LEFT), measure, caret);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool flushRaw(void *context) {
    __try {
        auto table = *reinterpret_cast<void ***>(context);
        reinterpret_cast<Flush>(table[6])(context, 0.f, 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

float scaleRaw(void *clientInstance, int guiData, int guiScale) {
    __try {
        auto gui = *reinterpret_cast<uintptr_t *>(reinterpret_cast<uintptr_t>(clientInstance) + guiData);
        return gui ? *reinterpret_cast<float *>(gui + guiScale) : 0.f;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0.f;
    }
}

float measured(void *context, const std::string &text) {
    static std::unordered_map<std::string, float> cache;
    static void *cachedFor = nullptr;
    if (cachedFor != font || cache.size() > 4096) {
        cache.clear();
        cachedFor = font;
    }
    auto it = cache.find(text);
    if (it != cache.end()) return it->second;
    float w = measureRaw(context, font, &text);
    if (!(w >= 0.f) || w > 64.f * float(text.size())) w = 6.f * float(text.size());
    cache.emplace(text, w);
    return w;
}

unsigned codeColor(char c, unsigned base) {
    static const unsigned table[16] = {0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xFFAA00, 0xAAAAAA,
                                       0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF};
    if (c >= '0' && c <= '9') return table[c - '0'];
    if (c >= 'a' && c <= 'f') return table[c - 'a' + 10];
    if (c == 'g') return 0xDDD605;
    if (c == 'r') return base;
    return ~0u;
}

std::string rowsKey(const std::string &text, float width, float lineHeight) {
    return std::format("{}\x01{}\x01{}", text, int(std::lround(width)), int(std::lround(lineHeight * 4.f)));
}

struct Pen {
    void *context, *drawText;
    float left, top, width, rowHeight, size, alpha;
    float x = 0.f;
    int row = 0;
    bool failed = false;

    void newRow() {
        x = 0.f;
        row++;
    }

    void put(Piece &piece) {
        if (x > 0.f && x + piece.width * size > width) newRow();
        RectangleArea rect(left + x, left + x + piece.width * size + 1.f, top + float(row) * rowHeight, top + float(row + 1) * rowHeight);
        float rgb[4] = {float((piece.rgb >> 16) & 255) / 255.f, float((piece.rgb >> 8) & 255) / 255.f, float(piece.rgb & 255) / 255.f, alpha};
        TextMeasureData measure(size, true, false);
        CaretMeasureData caret(-1, true);
        x += piece.width * size;
        if (!drawRaw(drawText, context, font, &rect, &piece.text, rgb, alpha, &measure, &caret)) failed = true;
    }
};

// words wrap as a whole, a word wider than the box breaks between its pieces
int layout(Pen &pen, const std::string &text, unsigned base) {
    std::vector<Piece> word;
    float wordWidth = 0.f;
    unsigned ink = base;
    float space = measured(pen.context, " ");
    if (space <= 0.f) space = 4.f;

    auto flushWord = [&] {
        if (word.empty()) return;
        if (pen.x > 0.f && pen.x + wordWidth * pen.size > pen.width) pen.newRow();
        for (auto &piece : word) pen.put(piece);
        word.clear();
        wordWidth = 0.f;
    };
    auto add = [&](const char *at, size_t n) {
        if (word.empty() || word.back().rgb != ink || word.back().text.size() + n > 15) word.push_back({{}, 0.f, ink});
        word.back().text.append(at, n);
    };
    auto close = [&] {
        wordWidth = 0.f;
        for (auto &piece : word) wordWidth += piece.width = measured(pen.context, piece.text);
        flushWord();
    };

    for (size_t at = 0; at < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[at]);
        size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        if (at + n > text.size()) break;
        if (c == 0xC2 && static_cast<unsigned char>(text[at + 1]) == 0xA7) {
            if (at + 2 < text.size()) {
                unsigned next = codeColor(text[at + 2], base);
                if (next != ~0u) ink = next;
            }
            at += 3;
            continue;
        }
        if (c == '\n') {
            close();
            pen.newRow();
        } else if (c == ' ' || c == '\t') {
            close();
            if (pen.x > 0.f) pen.x += space * pen.size;
        } else if (c >= 0x20) {
            add(text.data() + at, n);
        }
        at += n;
    }
    close();
    return pen.row + 1;
}

}

void monchiText::begin(bool hud) {
    inHud = hud;
    if (!hud) return;
    firstFont = nullptr;
    glyphFont = nullptr;
}

void monchiText::seen(void *with, const std::string &text) {
    if (!inHud || !with) return;
    if (!firstFont) firstFont = with;
    // a font that has just drawn a private use character is the one that knows the server's glyphs
    if (!glyphFont && text.find('\xEE') != std::string::npos) glyphFont = with;
}

void monchiText::serve(void *context, void *clientInstance, void *drawText) {
    inHud = false;
    ULONGLONG now = GetTickCount64();
    if (void *fresh = glyphFont ? glyphFont : firstFont) {
        font = fresh;
        fontSeen = now;
    }
    std::vector<Line> todo;
    {
        std::scoped_lock guard(lock);
        if (lines.empty()) return;
        if (now - submitted > 300) {
            lines.clear();
            return;
        }
        todo = lines;
    }
    static int told = 0;
    auto tell = [&](int state, const char *what) {
        if (told == state) return;
        told = state;
        Logger::info("native text: {}", what);
    };
    if (!context || !clientInstance || !drawText) return tell(1, "the game's text function is not hooked, nothing is drawn");
    // fonts are rebuilt when a server's pack loads, so only one the hud has used within the last few frames is trusted
    if (!font || now - fontSeen > 100) return tell(2, "the hud has drawn no text of its own, so no font is known yet");

    static const int guiData = GET_OFFSET("ClientInstance::guiData"), guiScale = GET_OFFSET("GuiData::GuiScale");
    float scale = guiData && guiScale ? scaleRaw(clientInstance, guiData, guiScale) : 0.f;
    if (!(scale >= 0.5f && scale <= 16.f)) return tell(3, "the gui scale could not be read, nothing is drawn");

    bool failed = false;
    std::vector<std::pair<std::string, int>> counted;
    std::vector<std::pair<std::string, float>> wide;
    for (auto &line : todo) {
        float rowHeight = line.lineHeight / scale;
        Pen pen{context, drawText, line.x / scale, line.y / scale, line.width / scale, rowHeight, rowHeight / 10.f,
                float(line.color >> 24) / 255.f};
        // ImGui packs red lowest, the table above has it highest
        unsigned base = ((line.color & 255) << 16) | (line.color & 0xFF00) | ((line.color >> 16) & 255);
        counted.emplace_back(rowsKey(line.text, line.width, line.lineHeight), layout(pen, line.text, base));
        if (pen.row == 0 && pen.x > 0.f) wide.emplace_back(rowsKey(line.text, 0.f, line.lineHeight), pen.x * scale);
        failed |= pen.failed;
    }
    if (!flushRaw(context)) failed = true;
    {
        std::scoped_lock guard(lock);
        if (rowsOf.size() > 1024) rowsOf.clear();
        for (auto &[key, rows] : counted) rowsOf[key] = rows;
        if (widthOf.size() > 2048) widthOf.clear();
        for (auto &[key, width] : wide) widthOf[key] = width;
    }
    tell(failed ? 4 : 5, failed ? "a call into the game's text drawing failed" : "lines are drawn with the game's own font");
}

extern "C" __declspec(dllexport) void monchiFlarialNativeText(const MonchiNativeLine *in, int count) {
    std::scoped_lock guard(lock);
    lines.clear();
    for (int i = 0; in && i < count; i++)
        if (in[i].text) lines.push_back({in[i].text, in[i].x, in[i].y, in[i].width, in[i].lineHeight, in[i].color});
    submitted = GetTickCount64();
}

extern "C" __declspec(dllexport) float monchiFlarialNativeWidth(const char *text, float lineHeight) {
    if (!text) return 0.f;
    std::scoped_lock guard(lock);
    auto it = widthOf.find(rowsKey(text, 0.f, lineHeight));
    return it == widthOf.end() ? 0.f : it->second;
}

extern "C" __declspec(dllexport) int monchiFlarialNativeRows(const char *text, float width, float lineHeight) {
    if (!text) return 0;
    std::scoped_lock guard(lock);
    auto it = rowsOf.find(rowsKey(text, width, lineHeight));
    return it == rowsOf.end() ? 0 : it->second;
}
