#pragma once

#include "sdk/Game.hpp"
#include "sig/Sigs.hpp"

#include <string>
#include <vector>

namespace need {

constexpr unsigned player = unsigned(game::Domain::Player);
constexpr unsigned inventory = unsigned(game::Domain::Inventory);
constexpr unsigned effects = unsigned(game::Domain::Effects);
constexpr unsigned target = unsigned(game::Domain::Target);
constexpr unsigned world = unsigned(game::Domain::World);
constexpr unsigned combat = unsigned(game::Domain::Combat);
constexpr unsigned chat = unsigned(game::Domain::Chat);
constexpr unsigned board = unsigned(game::Domain::Scoreboard);
constexpr unsigned tab = unsigned(game::Domain::Tab);
constexpr unsigned camera = unsigned(game::Domain::Camera);
constexpr unsigned others = unsigned(game::Domain::Others);
constexpr unsigned light = unsigned(game::Domain::Light);
constexpr unsigned shots = unsigned(game::Domain::Others);

// pseudo signatures stand for game values the live reader does not deliver yet: PlayerStats, MoveState,
// WorldTime, Dimension, PlayerName, HurtEvents. They resolve once the reader fills those fields.
inline bool have(const char* name) { return game::demo() || sigs::address(name) != 0; }

inline std::vector<std::string> sigs(std::initializer_list<const char*> names) {
    std::vector<std::string> out;
    for (auto n : names) out.emplace_back(n);
    return out;
}

}
