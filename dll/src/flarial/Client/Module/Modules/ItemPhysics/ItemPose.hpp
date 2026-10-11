// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/ItemPhysics/ItemPhysics.cpp: the matrix and rotation steps of Item Physics on their own, so
// they can be tested without the game.
#pragma once

#include <cmath>

#include <glm/glm/mat4x4.hpp>
#include <glm/glm/ext/matrix_transform.hpp>

namespace itemPose {

inline glm::vec3 sizeOf(const glm::mat4 &m) {
    return {glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2]))};
}

inline glm::mat4 at(glm::vec3 place, glm::vec3 size) {
    return glm::translate(glm::mat4(1.f), place) * glm::scale(glm::mat4(1.f), size);
}

// the game has moved to the item, bobbed it and spun it: keep the place and the size, drop the spin
inline void dropSpin(glm::mat4 &m) {
    m = at(glm::vec3(m[3]), sizeOf(m));
}

// a copy keeps the spread the game gave it around the group and lies at the height the item rests at; a hair of lift per
// copy keeps flat copies from flickering through each other
inline void settle(glm::mat4 &m, glm::vec3 ground, glm::vec3 group, int copy) {
    glm::vec3 place = ground + (glm::vec3(m[3]) - group);
    place.y = ground.y + 0.012f * float(copy);
    m = at(place, sizeOf(m));
}

// where a lying item comes to rest, in degrees
inline glm::vec3 lying(bool flat, bool eased) {
    return {90.f, eased ? (flat ? 180.f : 90.f) : 0.f, flat ? 0.f : 174.f};
}

inline void lay(glm::mat4 &m, glm::vec3 rotation, bool flat, float lift) {
    // world-space height adjustment, before the rotation so it stays vertical
    if (flat) m = glm::translate(m, {0.f, lift, 0.f});

    m = glm::rotate(m, glm::radians(rotation.x), {1.f, 0.f, 0.f});
    m = glm::rotate(m, glm::radians(rotation.y), {0.f, 1.f, 0.f});
    m = glm::rotate(m, glm::radians(rotation.z), {0.f, 0.f, 1.f});

    // shifts the model's pivot to its visual centre, so the laid-flat item stays over its shadow
    if (flat) m = glm::translate(m, {0.f, -0.5f, 0.f});
}

inline float wrap(float degrees) {
    return fmodf(fmodf(degrees, 360.f) + 360.f, 360.f);
}

// one frame of an item's own rotation: tumbling while it moves, easing toward lying() once it rests
inline void step(glm::vec3 &rotation, float spin, bool resting, bool flat, bool eased, bool keep, float speed, float delta) {
    if (!resting) {
        rotation.y = wrap(rotation.y + spin * delta * speed * 30.f);
        return;
    }
    if (!eased || keep) return;
    float ease = 1.f - std::exp(-10.f * delta);
    glm::vec3 target = lying(flat, true);
    rotation.y = wrap(rotation.y + (fmodf(target.y - rotation.y + 540.f, 360.f) - 180.f) * ease);
    rotation.z += (target.z - rotation.z) * ease;
}

}
