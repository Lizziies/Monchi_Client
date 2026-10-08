// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/SDK/Client/Render/NameTagRenderObject.hpp: on 1.26.52 renderText (0x1f122d0) reads the string at +0,
// a mesh pointer at +0x20, the tag colour at +0x30 (copied into the screen context colour), the text colour at +0x40
// (handed to the font draw), the position at +0x50 and a scale at +0x5c; the object is 0x90 bytes (loop stride in 0x46ba370).
#pragma once

#include <Utils/Memory/Memory.hpp>

struct __declspec(align(8)) NameTagRenderObject {
    std::string nameTag;
    std::shared_ptr<uintptr_t> mesh;
    MCCColor tagColor;
    MCCColor textColor;
    Vec3<float> pos;
    float scale;
    uint8_t rest[0x90 - 0x60];
};

static_assert(offsetof(NameTagRenderObject, tagColor) == 0x30);
static_assert(offsetof(NameTagRenderObject, textColor) == 0x40);
static_assert(offsetof(NameTagRenderObject, pos) == 0x50);
static_assert(sizeof(NameTagRenderObject) == 0x90);
