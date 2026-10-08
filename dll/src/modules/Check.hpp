#pragma once

// Writes logs/module-check.txt: for every module that is on, what proves that it reaches the game. A module that
// changes the game does so through a hooked game function; the file says whether that function was found, whether
// the game ran it and how often the module's value replaced the game's own.
namespace check {

void tick();
void write();

}
