// SPDX-License-Identifier: AGPL-3.0-only
#include "NameStyles.hpp"

#include <windows.h>

#include <cctype>
#include <mutex>
#include <vector>

namespace {

struct Entry {
    std::string name;
    monchiNames::Look look;
};

std::mutex lock;
std::vector<Entry> entries;
ULONGLONG renewed = 0;

bool wordChar(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }

// the first line without the game's color codes, lower case
std::string plain(const std::string &tag) {
    std::string out;
    for (size_t i = 0; i < tag.size() && tag[i] != '\n'; i++) {
        if (static_cast<unsigned char>(tag[i]) == 0xC2 && i + 2 < tag.size() && static_cast<unsigned char>(tag[i + 1]) == 0xA7) {
            i += 2;
            continue;
        }
        out += char(std::tolower(static_cast<unsigned char>(tag[i])));
    }
    return out;
}

}

bool monchiNames::find(const std::string &tag, Look &out) {
    std::scoped_lock guard(lock);
    if (entries.empty() || GetTickCount64() - renewed > 2000) return false;
    std::string line = plain(tag);
    for (auto &e : entries) {
        // a rank in front of the name or a clan behind it is fine, a longer name that merely contains this one is not
        size_t at = line.find(e.name);
        if (at == std::string::npos) continue;
        size_t end = at + e.name.size();
        if ((at && wordChar(static_cast<unsigned char>(line[at - 1]))) || (end < line.size() && wordChar(static_cast<unsigned char>(line[end])))) continue;
        out = e.look;
        return true;
    }
    return false;
}

extern "C" __declspec(dllexport) void monchiFlarialNameStyles(const MonchiNameStyle *styles, int count) {
    std::vector<Entry> next;
    for (int i = 0; styles && i < count; i++) {
        if (!styles[i].name || !*styles[i].name) continue;
        Entry e;
        for (const char *c = styles[i].name; *c; c++) e.name += char(std::tolower(static_cast<unsigned char>(*c)));
        unsigned rgb = styles[i].color;
        e.look.color[0] = float((rgb >> 16) & 255) / 255.f;
        e.look.color[1] = float((rgb >> 8) & 255) / 255.f;
        e.look.color[2] = float(rgb & 255) / 255.f;
        if (styles[i].prefix) e.look.prefix = styles[i].prefix;
        next.push_back(std::move(e));
    }
    std::scoped_lock guard(lock);
    entries = std::move(next);
    renewed = GetTickCount64();
}
