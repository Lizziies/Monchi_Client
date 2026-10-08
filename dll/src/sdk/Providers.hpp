#pragma once

#include "Game.hpp"

#include <memory>

namespace game {

std::unique_ptr<Provider> makeDemo();
std::unique_ptr<Provider> makeLive();

}
