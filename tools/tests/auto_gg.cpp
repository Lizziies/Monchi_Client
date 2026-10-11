#include "modules/server/ServerChat.hpp"
#include <cassert>

namespace game {
const State& state() {
    static State value;
    value.player.name = "PlayerOne";
    return value;
}
}

int main() {
    auto zeqa = srv::endWords("Zeqa");
    assert(srv::hasAny(srv::plain("You lost the duel!"), zeqa));
    assert(srv::hasAny(srv::plain("You have won the duel!"), zeqa));
    assert(srv::hasAny(srv::plain("Winner: PlayerOne"), zeqa));
    assert(!srv::typed("Winner: PlayerOne"));
    assert(srv::typed("PlayerOne: you lost the duel"));
    assert(srv::typed("<PlayerOne> victory"));
    assert(!srv::hasAny(srv::plain("Welcome to the lobby"), zeqa));
    assert(srv::hasAny(srv::plain("You have lost!"), srv::endWords("Unknown")));
}
