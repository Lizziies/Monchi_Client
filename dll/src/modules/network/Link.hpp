#pragma once

#include "Probe.hpp"

#include <string>

namespace netlink {

probe::Link query(const std::string& targetIp);

}
