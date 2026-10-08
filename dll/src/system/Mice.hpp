#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mice {

struct Device {
    std::string key;
    std::string name;
    std::string vendor;
    int hz = 0;
    bool active = false;
};

struct Snapshot {
    std::vector<Device> devices;
    std::vector<std::string> software;
};

void onRaw(void* device, int dx, int dy, int64_t qpc);
void use(bool on);
Snapshot snapshot();

void calibrateBegin();
long calibrateCounts();
std::string activeKey();

}
