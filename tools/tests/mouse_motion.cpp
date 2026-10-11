#include "hook/MouseMotion.hpp"
#include <cstdlib>

void check(bool ok) { if (!ok) std::abort(); }

int main() {
    MouseMotion x, y;
    int64_t dx = 0, dy = 0;
    for (int i = 0; i < 100; i++) {
        dx += x.step(1, 0.25f, 1);
        dy += y.step(-1, 0.75f, 1);
    }
    check(dx == 25 && dy == -75);
    check(x.step(0, 1, 1) == 0 && y.step(0, 1, 1) == 0);
    MouseMotion smooth;
    int64_t sum = smooth.step(100, 1, 0.5);
    for (int i = 0; i < 5; i++) sum += smooth.step(0, 1, 0.5);
    sum += smooth.step(0, 1, 1);
    check(sum == 100);
    smooth.step(100, 1, 0.5);
    smooth = {};
    check(smooth.step(0, 1, 1) == 0);
}
