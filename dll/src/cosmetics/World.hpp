#pragma once

#include "modules/online/Online.hpp"

#include <vector>

// worn cosmetics on the players in the world: the own ones in third person, and those of other Monchi users
namespace cosmetics::world {

void update(bool self, bool others, const std::vector<online::Worn>& mine);
// test switch until the game has shown which way the legs swing
void flipLegs(bool flip);
void stop();
// 0 nothing drawn yet, 1 the game draws them, 2 not possible here
int state();

}
