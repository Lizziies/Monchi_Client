// The core's queue of module switches: one that throws must not stay in front and stop every later one (the bug that made
// Block Hit and other modules switch themselves off again, with "fault in flarial frame" in every frame of the log).
#include "flarial/Bridge/ToggleQueue.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Toggle {
    std::string name;
    bool enable;
};

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        failures++;
    }
}

}

// what the core did before: the entry stays in front until it succeeds
void oldDrain(std::queue<Toggle>& queue, std::vector<std::string>& applied) {
    try {
        while (!queue.empty()) {
            auto& t = queue.front();
            if (t.name == "Throws") throw std::runtime_error("onEnable failed");
            applied.push_back(t.name);
            queue.pop();
        }
    } catch (...) {
    }
}

int main() {
    {
        std::queue<Toggle> stuck;
        std::vector<std::string> done;
        stuck.push({"First", true});
        stuck.push({"Throws", true});
        stuck.push({"Block Hit", true});
        oldDrain(stuck, done);
        oldDrain(stuck, done);
        check(done.size() == 1 && stuck.size() == 2, "the old drain stays stuck on the throwing switch (the test sees the bug)");
    }

    std::queue<Toggle> queue;
    std::mutex lock;
    std::vector<std::string> applied, failed;
    auto apply = [&](Toggle& t) {
        if (t.name == "Throws") throw std::runtime_error("onEnable failed");
        applied.push_back(t.name);
    };
    auto fail = [&](Toggle& t) { failed.push_back(t.name); };

    queue.push({"First", true});
    queue.push({"Throws", true});
    queue.push({"Block Hit", true});
    queue.push({"Swing Animations", true});
    toggleQueue::drain(queue, lock, apply, fail);
    check(applied == std::vector<std::string>{"First", "Block Hit", "Swing Animations"}, "switches behind a throwing one are applied");
    check(failed == std::vector<std::string>{"Throws"}, "the throwing switch is reported once");
    check(queue.empty(), "the queue is empty afterwards");

    applied.clear();
    failed.clear();
    toggleQueue::drain(queue, lock, apply, fail);
    check(applied.empty() && failed.empty(), "a second frame does not run the failed switch again");

    queue.push({"Later", true});
    toggleQueue::drain(queue, lock, apply, fail);
    check(applied == std::vector<std::string>{"Later"} && failed.empty(), "a switch queued afterwards is applied");

    int structured = 0;
    queue.push({"Hardware", true});
    toggleQueue::drain(queue, lock, [&](Toggle&) { structured++; throw 7; }, [&](Toggle&) { structured += 10; });
    check(structured == 11, "any thrown type is caught");

    if (!failures) std::printf("toggle_queue ok\n");
    return failures ? 1 : 0;
}
