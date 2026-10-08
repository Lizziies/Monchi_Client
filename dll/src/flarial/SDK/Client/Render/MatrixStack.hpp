// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/SDK/Client/Render/MatrixStack.hpp: the deque is read through its real 1.26.52 layout instead of
// std::stack, and push/pop save and restore the top matrix instead of allocating, so a wrong stack pointer costs the
// effect and not the game.
#pragma once

#include <cstdint>

#include "Matrix.hpp"

class MatrixStack {
    // static, ItemInHandRenderer::renderItem (0x47b5ba0): deque fields at +0x08..+0x20, one matrix per block, the dirty
    // flag at +0x38; mce::Camera holds three of these 0x40 bytes apart
    uint64_t head;
    Matrix **map;
    uint64_t mapSize;
    uint64_t offset;
    uint64_t length;
    char filling[0x10];

    Matrix *current();

public:
    bool isDirty;

    // the world stack a BaseActorRenderContext draws with, null when the chain does not hold
    static MatrixStack *ofContext(void *context);

    bool valid();
    void push();
    void pop();

    Matrix &top();
};

static_assert(sizeof(MatrixStack) == 0x40);
