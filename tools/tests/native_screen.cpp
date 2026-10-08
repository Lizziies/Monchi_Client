#include "flarial/Bridge/ScreenState.hpp"
#include <cstdlib>

void check(bool ok) { if (!ok) std::abort(); }

int main() {
    nativeScreen::State s;
    check(s.read(1000) == 0);
    s.observe("hud_screen", 1000);
    s.observe("scoreboard_screen", 1001);
    s.observe("unknown_screen", 1002);
    check(s.read(1003) == 1);
    s.observe("pause_screen", 1100);
    for (unsigned long long t = 1101; t < 1200; t += 5) {
        s.observe("hud_screen", t);
        check(s.read(t) == 2);
    }
    s.observe("hud_screen", 1201);
    check(s.read(1201) == 1);
    s.observe("inventory_screen", 1300);
    check(s.read(1300) == 3);
    s.observe("chat_screen", 1500);
    check(s.read(1500) == 4);
    s.observe("settings_screen", 1700);
    check(s.read(1700) == 5);
    check(s.read(2701) == 0);
}
