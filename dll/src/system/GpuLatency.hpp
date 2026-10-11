#pragma once

#include "LatencyControl.hpp"
#include <unknwn.h>
#include <string>

namespace gpuLatency {

struct Adapter {
    Vendor vendor = Vendor::Unknown;
    unsigned vendorId = 0;
    unsigned deviceId = 0;
    std::wstring name;
};

Adapter adapter(IUnknown* device);
void attach(IUnknown* device);
bool set(Mode mode);
void bindFrameStart(bool verified);
bool beforeInput(uint64_t frame);
Status status();
bool shutdown();

}
