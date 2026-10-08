// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/SDK/Client/Render/MatrixStack.cpp, see MatrixStack.hpp.
#include "MatrixStack.hpp"

#include <vector>
#include <windows.h>

namespace {

struct Saved {
    MatrixStack *stack;
    bool live;
    Matrix matrix;
};

thread_local std::vector<Saved> saved;
thread_local Matrix spare;

}

Matrix *MatrixStack::current() {
    __try {
        if (!map || !length || !mapSize || (mapSize & (mapSize - 1)) || mapSize > 0x10000 || length > mapSize) return nullptr;
        Matrix *m = map[(offset + length - 1) & (mapSize - 1)];
        if (!m) return nullptr;
        volatile float probe = reinterpret_cast<float *>(m)[15];
        (void) probe;
        return m;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// static, ItemInHandRenderer::renderItem (0x47b66ad) and ItemRenderer::render (0x55aac1f): the context holds its
// ScreenContext at +0x28, that the mce::Camera at +0x18, whose world matrix stack sits at +0x40
MatrixStack *MatrixStack::ofContext(void *context) {
    __try {
        auto screen = *reinterpret_cast<uintptr_t *>(reinterpret_cast<uintptr_t>(context) + 0x28);
        if (!screen) return nullptr;
        auto camera = *reinterpret_cast<uintptr_t *>(screen + 0x18);
        if (!camera) return nullptr;
        auto *stack = reinterpret_cast<MatrixStack *>(camera + 0x40);
        return stack->valid() ? stack : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool MatrixStack::valid() {
    return current() != nullptr;
}

Matrix &MatrixStack::top() {
    auto *m = current();
    if (m) return *m;
    spare = Matrix{};
    return spare;
}

void MatrixStack::push() {
    auto *m = current();
    Saved s{this, m != nullptr, Matrix{}};
    if (m) {
        s.matrix = *m;
        isDirty = true;
    }
    saved.push_back(s);
}

void MatrixStack::pop() {
    if (saved.empty() || saved.back().stack != this) return;
    auto s = saved.back();
    saved.pop_back();
    if (!s.live) return;
    auto *m = current();
    if (!m) return;
    *m = s.matrix;
    isDirty = true;
}
