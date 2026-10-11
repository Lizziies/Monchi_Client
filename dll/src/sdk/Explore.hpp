#pragma once

#include <string>

namespace explore {

// development builds only (-DMONCHI_DEV=ON): runs a script from the explore folder next to the dll, see tools/explore
void run(const std::string& name);
// ends a running script (also before unloading); "explore stop" does the same
void stop();

}
