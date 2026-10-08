#pragma once

#include "modules/Module.hpp"

#include <imgui.h>

#include <string>

namespace gui {

enum class Page { Hub, Modules, Cosmetics, Settings };

std::string fitText(ImFont* font, float size, std::string text, float maxW);
void smoothScroll(bool top = false);
void beginScroll(const char* id, ImVec2 size, bool top = false);
void endScroll(const char* id);
void star(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool filled);

void go(Page p);
int& settingsTab();
void pollDevCommands();
Page page();
Module*& selectedModule();

char* searchText();
bool& favoritesOnly();

void profileDrive();
void drawModulesPage(ImVec2 origin, ImVec2 size);
void drawDetails(ImVec2 origin, ImVec2 size);
float detailsWidth();
void drawSettingsPage(ImVec2 origin, ImVec2 size);
void drawCosmeticsPage(ImVec2 origin, ImVec2 size);
// development only: "ids motion yaw" wears these, picks the motion and holds the big preview at that angle
void cosmeticsDev(const std::string& args);
void reloadCosmetics();

}
