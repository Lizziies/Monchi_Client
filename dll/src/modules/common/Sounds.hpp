#pragma once

#include <windows.h>
#include <mmsystem.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

// Sounds are started on a thread of their own. PlaySound and MessageBeep open the audio device before they return,
// even with SND_ASYNC, which held the frame for 8 ms on every hit sound.
namespace sounds {

struct Worker {
    std::mutex lock;
    std::condition_variable wake;
    std::deque<std::function<void()>> jobs;
    std::thread thread;
    bool quit = false;

    void run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock g(lock);
                wake.wait(g, [&] { return quit || !jobs.empty(); });
                if (quit) return;
                job = std::move(jobs.front());
                jobs.pop_front();
            }
            job();
        }
    }
};

inline Worker& worker() {
    static Worker w;
    return w;
}

inline void post(std::function<void()> job) {
    auto& w = worker();
    std::scoped_lock g(w.lock);
    if (w.quit) return;
    // a burst of hits does not queue up a second of sounds
    if (w.jobs.size() >= 4) w.jobs.pop_front();
    w.jobs.push_back(std::move(job));
    if (!w.thread.joinable()) w.thread = std::thread([&w] { w.run(); });
    w.wake.notify_one();
}

inline void beep(UINT kind) {
    post([kind] { MessageBeep(kind); });
}

// before the dll goes: the thread must not be left inside our code
inline void shutdown() {
    auto& w = worker();
    {
        std::scoped_lock g(w.lock);
        w.quit = true;
        w.jobs.clear();
    }
    w.wake.notify_one();
    if (w.thread.joinable()) w.thread.join();
    PlaySoundW(nullptr, nullptr, 0);
    std::scoped_lock g(w.lock);
    w.quit = false;
}

}
