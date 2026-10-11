#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace hive {

enum class Load { None, Loading, Ready, Missing, Failed };

struct Stats {
    double played = 0;
    double wins = 0;
    double kills = 0;
    double deaths = 0;
    double finalKills = 0;
    double beds = 0;
    double xp = 0;
    double prestige = 0;
    double streak = 0;
    long long firstPlayed = 0;

    float kd() const { return float(kills / std::max(1.0, deaths)); }
    float fkdr() const { return float(finalKills / std::max(1.0, deaths)); }
    float winRate() const { return played > 0 ? float(100.0 * wins / played) : 0.f; }
    double losses() const { return std::max(0.0, played - wins); }
};

struct Player {
    Load load = Load::None;
    Stats stats;
};

struct Row {
    int rank = 0;
    std::string name;
    double value = 0;
};

struct Board {
    Load load = Load::None;
    std::vector<Row> rows;
};

constexpr int gameCount = 13;
const char* gameName(int index);
const char* gameId(int index);
int gameFromTitle(const std::string& lowerTitle);

Player player(int game, const std::string& name, float maxAgeSec);
Board board(int game, bool monthly, int rows, float maxAgeSec);
Stats sample(const std::string& name);
Board sampleBoard(int rows);
void shutdown();

}
