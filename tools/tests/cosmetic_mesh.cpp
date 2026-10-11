#include "cosmetics/Cosmetics.hpp"
#include <cassert>
#include <cmath>

namespace cosmetics {
unsigned generation() { return 1; }
void retire(ImTextureData* texture) { if (texture) texture->WantDestroyNextFrame = true; }
}

int main() {
    cosmetics::Rig rig;
    assert(cosmetics::mesh({}, rig, true).empty());
    ImTextureData texture;
    cosmetics::Item item;
    item.texture = &texture;
    item.texW = item.texH = 64;
    item.tints.push_back({"Primary", {1, 0, 0, 1}});
    cosmetics::Bone bone;
    bone.cubes.push_back({{10, 10, 10}, {2, 2, 2}, {2, 2, 2}, 0, 0, 0});
    item.bones.push_back(bone);
    auto quads = cosmetics::mesh({{&item, {{0, 1, 0, 1}}}}, rig, true);
    assert(quads.size() == 6);
    for (const auto& q : quads) {
        assert(q.texture == &texture);
        assert(q.color.x == 0 && q.color.y == 1 && q.color.z == 0);
        for (int i = 0; i < 4; ++i) {
            assert(q.points[i].x >= 10 && q.points[i].x <= 12);
            assert(q.uv[i].x >= 0 && q.uv[i].x <= 1);
            assert(q.uv[i].y >= 0 && q.uv[i].y <= 1);
        }
    }
    item.bones[0].rotation.z = 90;
    cosmetics::Bone child;
    child.parent = 0;
    child.cubes = bone.cubes;
    item.bones.push_back(child);
    quads = cosmetics::mesh({{&item, {}}}, rig, false);
    assert(quads.size() == 12);
    for (size_t i = 6; i < quads.size(); ++i)
        for (auto p : quads[i].points) assert(p.x <= -9.99f && p.y >= 9.99f);
    item.bones.clear();
    cosmetics::Bone tubeBone;
    cosmetics::Tube tube;
    tube.sides = 8;
    tube.path.resize(2);
    tube.path[0].pos = {12, 12, 12};
    tube.path[1].pos = {12, 16, 12};
    tubeBone.tubes.push_back(tube);
    item.bones.push_back(tubeBone);
    quads = cosmetics::mesh({{&item, {}}}, rig, true);
    assert(!quads.empty());
    for (const auto& q : quads)
        for (auto p : q.points) assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
}
