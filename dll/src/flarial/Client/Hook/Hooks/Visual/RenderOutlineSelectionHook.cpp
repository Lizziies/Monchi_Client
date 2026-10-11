// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/RenderOutlineSelectionHook.cpp, see the header.
#include "RenderOutlineSelectionHook.hpp"

namespace {

bool readPos(const int *pos, int *out) {
    __try {
        out[0] = pos[0];
        out[1] = pos[1];
        out[2] = pos[2];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

struct DrawContext {
    ScreenContext *saved = SDK::scn;
    explicit DrawContext(ScreenContext *scn) { if (scn) SDK::scn = scn; }
    ~DrawContext() { SDK::scn = saved; }
};

}

void RenderOutlineSelectionHook::OutlineSelectioCallback(LevelRendererPlayer *obj, ScreenContext *scn, void *block,
                                                         void *region, Vec3<int> *pos)
{
    int p[3];
    if (!pos || !readPos(reinterpret_cast<const int *>(pos), p)) return funcOriginal(obj, scn, block, region, pos);
    Vec3<float> at(static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]));

    bool cancelled;
    {
        // the call hands over the context the world is being drawn with; the module draws through SDK::scn
        DrawContext draw(scn);
        auto event = nes::make_holder<RenderOutlineSelectionEvent>(at, scn);
        eventMgr.trigger(event);
        cancelled = event->isCancelled();
    }

    if (!cancelled) funcOriginal(obj, scn, block, region, pos);
}

RenderOutlineSelectionHook::RenderOutlineSelectionHook()
    : Hook("RenderOutlineSelectionHook", Memory::offsetFromSig(GET_SIG_ADDRESS("LevelRendererPlayer::renderOutlineSelection"), 1)) {}

void RenderOutlineSelectionHook::enableHook()
{
    this->autoHook((void *) OutlineSelectioCallback, (void **) &funcOriginal);
}
