#pragma once

#include <windows.h>

namespace client {

void start(HMODULE self);
void requestUnload();
bool unloading();
HMODULE module();

}
