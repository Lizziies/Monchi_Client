#pragma once

#include "Settings.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace ui {

constexpr float width = 960.f;
constexpr float height = 600.f;
constexpr float titleHeight = 40.f;
constexpr float controlsWidth = 96.f;

enum class Page { Start, Cosmetics, Versions, Settings, About };
enum class Phase { Idle, Updating, Starting, Waiting, Injecting, Done, Failed };

struct GameVersion {
    std::string name;
    bool preview = false;
    bool installed = false;
    bool supported = false;
    bool active = false;
    bool store = false;
    std::string path;
};

struct State {
    Page page = Page::Start;
    Phase phase = Phase::Idle;
    float progress = 0.f;
    std::string status;

    std::string clientVersion;
    std::string latestVersion;
    std::string gameVersion;
    std::string changelog;
    bool updatePrompt = false;
    bool updateKnown = false;
    bool updateAvailable = false;
    bool gameSupported = true;

    std::vector<GameVersion> versions;
    std::vector<GameVersion> downloads;
    bool downloadsOpen = false;
    bool versionBusy = false;
    bool versionInstalling = false;
    float versionProgress = 0.f;
    std::string versionStatus;
    Settings settings;
    int clientAccent = 0;
    bool managerInstalled = false;
    bool managerBusy = false;
    float managerProgress = 0.f;
    std::string managerStatus;
    char dllPath[260] = {};
};

struct Events {
    bool play = false;
    bool update = false;
    bool checkUpdate = false;
    bool dismissUpdate = false;
    bool minimize = false;
    bool close = false;
    bool browseDll = false;
    bool installManager = false;
    bool openManager = false;
    bool openLogs = false;
    bool openFolder = false;
    bool addFolder = false;
    bool rescan = false;
    bool loadVersions = false;
    bool cancelVersion = false;
    int installVersion = -1;
    int pick = -2;
    bool settingsChanged = false;
};

int nearestAccent(float r, float g, float b);
void setFonts(ImFont* regular, ImFont* bold);
void draw(State& state, Events& events);

}
