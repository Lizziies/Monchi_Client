#pragma once

#include <cstdint>

namespace gpuLatency {

enum class Vendor { Unknown, Nvidia, Amd, Intel };
enum class Mode { Off, On, Boost };
enum class Stage { Unsupported, Off, DriverOnly, BeforeInput, Failed };

struct Status {
    Vendor vendor = Vendor::Unknown;
    Stage stage = Stage::Unsupported;
    int error = 0;
    bool frameStartVerified = false;
    uint64_t pacedFrames = 0;
};

class Driver {
public:
    virtual ~Driver() = default;
    virtual bool configure(Mode mode) = 0;
    virtual bool pace() = 0;
    virtual int error() const = 0;
};

class Control {
public:
    Control(Driver& driver, Vendor vendor) : driver_(driver) {
        status_.vendor = vendor;
        status_.error = driver.error();
        status_.stage = status_.error ? Stage::Unsupported : Stage::Off;
    }

    bool set(Mode mode) {
        if (mode == Mode::Off && mode_ == Mode::Off && status_.stage == Stage::Unsupported) return true;
        if (mode == mode_ && status_.stage != Stage::Failed) return status_.stage != Stage::Unsupported;
        if (mode != Mode::Off && status_.vendor == Vendor::Amd && !bound_) return false;
        if (!driver_.configure(mode)) {
            status_.stage = Stage::Failed;
            status_.error = driver_.error();
            return false;
        }
        mode_ = mode;
        status_.error = 0;
        status_.stage = mode == Mode::Off ? Stage::Off : bound_ ? Stage::BeforeInput : Stage::DriverOnly;
        return true;
    }

    void bind(bool verified) {
        if (bound_ == verified) return;
        bool stopped = true;
        if (!verified && mode_ != Mode::Off) stopped = set(Mode::Off);
        bound_ = verified;
        status_.frameStartVerified = verified;
        frame_ = 0;
        if (stopped && mode_ != Mode::Off && status_.stage != Stage::Failed)
            status_.stage = bound_ ? Stage::BeforeInput : Stage::DriverOnly;
    }

    bool beforeInput(uint64_t frame) {
        if (!bound_ || mode_ == Mode::Off || status_.stage == Stage::Failed || !frame || frame <= frame_) return false;
        frame_ = frame;
        if (!driver_.pace()) {
            status_.error = driver_.error();
            driver_.configure(Mode::Off);
            mode_ = Mode::Off;
            status_.stage = Stage::Failed;
            return false;
        }
        status_.pacedFrames++;
        return true;
    }

    // The same step in two halves, for a caller that must not hold its lock while the driver sleeps: wantsPace says
    // whether this frame is due, the caller then calls the driver itself and hands the result to paced.
    bool wantsPace(uint64_t frame) {
        if (!bound_ || mode_ == Mode::Off || status_.stage == Stage::Failed || !frame || frame <= frame_) return false;
        frame_ = frame;
        return true;
    }

    void paced(bool ok) {
        if (ok) {
            status_.pacedFrames++;
            return;
        }
        status_.error = driver_.error();
        driver_.configure(Mode::Off);
        mode_ = Mode::Off;
        status_.stage = Stage::Failed;
    }

    const Status& status() const { return status_; }
    Mode mode() const { return mode_; }

private:
    Driver& driver_;
    Status status_;
    Mode mode_ = Mode::Off;
    bool bound_ = false;
    uint64_t frame_ = 0;
};

}
