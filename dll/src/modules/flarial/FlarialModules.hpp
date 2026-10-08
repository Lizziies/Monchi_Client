#pragma once

#include "core/Events.hpp"

#include <string>
#include <vector>

// Flarial's modules in Monchi's own registry: every module of the Flarial core that Monchi does not already have
// becomes a stand-in module with Monchi's switch, key, favorite and settings widgets; one Monchi already has stays
// switched off in the core so nothing runs twice. Runs on the render thread.
class Module;

namespace flarialModules {

// adopts the core's modules once it runs, then keeps switches and server blocks in step every frame
void sync();
// 0 unavailable, 1 gameplay, 2 pause, 3 inventory, 4 chat, 5 other menus.
int screen();
bool hotbar(float* rect);
// an open inventory's hotbar, inventory, offhand and armor grids as 4 x {x, y, width, height} pixels (zero size when
// absent); false while no such screen is open
bool slotGrids(float* rects);
bool sneak(int state);
bool sneakApplied();
unsigned attacks(uintptr_t* out, unsigned capacity);
bool zoom(float factor);
bool perspective(int view);
unsigned perspectiveCalls(bool changed = false);
bool hideCrosshair(bool hide);
bool hideHud(bool hide);
// a key or focus message from the game window for Flarial's modules; true when one of them consumed it. Called on
// the window thread; Monchi's own modules and menu have already seen the message.
bool key(void* hwnd, unsigned msg, unsigned long long wp, long long lp);
// a mouse button or wheel event for Flarial's modules; true when one of them cancelled it. Called on the window thread.
bool mouse(const MouseEvent& ev);
// the game's own chat is moved out of the picture while Monchi's Better Chat replaces it; no-op without the core
void hideChat(bool hide);
void privateChat(bool hide);
void hideScoreboard(bool hide);
// asks the game, on its own thread, to compute its screen size and gui scale again; false without the core
bool refreshScreen();
// for the module check: empty for a module of Monchi's own, otherwise what the core says about its module
std::string proof(const Module& m);
// text with characters only the game's font has, drawn by the game after its hud: position, width and line height in
// pixels. The set stays until the next call and has to be renewed every frame. False without the core.
struct NativeLine {
    std::string text;
    float x, y, width, lineHeight;
    unsigned color;
};
bool nativeText(const std::vector<NativeLine>& lines);
bool nativeTextReady();
// quads in world coordinates that the game draws with the level, four vertices each, grouped by the png they use.
// The set has to be renewed every frame. State: 0 nothing drawn yet, 1 drawn, 2 not possible.
struct WorldVertex {
    float x, y, z, u, v;
    unsigned color;
};
struct WorldBatch {
    std::string texture;
    std::vector<WorldVertex> vertices;
};
// how other Monchi users' names look above their heads: color as 0xRRGGBB, prefix in front of the name
struct NameStyle {
    std::string name;
    unsigned color;
    std::string prefix;
};
void nameStyles(const std::vector<NameStyle>& styles);
bool worldMeshReady();
void worldMesh(const std::vector<WorldBatch>& batches);
int worldMeshState();
// where in the frame the quads are drawn: 0 before the level, 1 with the name tags, 2 after the level
void worldMeshPlace(int place);
// rows the game needed for this text when it last drew it, 0 before the first time
int nativeRows(const std::string& text, float width, float lineHeight);
// width in pixels of a text the game's font has drawn on one row at this line height, 0 while not known
float nativeWidth(const std::string& text, float lineHeight);
// lets go of the core before it stops; the stand-ins stay registered but do nothing
void shutdown();

}
