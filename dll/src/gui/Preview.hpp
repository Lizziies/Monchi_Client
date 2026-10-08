#pragma once

class Module;

// The live picture in a module's settings: the game as it looks right now with the HUD on it, small, so a change shows
// without closing the menu.
namespace gui::preview {

// called once the HUD is in the background draw list and before the menu adds to it
void mark();
void draw(Module& m);

}
