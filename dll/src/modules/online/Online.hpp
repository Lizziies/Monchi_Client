#pragma once

#include <imgui.h>

#include <cstdint>
#include <string>
#include <vector>

namespace online {

enum class Mode { Solid, Gradient, Rainbow, Pulse };

struct Style {
    Mode mode = Mode::Solid;
    uint32_t a = 0x3ba7ec;
    uint32_t b = 0xffffff;
    float speed = 1.f;
    uint32_t heartColor = 0x3ba7ec;
    std::string tag;
    uint32_t tagColor = 0x3ba7ec;
    bool heart = true;

    bool operator==(const Style&) const = default;
};

struct Worn {
    std::string id;
    std::vector<uint32_t> tint;

    bool operator==(const Worn&) const = default;
};

struct User {
    std::string name;
    Style style;
    std::vector<Worn> worn;
    std::string role;
};

struct Config {
    bool on = false;
    bool visible = true;
    bool demo = false;
    std::string url;
    std::string server;
};

enum class State { Off, NoUrl, Starting, Online, Demo, Offline, Claimed };

void tick(const Config& cfg, const std::vector<std::string>& names, const std::string& self);
void setStyle(const Style& style);
void setWorn(const std::vector<Worn>& worn);
Style style();
std::vector<Worn> worn();

bool find(const std::string& name, User& out);
std::vector<User> users();
int count();
State state();
std::string stateText();
void forget();
void shutdown();

ImU32 color(const Style& s, double t, int index, int total);
ImU32 rgb(uint32_t c, float alpha = 1.f);
std::string hex(uint32_t c);
uint32_t parseHex(const std::string& s, uint32_t fallback);
const char* modeId(Mode m);

void heartIcon(ImDrawList* dl, ImVec2 center, float size, ImU32 color);

template <class Draw>
float paint(const std::string& text, const Style& s, double t, Draw&& draw) {
    int total = 0;
    for (unsigned char c : text)
        if ((c & 0xC0) != 0x80) total++;
    float x = 0.f;
    int index = 0;
    for (size_t i = 0; i < text.size();) {
        size_t n = 1;
        unsigned char c = (unsigned char)text[i];
        if (c >= 0xF0) n = 4;
        else if (c >= 0xE0) n = 3;
        else if (c >= 0xC0) n = 2;
        x += draw(text.substr(i, n), color(s, t, index++, total), x);
        i += n;
    }
    return x;
}

std::string tagLine(const std::string& line, bool names, bool hearts);
// the game's color code closest to this color; its own text knows sixteen
const char* nearestCode(uint32_t rgb);
// For the game's own chat, on the game's thread: puts tag, heart and name color of a known user (or of oneself) into
// a line, either at the sender or where the name stands at the start of the text. True when the line was changed.
bool decorate(std::string& sender, std::string& body, bool names, bool hearts);
std::string badge(const std::string& role);

}
