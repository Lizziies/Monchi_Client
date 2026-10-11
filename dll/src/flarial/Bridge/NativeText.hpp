// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>

// Text that only the game's own font can draw (a server's glyphs come from its resource pack, which is encrypted on
// disk) is handed over by Monchi and drawn by the game after its hud.
namespace monchiText {

// called from the screen hook on the game's thread: before a screen is drawn, for every text the game draws, and after
// the hud has been drawn
void begin(bool hud);
void seen(void *font, const std::string &text);
void serve(void *context, void *clientInstance, void *drawText);

}

extern "C" {
// position, width and line height in screen pixels, color as ImGui packs it (alpha in the top byte)
struct MonchiNativeLine {
    const char *text;
    float x, y, width, lineHeight;
    unsigned color;
};
// replaces the lines that are drawn from now on; they are dropped when nothing new arrives for a moment
using MonchiFlarialNativeText = void (*)(const MonchiNativeLine *lines, int count);
// how many rows the game needed for this text the last time it drew it, 0 when it has not been drawn yet
using MonchiFlarialNativeRows = int (*)(const char *text, float width, float lineHeight);
// how wide the game drew this text on one row at this line height, in screen pixels; 0 while it has not drawn it
using MonchiFlarialNativeWidth = float (*)(const char *text, float lineHeight);
// how wide the game drew this text on one row at this line height, in screen pixels; 0 while it has not drawn it
using MonchiFlarialNativeWidth = float (*)(const char *text, float lineHeight);
}
