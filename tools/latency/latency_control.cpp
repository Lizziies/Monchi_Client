#include "system/LatencyControl.hpp"
#include "system/FrameStats.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

using namespace gpuLatency;

static void check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

struct Fake : Driver {
    std::vector<Mode> requests;
    unsigned sleeps = 0;
    bool configureOk = true, sleepOk = true;
    int code = 0;
    bool configure(Mode mode) override { requests.push_back(mode); return configureOk; }
    bool pace() override { sleeps++; return sleepOk; }
    int error() const override { return code; }
};

int main() {
    timing::FrameStats frames;
    frames.add(-1);
    frames.add(std::numeric_limits<double>::quiet_NaN());
    frames.add(std::numeric_limits<double>::infinity());
    check(frames.summary().count == 0, "invalid frame intervals cannot pollute statistics");
    for (int i = 1; i <= 100; i++) frames.add(i);
    auto stats = frames.summary();
    check(stats.count == 100 && stats.mean == 50.5 && stats.p95 == 95 && stats.p99 == 99, "frame percentiles");
    for (int i = 0; i < 256; i++) frames.add(4);
    stats = frames.summary();
    check(stats.count == 256 && stats.mean == 4 && stats.worst == 4, "statistics use bounded recent window");
    frames.clear();
    check(frames.summary().count == 0, "statistics reset");
    Fake nvidia;
    Control control(nvidia, Vendor::Nvidia);
    check(control.set(Mode::On), "NVIDIA can use documented driver-only fallback");
    check(control.status().stage == Stage::DriverOnly, "fallback must not claim pre-input pacing");
    check(!control.beforeInput(1) && nvidia.sleeps == 0, "unverified frame boundary must never sleep");
    check(control.set(Mode::On) && nvidia.requests.size() == 1, "do not reconfigure every frame");
    control.bind(true);
    check(control.beforeInput(1), "verified frame should pace");
    check(!control.beforeInput(1) && !control.beforeInput(0), "repeated and zero frame IDs must not pace twice");
    check(control.beforeInput(3) && !control.beforeInput(2), "reject backwards frame IDs");
    check(nvidia.sleeps == 2 && control.status().pacedFrames == 2, "count accepted pacing calls");
    control.bind(false);
    check(control.mode() == Mode::Off && nvidia.requests.back() == Mode::Off, "lost hook disables pacing");
    check(!control.beforeInput(4), "lost hook must not sleep");

    Fake amd;
    Control antiLag(amd, Vendor::Amd);
    check(!antiLag.set(Mode::On) && amd.requests.empty(), "AMD must not enable without verified frame hook");
    antiLag.bind(true);
    check(antiLag.set(Mode::On) && antiLag.beforeInput(10), "AMD enables at verified boundary");
    amd.sleepOk = false;
    amd.code = -123;
    check(!antiLag.beforeInput(11), "pacing error must stop");
    check(antiLag.status().stage == Stage::Failed && antiLag.status().error == -123, "preserve driver failure");
    check(amd.requests.back() == Mode::Off, "disable backend after pacing error");
    check(!antiLag.beforeInput(12) && amd.sleeps == 2, "failed backend cannot keep sleeping");

    Fake reset;
    Control restore(reset, Vendor::Nvidia);
    check(restore.set(Mode::Boost), "boost request");
    reset.configureOk = false;
    reset.code = -456;
    check(!restore.set(Mode::Off) && restore.mode() == Mode::Boost, "failed reset must retain ownership");
    reset.configureOk = true;
    check(restore.set(Mode::Off) && restore.mode() == Mode::Off, "reset must be retryable");

    Fake unsupported;
    unsupported.code = -2;
    Control absent(unsupported, Vendor::Nvidia);
    check(absent.status().stage == Stage::Unsupported, "failed support probe must show unavailable");
    check(absent.set(Mode::Off) && unsupported.requests.empty(), "unsupported backend must not prevent unload when untouched");
    std::cout << "latency control checks passed\n";
}
