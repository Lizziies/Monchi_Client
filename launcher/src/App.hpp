#pragma once

#include "Ui.hpp"

#include <windows.h>

namespace app {

void init(ui::State& state);
void handle(ui::State& state, const ui::Events& events, HWND window);
void sync(ui::State& state);
bool wantsQuit();
bool canClose();

}
