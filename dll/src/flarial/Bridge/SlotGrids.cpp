// SPDX-License-Identifier: AGPL-3.0-only
// Where the item grids of an open inventory sit on screen, for Monchi's overlays. Exported from MonchiFlarial.dll.
#include "Events/EventManager.hpp"
#include "Events/Listener.hpp"
#include "Events/Render/SetupAndRenderEvent.hpp"
#include "SDK/SDK.hpp"
#include "SDK/Client/Render/ScreenView/ScreenView.hpp"
#include "SDK/Client/Render/ScreenView/VisualTree.hpp"
#include "Utils/Render/PositionUtils.hpp"

#include <windows.h>

#include <array>
#include <cstring>
#include <memory>
#include <mutex>

namespace {

// read live on 1.26.52.3 with the inventory open: absolute position at +0x10 and size at +0x48 in GUI units, child
// controls in the vector at +0x98, the name at +0x20; one slot is 18 units, the item inside it 16
constexpr const char* names[] = {"hotbar_grid", "inventory_grid", "offhand_grid", "armor_grid"};
constexpr size_t gridCount = std::size(names);

struct Found {
    float rect[gridCount][4]{};
    bool any = false;
    // a screen being built or torn down can hand out stale child vectors; the walk stops instead of running away
    int budget = 4000;
};

bool nameIs(uintptr_t control, const char* want) {
    const auto* name = reinterpret_cast<const std::string*>(control + 0x20);
    size_t n = std::strlen(want);
    return name->size() == n && std::memcmp(name->data(), want, n) == 0;
}

void walk(uintptr_t control, int depth, Found& out) {
    if (!control || depth > 24 || --out.budget < 0) return;
    for (size_t i = 0; i < gridCount; i++) {
        if (out.rect[i][2] > 0.f || !nameIs(control, names[i])) continue;
        auto* pos = reinterpret_cast<const float*>(control + 0x10);
        auto* size = reinterpret_cast<const float*>(control + 0x48);
        auto a = PositionUtils::getScreenScaledPos(Vec2<float>(pos[0], pos[1]));
        auto b = PositionUtils::getScreenScaledPos(Vec2<float>(pos[0] + size[0], pos[1] + size[1]));
        out.rect[i][0] = a.x;
        out.rect[i][1] = a.y;
        out.rect[i][2] = b.x - a.x;
        out.rect[i][3] = b.y - a.y;
        out.any = true;
    }
    auto begin = *reinterpret_cast<const uintptr_t*>(control + 0x98);
    auto end = *reinterpret_cast<const uintptr_t*>(control + 0xa0);
    if (!begin || end < begin || end - begin > 16 * 512) return;
    for (uintptr_t it = begin; it < end && out.budget > 0; it += 16) walk(*reinterpret_cast<const uintptr_t*>(it), depth + 1, out);
}

// the hud draws every frame and has none of these grids, loading screens are rebuilt while they draw, and a server's
// form (a shop on Zeqa: hundreds of image buttons) has no item grid; only container screens are searched
bool searched(uintptr_t root) {
    if (nameIs(root, "hud_screen") || nameIs(root, "toast_screen")) return false;
    const auto* name = reinterpret_cast<const std::string*>(root + 0x20);
    for (const char* skip : {"loading", "progress", "server_form", "form"})
        if (name->find(skip) != std::string::npos) return false;
    return true;
}

bool scan(uintptr_t root, Found& out) {
    __try {
        if (!searched(root)) return false;
        walk(root, 0, out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

class Grids : public Listener {
public:
    Grids() { Listen(this, SetupAndRenderEvent, &Grids::onSetupAndRender) }
    ~Grids() { Deafen(this, SetupAndRenderEvent, &Grids::onSetupAndRender) }

    // The walk over an open inventory's tree costs a measurable part of a frame, so it runs when the screen opens and
    // every two seconds after that (the grids move only with a resize); the frames in between only check that the
    // same screen is still drawn.
    void onSetupAndRender(SetupAndRenderEvent&) {
        auto* view = SDK::screenView;
        if (!view || !view->VisualTree || !view->VisualTree->root) return;
        auto root = reinterpret_cast<uintptr_t>(view->VisualTree->root);
        uint64_t now = GetTickCount64();
        if (root == root_ && now - walkedAt_ < 2000) {
            std::lock_guard g(lock_);
            if (last_.any) at_ = now;
            return;
        }
        Found found;
        bool ok = scan(root, found);
        root_ = ok ? root : 0;
        walkedAt_ = now;
        std::lock_guard g(lock_);
        last_ = found;
        if (ok && found.any) at_ = now;
    }

    bool read(float* out) {
        std::lock_guard g(lock_);
        // a frame or two of slack: the grids stop at once when the screen closes
        if (!at_ || GetTickCount64() - at_ > 50) return false;
        std::memcpy(out, last_.rect, sizeof(last_.rect));
        return true;
    }

private:
    std::mutex lock_;
    Found last_;
    uint64_t at_ = 0;
    uintptr_t root_ = 0;
    uint64_t walkedAt_ = 0;
};

std::unique_ptr<Grids> grids;

}

extern "C" {

// hotbar, inventory, offhand and armor grid as x, y, width, height in pixels each (zero size when the screen has
// none); false while no inventory-like screen is open
__declspec(dllexport) bool monchiFlarialSlotGrids(float* out) {
    if (!grids) grids = std::make_unique<Grids>();
    return out && grids->read(out);
}

// called before the core stops
__declspec(dllexport) void monchiFlarialReleaseSlotGrids() { grids.reset(); }

}
