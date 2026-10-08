#pragma once

#include <imgui.h>

#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;

// The player figure of the launcher: the skin the client saw last in the game and the cosmetics that are switched on,
// drawn by the same code as the client's own cosmetics page. No account and no name is involved.
namespace look {

void init(ID3D11Device* device, ID3D11DeviceContext* context);
void stop();

struct Entry {
    std::string id, name, slot;
    bool on = false;
};

void figure(ImDrawList* dl, const char* id, ImVec2 min, ImVec2 max, ImVec4 body);
const std::vector<Entry>& entries();
void toggle(const std::string& id);
bool hasSkin();
// which of the two built-in figures stands in while the player's own skin is not known (0 boy, 1 girl)
void standIn(int which);
bool usingStandIn();
// the skin was taken from the game's own files, before the client ever saw it in a game
bool skinFromGameFiles();
// the picked skin is one the game keeps encrypted (Marketplace, character creator): it shows after a game
bool skinLocked();
// the game is running: the client owns the settings then and would overwrite a change made here
bool gameOwnsSettings();
// no settings yet: the client writes them the first time it runs
bool settingsMissing();

}
