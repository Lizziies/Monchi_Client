#include "flarial/Bridge/FontJobs.hpp"

#include <chrono>
#include <future>
#include <stdexcept>

int main() {
    using namespace std::chrono_literals;
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    fontJobs::run([&] { entered.set_value(); gate.wait(); });
    entered.get_future().wait();
    fontJobs::run([] { throw std::runtime_error("failed font read"); });
    auto stop = std::async(std::launch::async, [] { fontJobs::stop(); });
    bool waited = stop.wait_for(30ms) == std::future_status::timeout;
    release.set_value();
    if (stop.wait_for(2s) != std::future_status::ready) return 2;
    stop.get();
    if (!waited) return 1;
    if (fontJobs::failures.load() != 1) return 4;
    bool ran = false;
    fontJobs::run([&] { ran = true; });
    fontJobs::stop();
    return ran ? 3 : 0;
}
