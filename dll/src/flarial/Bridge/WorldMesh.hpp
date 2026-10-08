// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

// Monchi's cosmetics on players in the world: Monchi hands over finished quads in world coordinates, the game draws
// them itself while it renders the level, so walls and other players cover them like anything else.
namespace monchiWorld {

// Where in the frame the quads are drawn is Monchi's choice until the game has shown which place is right:
// 0 before the level (textures right, but the material writes no depth, so whatever the level draws later covers the
//   quads, the wearer's own body included),
// 1 with the name tags, which the game draws after the players with this very material,
// 2 after the level (comes out black).
// Called from the level render hook around the level, and from the name tag hook before every tag.
void draw(void *levelRenderer, void *screenContext, bool after);
void atNameTags(void *screenContext);

}

extern "C" {
struct MonchiWorldVertex {
    float x, y, z, u, v;
    // as ImGui packs it: red lowest, alpha in the top byte
    unsigned color;
};
// four vertices per quad; texture is the full path of a png on disk
struct MonchiWorldBatch {
    const char *texture;
    const MonchiWorldVertex *vertices;
    int count;
};
// replaces what is drawn from now on; dropped when nothing new arrives for a moment
using MonchiFlarialWorldMesh = void (*)(const MonchiWorldBatch *batches, int count);
// 0 nothing drawn yet, 1 the game draws the quads, 2 not possible on this version or switched off after a fault
using MonchiFlarialWorldMeshState = int (*)();
using MonchiFlarialWorldMeshPlace = void (*)(int place);
}
