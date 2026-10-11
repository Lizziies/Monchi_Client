#pragma once

#include <mutex>
#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace fontJobs {

inline std::mutex mutex;
inline std::mutex stopMutex;
inline std::atomic<unsigned> failures{0};
struct Job {
    std::shared_ptr<std::atomic<bool>> done;
    std::jthread thread;
};
inline std::vector<Job> jobs;
inline bool closed = false;

template<class F>
void run(F&& task) {
    std::scoped_lock lock(mutex);
    if (closed) return;
    std::erase_if(jobs, [](const Job& job) { return job.done->load(); });
    auto done = std::make_shared<std::atomic<bool>>(false);
    auto thread = std::jthread([done, task = std::forward<F>(task)] {
        try { task(); }
        catch (...) { failures.fetch_add(1); }
        done->store(true);
    });
    jobs.push_back({std::move(done), std::move(thread)});
}

inline void stop() {
    std::scoped_lock stopLock(stopMutex);
    std::vector<Job> pending;
    {
        std::scoped_lock lock(mutex);
        closed = true;
        pending.swap(jobs);
    }
    pending.clear();
}

}
