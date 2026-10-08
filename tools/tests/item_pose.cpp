// Item Physics against the matrix steps the game takes for a dropped item (ItemRenderer::render 0x55aaa60, the group
// function 0x55aa4c0): whatever the bob and the spin of the frame are, a resting item must come out at the same place, lying.
#include "flarial/Client/Module/Modules/ItemPhysics/ItemPose.hpp"

#include <cstdio>
#include <cstdlib>

static int failed = 0;

static void check(bool ok, const char *what) {
    if (ok) return;
    std::printf("FAILED: %s\n", what);
    failed++;
}

static bool near(glm::vec3 a, glm::vec3 b, float eps = 1e-4f) {
    return glm::length(a - b) < eps;
}

struct Drawn {
    glm::mat4 copy[4];
};

// the spread of the copies: the game's table holds one offset per copy, scaled by 0.2 over the size
static const glm::vec3 spread[4] = {{0, 0, 0}, {0.4f, -0.3f, 0.2f}, {-0.5f, 0.2f, 0.35f}, {0.1f, 0.45f, -0.4f}};

static Drawn draw(glm::mat4 base, glm::vec3 position, float bob, float spin, float size, bool flat, glm::vec3 rotation, int copies) {
    glm::vec3 ground = glm::vec3(base * glm::vec4(position, 1.f));

    glm::mat4 m = glm::translate(base, position);
    m = glm::translate(m, {0.f, bob, 0.f});
    m = glm::rotate(m, spin, {0.f, 1.f, 0.f});
    m = glm::scale(m, glm::vec3(size));

    glm::vec3 group = glm::vec3(m[3]);
    itemPose::dropSpin(m);

    Drawn out{};
    for (int i = 0; i < copies; i++) {
        glm::mat4 c = m;
        if (i) c = glm::translate(c, spread[i] * (0.2f / size));
        c = glm::scale(c, glm::vec3(1.f));
        itemPose::settle(c, ground, group, i);
        itemPose::lay(c, rotation, flat, 0.3f);
        out.copy[i] = c;
    }
    return out;
}

int main() {
    glm::vec3 position{3.25f, -1.5f, -7.f};
    glm::vec3 rest = itemPose::lying(true, true);

    Drawn a = draw(glm::mat4(1.f), position, 0.02f, 0.3f, 0.3f, true, rest, 4);
    Drawn b = draw(glm::mat4(1.f), position, 0.19f, 4.1f, 0.3f, true, rest, 4);
    for (int i = 0; i < 4; i++) {
        bool same = true;
        for (int c = 0; c < 4; c++) same = same && near(glm::vec3(a.copy[i][c]), glm::vec3(b.copy[i][c]));
        check(same, "a resting copy does not depend on the bob and the spin of the frame");
    }

    // the flat model's face normal is its z axis; lying means it points along the world's y axis
    glm::vec3 normal = glm::normalize(glm::vec3(a.copy[0] * glm::vec4(0.f, 0.f, 1.f, 0.f)));
    check(std::abs(std::abs(normal.y) - 1.f) < 1e-4f, "a resting flat item lies in the ground plane");

    // the pivot shift moves the model's middle (0, 0.5, 0) onto the item's place, lifted by the offset setting
    glm::vec3 middle = glm::vec3(a.copy[0] * glm::vec4(0.f, 0.5f, 0.f, 1.f));
    check(near(middle, position + glm::vec3(0.f, 0.3f * 0.3f, 0.f)), "the middle of a flat item sits over the item's place");

    for (int i = 1; i < 4; i++) {
        glm::vec3 m0 = glm::vec3(a.copy[0] * glm::vec4(0.f, 0.5f, 0.f, 1.f));
        glm::vec3 mi = glm::vec3(a.copy[i] * glm::vec4(0.f, 0.5f, 0.f, 1.f));
        check(std::abs((mi.y - m0.y) - 0.012f * float(i)) < 1e-4f, "each copy lies a hair above the one before");
        check(near({mi.x - m0.x, 0.f, mi.z - m0.z}, {spread[i].x * 0.2f, 0.f, spread[i].z * 0.2f}), "a copy keeps the game's spread on the ground");
    }

    // a block goes through the other path: no pivot shift, no offset, the cube's middle is the item's place
    Drawn cube = draw(glm::mat4(1.f), position, 0.11f, 2.f, 0.25f, false, itemPose::lying(false, true), 1);
    check(near(glm::vec3(cube.copy[0][3]), position), "a block rests with its middle on the item's place");
    check(near(itemPose::sizeOf(cube.copy[0]), glm::vec3(0.25f)), "the size the game chose stays");

    // a stack that does not start at the origin
    glm::mat4 base = glm::translate(glm::mat4(1.f), {10.f, 5.f, -2.f});
    Drawn moved = draw(base, position, 0.07f, 1.f, 0.3f, true, rest, 1);
    glm::vec3 there = glm::vec3(moved.copy[0] * glm::vec4(0.f, 0.5f, 0.f, 1.f));
    check(near(there, position + glm::vec3(10.f, 5.f + 0.09f, -2.f)), "the place follows the matrix the game started from");

    // rotation over time
    glm::vec3 r{90.f, 350.f, 0.f};
    itemPose::step(r, 1.f, false, true, true, false, 8.f, 0.1f);
    check(std::abs(r.y - 14.f) < 1e-3f, "a moving item tumbles by speed * 30 degrees a second and wraps");
    itemPose::step(r, -1.f, false, true, true, false, 8.f, 0.1f);
    check(std::abs(r.y - 350.f) < 1e-3f, "the other direction wraps back");

    r = {90.f, 10.f, 0.f};
    for (int i = 0; i < 240; i++) itemPose::step(r, 1.f, true, true, true, false, 8.f, 1.f / 120.f);
    check(std::abs(r.y - 180.f) < 0.5f && std::abs(r.z) < 0.5f, "a resting flat item eases to lying");

    r = {90.f, 300.f, 0.f};
    for (int i = 0; i < 240; i++) itemPose::step(r, 1.f, true, false, true, false, 8.f, 1.f / 120.f);
    check(std::abs(r.y - 90.f) < 0.5f && std::abs(r.z - 174.f) < 0.5f, "a resting block eases to its own rest");

    r = {90.f, 123.f, 7.f};
    itemPose::step(r, 1.f, true, true, true, true, 8.f, 0.016f);
    check(r == glm::vec3(90.f, 123.f, 7.f), "keeping rotations leaves a resting item as it landed");
    itemPose::step(r, 1.f, true, true, false, false, 8.f, 0.016f);
    check(r == glm::vec3(90.f, 123.f, 7.f), "without easing the stored rotation is not touched");

    r = {90.f, 45.f, 0.f};
    itemPose::step(r, 1.f, false, true, true, false, 8.f, 0.f);
    itemPose::step(r, 1.f, true, true, true, false, 8.f, 0.f);
    check(std::isfinite(r.y) && std::isfinite(r.z) && r.y == 45.f, "a frame without time changes nothing");

    if (failed) return 1;
    std::printf("ok\n");
    return 0;
}
