#pragma once

#include <string>

namespace notify {

enum class Kind { Info, Ok, Warn, Error };

void push(std::string title, std::string body, Kind kind = Kind::Info, float seconds = 4.f);
void draw();
void setMuted(bool muted);

}
