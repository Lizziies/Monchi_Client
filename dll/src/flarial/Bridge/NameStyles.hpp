// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>

// Monchi users get their tag as a line above the name the game draws over their head; the name itself stays as the
// game draws it. Monchi keeps the list current; the name tag hook asks for every tag it draws.
namespace monchiNames {

struct Look {
    float color[3];
    std::string prefix;
};

// true when the tag's first line carries the name of a listed user
bool find(const std::string &tag, Look &out);

}

extern "C" {
// color as 0xRRGGBB; prefix is the tag text drawn above the name in that color
struct MonchiNameStyle {
    const char *name;
    unsigned color;
    const char *prefix;
};
// replaces the list; it is dropped when nothing new arrives for two seconds
using MonchiFlarialNameStyles = void (*)(const MonchiNameStyle *styles, int count);
}
