#include "ItemIcons.hpp"
#include "Hook.hpp"
#include "core/Log.hpp"
#include "core/Guard.hpp"
#include "sig/Sigs.hpp"

#include <atomic>

namespace itemIcons {

namespace {

// 1.26.52.3 ItemRenderer::renderGuiItemNew (0x55ac5b0): this, render context, item stack, mode, x, y, the glint pass
// flag, two floats the inventory renderer passes as 1.0, the size it computed for the slot, and an int. Its only
// caller (0x5568b00) calls it twice per slot, the second time with the glint flag set.
using Render = void (*)(void*, void*, void*, int, float, float, bool, float, float, float, int);

Render original = nullptr;
std::atomic<float> factor{1.f};
std::atomic<int> logged{0};
bool installed = false;
bool failed = false;

void render(void* self, void* ctx, void* stack, int mode, float x, float y, bool glint, float a, float b, float size, int flags) {
    if (!guard::call("ItemIconRender", [&] {
        if (logged.load(std::memory_order_relaxed) < 4) {
            logged.fetch_add(1, std::memory_order_relaxed);
            logger::info("item icon: mode {} at {:.1f},{:.1f} glint {} values {} {} size {} flags {}", mode, x, y, glint, a, b, size, flags);
        }
        float s = factor.load(std::memory_order_relaxed);
        if (s != 1.f) size *= s;
        original(self, ctx, stack, mode, x, y, glint, a, b, size, flags);
    })) factor.store(1.f, std::memory_order_relaxed);
}

}

void scale(float f) {
    factor.store(f, std::memory_order_relaxed);
    if (f == 1.f || installed || failed) return;
    auto at = sigs::address("ItemIconRender");
    if (!at) return;
    installed = hook::create("ItemIconRender", reinterpret_cast<void*>(at), render, &original) && hook::enableAll();
    failed = !installed;
}

}
