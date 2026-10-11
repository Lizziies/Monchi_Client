#pragma once

#include <imgui.h>

namespace fonts {

void load();

ImFont* regular();
ImFont* bold();
ImFont* hud();
float hudSize();
// whether the fonts carry this character; a plain range check, safe on any thread
bool covers(unsigned code);

}
