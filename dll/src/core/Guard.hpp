#pragma once

#include <exception>
#include <string>
#include <type_traits>

namespace guard {

struct Fault {
    std::string where;
    std::string what;
};

bool run(const char* where, void (*fn)(void*), void* ctx);
void report(const char* where, const char* what);
const Fault* last();
void clearLast();

void installNet();
void removeNet();

// called once per drawn frame; when the frames stop for seconds the log gets the stack of the thread that drew them
void beat();

template <class F>
bool call(const char* where, F&& f) {
    struct Box {
        std::remove_reference_t<F>* f;
        const char* where;
    } box{&f, where};

    return run(where, [](void* p) {
        auto* b = static_cast<Box*>(p);
        try {
            (*b->f)();
        } catch (const std::exception& e) {
            report(b->where, e.what());
        } catch (...) {
            report(b->where, "unknown exception");
        }
    }, &box);
}

}
