#pragma once

#include <string>
#include <utility>
#include <vector>

// The particle effects the game creates by name ("minecraft:critical_hit_emitter", "minecraft:totem_particle", a
// server's own): which ones were asked for, and a list of names that are not created.
namespace particles {

// installs the hook the first time it is wanted; false when the game function is not known on this version
bool use(bool on);
bool hooked();
// names that are not created; with `all` no named effect is created
void block(std::vector<std::string> names, bool all);
// every name seen so far with how often it was asked for and how often it was kept from being created
struct Seen {
    std::string name;
    unsigned asked = 0;
    unsigned blocked = 0;
};
std::vector<Seen> seen();
unsigned blockedTotal();

}
