#include <windows.h>
#include "modules/perf/Tuning.hpp"
#include "modules/perf/Pacing.hpp"
#include "hook/Dx.hpp"
#include "hook/GameInput.hpp"
#include "system/GpuLatency.hpp"
#include <cassert>

namespace { dx::Tuning tune; bool verified = false; bool focused = true; float cap = -1; int calls = 0; gpuLatency::Mode mode; }
namespace input { bool focused() { return ::focused; } }
namespace dx { Tuning& tuning() { return tune; } HWND window() { return nullptr; } const FrameInfo& frame() { static FrameInfo f; return f; } }
namespace gameinput { bool frameStartVerified() { return verified; } void pace(float fps) { cap = fps; } }
namespace gpuLatency { bool set(Mode m) { ++calls; mode = m; return true; } }
int main(int argc, char**) {
    assert(perf::refreshCap(180.f) == 178.f);
    assert(perf::refreshCap(144.f) == 142.f);
    assert(perf::refreshCap(60.f) == 58.f);
    assert(perf::refreshCap(75.f) == 73.f);
    assert(perf::refreshCap(120.f) == 118.f);
    assert(perf::refreshCap(165.f) == 163.f);
    assert(perf::refreshCap(240.f) == 238.f);
    assert(perf::refreshCap(360.f) == 358.f);
    assert(perf::refreshCap(500.f) == 498.f);
    assert(perf::refreshCap(59.94f) > 57.93f && perf::refreshCap(59.94f) < 57.95f);
    if (argc > 1) {
        assert(!perf::foreignPacer());
        auto rtss = LoadLibraryW(L"RTSSHooks64.dll");
        assert(rtss);
        assert(perf::foreignPacer());
        verified = true;
        perf::begin(); perf::limit(144); perf::alignToInput();
        perf::gpu(gpuLatency::Mode::Boost); perf::syncToDisplay(); perf::tearing(); perf::apply();
        assert(tune.fpsLimit == 0 && cap == 0 && mode == gpuLatency::Mode::Off);
        assert(tune.syncToDisplay && !tune.allowTearing);
        focused = false; perf::begin(); perf::limit(30); perf::apply();
        assert(tune.fpsLimit == 30 && cap == 0);
        FreeLibrary(rtss);
        assert(!perf::foreignPacer());
        return 0;
    }
    assert(perf::localCap(144.f, true, true) == 0.f);
    assert(perf::localCap(30.f, true, false) == 30.f);
    assert(perf::localCap(144.f, false, true) == 144.f);
    perf::begin(); perf::gpu(gpuLatency::Mode::Off); perf::gpu(gpuLatency::Mode::Boost);
    perf::gpu(gpuLatency::Mode::On); perf::lowLatency(); perf::limit(144); perf::alignToInput();
    perf::tearing(); perf::syncToDisplay(false); perf::apply();
    assert(calls == 1 && mode == gpuLatency::Mode::Boost);
    assert(tune.lowLatency && tune.syncToDisplay && !tune.allowTearing);
    assert(tune.fpsLimit == 144 && cap == 0);
    verified = true; perf::apply(); assert(tune.fpsLimit == 0 && cap == 144);
    focused = false; perf::apply();
    assert(mode == gpuLatency::Mode::Off && tune.fpsLimit == 144 && cap == 0);
    perf::begin(); perf::limit(5); perf::apply();
    assert(tune.fpsLimit == 5 && cap == 0);
    focused = true;
    perf::begin(); perf::apply();
    assert(mode == gpuLatency::Mode::Off && !tune.lowLatency && !tune.syncToDisplay);
    assert(tune.fpsLimit == 0 && cap == 0);
    // with the display sync the frame rate is held under the refresh rate, in front of the input once that is known
    perf::begin(); perf::syncToDisplay(); perf::apply();
    float under = perf::underRefreshCap();
    assert(under > 10 && under < float(perf::refreshRate()) && tune.syncToDisplay && tune.fpsLimit == 0 && cap == under);
    verified = false; perf::begin(); perf::syncToDisplay(); perf::apply(); assert(tune.fpsLimit == under && cap == 0);
}
