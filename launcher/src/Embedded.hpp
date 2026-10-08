#pragma once

#include <string>

namespace embedded {

bool present();
bool install(std::string& error);
// the cosmetics the launcher draws itself, unpacked when they are not on disk yet
void cosmetics();

}
