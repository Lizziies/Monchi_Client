#include "system/PaceGate.hpp"
#include <cassert>

struct TestDriver : gpuLatency::Driver {
    bool configureOk = true, paceOk = true;
    int calls = 0;
    bool configure(gpuLatency::Mode) override { return configureOk; }
    bool pace() override { calls++; return paceOk; }
    int error() const override { return 1; }
};

int main() {
    using namespace gpuLatency;
    TestDriver driver;
    Control control(driver, Vendor::Nvidia);
    PaceGate gate;
    assert(!gate.ready());
    gate.update(&control);
    assert(!gate.ready());
    assert(control.set(Mode::On));
    gate.update(&control);
    assert(!gate.ready());
    control.bind(true);
    gate.update(&control);
    assert(gate.ready());
    assert(control.beforeInput(1));
    assert(!control.beforeInput(1));
    assert(driver.calls == 1);
    assert(control.set(Mode::Off));
    gate.update(&control);
    assert(!gate.ready());
    assert(control.set(Mode::Boost));
    gate.update(&control);
    assert(gate.ready());
    control.bind(false);
    gate.update(&control);
    assert(!gate.ready());
    control.bind(true);
    assert(control.set(Mode::On));
    driver.paceOk = false;
    assert(!control.beforeInput(2));
    gate.update(&control);
    assert(!gate.ready());
    driver.configureOk = false;
    assert(!control.set(Mode::Boost));
    gate.update(&control);
    assert(!gate.ready());
    gate.update(nullptr);
    assert(!gate.ready());

    driver.configureOk = true;
    assert(control.set(Mode::On));
    driver.configureOk = false;
    control.bind(false);
    gate.update(&control);
    assert(!gate.ready());
    control.bind(true);
    gate.update(&control);
    assert(control.status().stage == Stage::Failed);
    assert(!gate.ready());
}
