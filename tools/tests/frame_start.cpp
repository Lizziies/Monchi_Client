// Where the game's frame starts, told from its input polls, and the frame limit that waits there.
#include "hook/FrameStart.hpp"

#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        failures++;
    }
}

constexpr int64_t freq = 10'000'000;

}

int main() {
    using framestart::Detector;
    using framestart::Limiter;

    {
        // keyboard and mouse polled by one thread, once per presented frame, at 500 fps
        Detector d;
        int starts = 0, frames = 0;
        int64_t now = 1000;
        bool early = false;
        for (int f = 0; f < 400; f++) {
            frames++;
            starts += d.poll(now, freq, 7, f + 1);
            check(!d.poll(now + 50, freq, 7, f + 1), "the second poll of a frame is not a frame start");
            if (f == 200 && d.verified()) early = true;
            now += freq / 500;
        }
        check(starts == frames, "one frame start per frame");
        check(!early, "not trusted after a short while");
        check(d.verified(), "trusted after a few hundred steady frames");
        check(d.polls() == 2, "the polls of a frame are counted");

        check(d.poll(now + 6 * freq, freq, 7, 401), "first poll after a six-second pause opens a frame");
        check(!d.verified(), "a pause invalidates the old input pacing rhythm");
        now += 6 * freq;
        // a handful of frames out of rhythm (no Present in between) and it is given up again
        for (int f = 0; f < 8; f++) {
            d.poll(now, freq, 7, 400);
            now += freq / 500;
        }
        check(!d.verified(), "given up when the polls stop following the frames");
    }
    {
        // a thread of its own that polls at its own rate, here two threads taking turns
        Detector d;
        int64_t now = 1000;
        for (int f = 0; f < 2000; f++) {
            d.poll(now, freq, f % 2 ? 7 : 9, f + 1);
            now += freq / 500;
        }
        check(!d.verified(), "polls from changing threads are never a frame start");
    }
    {
        // one odd frame now and then does not undo it
        Detector d;
        int64_t now = 1000;
        for (int f = 0; f < 3000; f++) {
            int64_t present = f % 100 == 99 ? f : f + 1;
            d.poll(now, freq, 7, present);
            now += freq / 240;
        }
        check(d.verified(), "a single frame without a new Present is tolerated");
    }
    {
        // a wait in front of the input is not mistaken for the gap between two frames
        Detector d;
        int64_t now = 1000;
        check(d.poll(now, freq, 7, 1), "first poll opens a frame");
        d.resume(now + 30'000);
        check(!d.poll(now + 30'040, freq, 7, 1), "the poll right after the wait belongs to the same frame");
    }
    {
        Detector d;
        int64_t now = 1000;
        for (int f = 1; f <= 350; ++f) {
            check(d.poll(now, freq, 7, f), "new presented frame opens one input boundary");
            now += freq / 177;
        }
        check(d.verified(), "input boundary is trained");
        check(!d.poll(now, freq, 7, 350), "a delayed poll in the same frame must not pace twice");
        now += freq / 333;
        check(!d.poll(now, freq, 7, 350), "a third delayed poll still must not pace twice");
        now += freq / 177;
        check(d.poll(now, freq, 7, 351), "pacing resumes on the next presented frame");
    }
    {
        Detector d;
        int64_t now = 1000;
        for (int f = 1; f <= 350; ++f) {
            d.poll(now, freq, 7, f);
            now += freq / 178;
        }
        d.suspend();
        check(!d.verified(), "focus loss suspends pacing immediately");
        now += 30 * freq;
        for (int f = 351; f < 410; ++f) {
            d.poll(now, freq, 7, f);
            now += freq / 178;
        }
        check(!d.verified(), "resume still requires fresh steady frames");
        d.poll(now, freq, 7, 410);
        check(d.verified(), "same input thread resumes after sixty fresh frames");
        d.suspend();
        now += 30 * freq;
        for (int f = 411; f < 511; ++f) {
            d.poll(now, freq, 9, f);
            now += freq / 178;
        }
        check(!d.verified(), "a different thread requires full training");
    }
    {
        Limiter l;
        int64_t period = freq / 100, base = 1'000'000;
        check(l.next(base, freq, 100.f) == base, "the first frame starts at once");
        check(l.next(base + 30'000, freq, 100.f) == base + period, "the next one waits for its slot");
        check(l.next(base + period + 30'000, freq, 100.f) == base + 2 * period, "slots follow each other without drift");
        // a slow frame: its slot is gone, the schedule starts again from now
        int64_t late = base + 2 * period + 250'000;
        check(l.next(late, freq, 100.f) == late, "a frame that took too long starts at once");
        check(l.next(late + 10'000, freq, 100.f) == late + period, "and the schedule goes on from there");
        check(l.next(late + 20'000, freq, 0.f) == late + 20'000, "no limit, no wait");
    }
    if (!failures) std::printf("frame_start ok\n");
    return failures ? 1 : 0;
}
