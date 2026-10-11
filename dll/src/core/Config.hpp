#pragma once

#include <json.hpp>

#include <string>
#include <vector>

namespace config {

void load();
void save();
void saveLater();
void saveIfDirty();
void tick();
void markDirty();
// safe from any thread: the save itself happens on the render thread within a second
void requestSave();
// what the active profile stored for a module; for modules that join after loading (the Flarial core's)
nlohmann::json stored(const std::string& module);

const std::string& profile();
std::vector<std::string> profiles();
void switchProfile(const std::string& name);
void deleteProfile(const std::string& name);

}
