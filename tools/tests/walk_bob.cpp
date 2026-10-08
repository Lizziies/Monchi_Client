#include "flarial/Bridge/WalkBob.hpp"
#include <cassert>
#include <cmath>
#include <limits>
walkBob::Motion run(int fps, bool walking) {
    walkBob::Motion m;
    for (int i = 0; i < fps * 2; ++i) m.update(walking ? 4.3f / fps : 0.f, 1.f / fps);
    return m;
}
int main() {
    auto a = run(30, true), b = run(240, true);
    assert(std::abs(a.sideways() - b.sideways()) < 0.0001f);
    assert(std::abs(a.vertical() - b.vertical()) < 0.0001f);
    assert(std::abs(a.sideways()) + std::abs(a.vertical()) > 0.001f);
    for (int i = 0; i < 120; ++i) a.update(0.f, 1.f / 60);
    assert(a.strength < 0.0001f);
    b.update(20.f, 1.f / 60);
    assert(b.strength == 0.f);
    b.update(std::numeric_limits<float>::quiet_NaN(), 0.01f);
    assert(std::isfinite(b.sideways()));
}
