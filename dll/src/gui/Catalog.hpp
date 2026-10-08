#pragma once

#include "modules/Module.hpp"

#include <string>
#include <deque>
#include <vector>

namespace gui {

enum class Section { Pvp, Hud, Visual, Utility, Performance, Server, Extras };

constexpr int sectionCount = 7;

struct Entry {
    std::string name;
    std::string blurb;
    Section section = Section::Pvp;
    std::vector<Module*> members;
    bool group = false;

    bool on() const;
    void toggle() const;
    int enabled() const;
    int usable() const;
    bool hud() const;
    bool risky() const;
};

bool locked(const Module& m);
const char* sectionName(Section s);

const std::deque<Entry>& catalog();
const Entry* entryOf(const Module& m);

}
