#pragma once

#include <functional>

namespace bg {

void run(std::function<void()> work);
bool drain(int timeoutMs);

}
