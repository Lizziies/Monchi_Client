#pragma once

#include <algorithm>
#include <cstdint>

// Where a frame of the game starts, told from the rhythm of its input polls. Kept free of Windows and of the hooks so
// the rules can be tested: tools/tests/frame_start.cpp.
namespace framestart {

// The polls of one frame (keyboard, mouse, pads) come within microseconds of each other; the first one after a pause
// opens the next frame. That poll counts as the frame start once the same thread has done it once per presented frame
// for a few hundred frames in a row. A polling thread of its own, or polls that do not follow the frames, never get there.
class Detector {
public:
    // true when this poll opens a new frame; present is any value that changes with every Present
    bool poll(int64_t now, int64_t frequency, unsigned long thread, int64_t present) {
        if (resumeThread_) {
            if (thread != resumeThread_) *this = {};
            resumeThread_ = 0;
        }
        // A short stall (a hitch, a chunk load) is not a new rhythm: the count is kept, only the gap is forgotten.
        // Otherwise the pacing is given up and learnt again every few seconds, moving the wait from in front of the
        // input to behind Present and back, and the aim feels uneven.
        if (lastPoll_ && now - lastPoll_ > frequency * 2) *this = {};
        else if (lastPoll_ && now - lastPoll_ > frequency / 4) lastPoll_ = 0;
        bool start = !lastPoll_ || now - lastPoll_ > frequency / burstPerSecond;
        lastPoll_ = now;
        polls_++;
        if (!start) return false;
        bool newFrame = !thread_ || present != present_;
        bool regular = thread == thread_ && newFrame && polls_ <= maxPolls;
        // capped just above the mark, so a rhythm that really breaks is given up within a few dozen frames, while a
        // frame without a new Present now and then (the game polls twice when a frame runs long) costs little
        steady_ = regular ? std::min(steady_ + 1, need + 60) : std::max(0, steady_ - penalty);
        lastPolls_ = polls_;
        polls_ = 0;
        thread_ = thread;
        present_ = present;
        verified_ = verified_ ? steady_ >= keep : steady_ >= need;
        return newFrame;
    }

    // a wait in front of the input must not look like the pause between two frames
    void resume(int64_t now) { lastPoll_ = now; }

    void suspend() {
        if (!verified_) { *this = {}; return; }
        resumeThread_ = thread_;
        lastPoll_ = 0;
        polls_ = lastPolls_ = 0;
        steady_ = need - 60;
        verified_ = false;
    }

    bool verified() const { return verified_; }
    int polls() const { return lastPolls_; }

    static constexpr int need = 300, keep = 60, penalty = 15, maxPolls = 32;
    static constexpr int64_t burstPerSecond = 3333;

private:
    int64_t lastPoll_ = 0, present_ = 0;
    unsigned long thread_ = 0;
    unsigned long resumeThread_ = 0;
    int steady_ = 0, polls_ = 0, lastPolls_ = 0;
    bool verified_ = false;
};

// A frame limit as a schedule of frame starts. A frame that took longer than its slot starts a new schedule, so one
// slow frame is not paid back by rushing the next ones.
class Limiter {
public:
    // the time this frame may start at; now itself when there is nothing to wait for
    int64_t next(int64_t now, int64_t frequency, float fps) {
        if (fps < 10.f) return last_ = now;
        int64_t target = last_ + int64_t(double(frequency) / double(fps));
        return last_ = last_ && now < target ? target : now;
    }

private:
    int64_t last_ = 0;
};

}
