// The core's event dispatcher: listeners run in priority order, a listener may listen or deafen during its own event
// (upstream froze there on its shared lock), and events can be dispatched while another thread switches listeners.
#include "flarial/shim/nes/event_dispatcher.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

struct Ping {
    std::string trail;
};

struct Counter {
    std::atomic<int> calls{0};
    void on(Ping&) { calls++; }
};

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        failures++;
    }
}

nes::event_dispatcher events;

struct Ordered {
    char tag;
    void on(Ping& p) { p.trail += tag; }
};

struct Leaver {
    int calls = 0;
    void on(Ping&) {
        calls++;
        events.deafen<Ping, &Leaver::on>(this);
    }
};

struct Joiner {
    Counter late;
    bool joined = false;
    void on(Ping&) {
        if (joined) return;
        joined = true;
        events.listen<Ping, &Counter::on>(&late);
    }
};

}

int main() {
    struct Unused {};
    check(events.get<Unused>().empty(), "an event without listeners needs no render pass");
    Counter render;
    events.listen<Ping, &Counter::on>(&render);
    check(!events.get<Ping>().empty(), "an active listener requires the pass");
    events.deafen<Ping, &Counter::on>(&render);
    check(events.get<Ping>().empty(), "removing the last listener stops the pass");
    using P = nes::event_priority;
    {
        Ordered a{'a'}, b{'b'}, c{'c'}, d{'d'};
        events.listen<Ping, &Ordered::on, P::LAST>(&a);
        events.listen<Ping, &Ordered::on, P::NORMAL>(&b);
        events.listen<Ping, &Ordered::on, P::FIRST>(&c);
        events.listen<Ping, &Ordered::on, P::NORMAL>(&d);
        auto e = nes::make_holder<Ping>();
        events.trigger(e);
        check(e->trail == "cbda", "priority first, then the order of listening");
        for (auto* o : {&a, &b, &c, &d}) events.deafen<Ping, &Ordered::on>(o);
        auto none = nes::make_holder<Ping>();
        events.trigger(none);
        check(none->trail.empty(), "deafened listeners are gone");
    }
    {
        Leaver leaver;
        Counter after;
        events.listen<Ping, &Leaver::on>(&leaver);
        events.listen<Ping, &Counter::on>(&after);
        auto e = nes::make_holder<Ping>();
        events.trigger(e);
        events.trigger(e);
        check(leaver.calls == 1, "a listener can deafen itself during its own event");
        check(after.calls == 2, "the listeners behind it still run");
        events.deafen<Ping, &Counter::on>(&after);
    }
    {
        Joiner joiner;
        events.listen<Ping, &Joiner::on>(&joiner);
        auto e = nes::make_holder<Ping>();
        events.trigger(e);
        check(joiner.late.calls == 0, "a listener added during an event waits for the next one");
        events.trigger(e);
        check(joiner.late.calls == 1, "and is called from then on");
        events.deafen<Ping, &Joiner::on>(&joiner);
        events.deafen<Ping, &Counter::on>(&joiner.late);
    }
    {
        Counter steady, flicker;
        events.listen<Ping, &Counter::on>(&steady);
        std::atomic<bool> stop{false};
        std::atomic<int> dispatched{0};
        std::thread senders[3];
        for (auto& t : senders)
            t = std::thread([&] {
                while (!stop) {
                    auto e = nes::make_holder<Ping>();
                    events.trigger(e);
                    dispatched++;
                }
            });
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
        int switches = 0;
        while (std::chrono::steady_clock::now() < until) {
            events.listen<Ping, &Counter::on>(&flicker);
            events.deafen<Ping, &Counter::on>(&flicker);
            switches++;
        }
        stop = true;
        for (auto& t : senders) t.join();
        check(switches > 100 && dispatched > 1000, "dispatching and switching made progress side by side");
        check(steady.calls == dispatched, "the steady listener saw every event exactly once");
        events.deafen<Ping, &Counter::on>(&steady);
    }
    {
        // a listener that faults is skipped from then on and the rest keeps running
        struct Bad {
            void on(Ping&) { throw 1; }
        } bad;
        Counter good;
        events.listen<Ping, &Bad::on>(&bad);
        events.listen<Ping, &Counter::on>(&good);
        auto e = nes::make_holder<Ping>();
        events.trigger(e);
        events.trigger(e);
        check(good.calls == 2, "a throwing listener does not stop the others");
        check(nes::listener_guard::faulted(&bad), "and is marked as broken");
    }
    if (!failures) std::printf("event_dispatcher ok\n");
    return failures ? 1 : 0;
}
