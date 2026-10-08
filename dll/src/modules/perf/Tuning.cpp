#include "Tuning.hpp"
#include "Pacing.hpp"
#include "hook/Dx.hpp"
#include "hook/Input.hpp"
#include "hook/GameInput.hpp"
#include "system/GpuLatency.hpp"
#include "core/Log.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace perf {

static bool queue = false;
static bool tear = false;
static float cap = 0.f;
static bool aligned = false;
static bool sync = false;
static bool under = false;
static gpuLatency::Mode gpuMode = gpuLatency::Mode::Off;

void begin() {
    gpuMode = gpuLatency::Mode::Off;
    sync = false;
    under = false;
    queue = false;
    tear = false;
    cap = 0.f;
    aligned = false;
}

// asking the driver costs up to milliseconds, the rate rarely changes
int refreshRate() {
    static int cached = 0;
    static ULONGLONG at = 0;
    ULONGLONG now = GetTickCount64();
    if (cached && now - at < 2000) return cached;
    at = now;
    HWND w = dx::window();
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    HMONITOR mon = MonitorFromWindow(w ? w : GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    cached = GetMonitorInfoW(mon, &mi) && EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1 ? int(dm.dmDisplayFrequency) : 60;
    return cached;
}

float underRefreshCap() {
    return refreshCap(float(refreshRate()));
}

// Holding the rate under the refresh rate only helps where the display follows the frame rate (G-Sync, FreeSync).
// On a display with a fixed rate it makes every twentieth frame stay for two refreshes, which is seen as judder. The
// display tells which kind it is by how the frames come out: with the cap on, a share of frames far longer than one
// refresh means the display did not follow, and the cap is dropped for the rest of the session.
static bool fixedRate = false;

static void watchPacing() {
    static int64_t seen = 0;
    static int frames = 0, doubled = 0, between = 0;
    auto& f = dx::frame();
    if (f.presentQpc == seen || f.frameMs <= 0.0) return;
    seen = f.presentQpc;
    double period = 1000.0 / double(refreshRate());
    // a hitch (a chunk load, a pause) is not a doubled frame
    if (f.frameMs > period * 2.6) return;
    frames++;
    // A fixed-rate display shows a late frame for exactly two refreshes. One that follows the frame rate also shows
    // frames anywhere in between, which is all a join or a chunk burst produces; those used to count as doubled and
    // dropped the cap for the whole session after the first unsteady minute.
    if (std::fabs(f.frameMs - period * 2.0) < period * 0.15) doubled++;
    else if (f.frameMs > period * 1.15 && f.frameMs < period * 1.85) between++;
    if (frames < 900) return;
    if (doubled * 100 > frames * 3 && between * 4 < doubled) fixedRate = true;
    frames = doubled = between = 0;
}

bool displayFollows() { return !fixedRate; }

// RivaTuner (RTSS, MSI Afterburner) can limit the frame rate and drive NVIDIA Reflex itself. Two tools doing that in
// one game take turns putting each frame to sleep and the frame rate jumps back and forth. While its hooks are in
// the game, Monchi leaves foreground pacing to it. Background throttling remains local.
bool foreignPacer() {
    return GetModuleHandleW(L"RTSSHooks64.dll") != nullptr;
}

// A one-frame queue makes the CPU wait for the GPU every frame. While the frame limit holds the rate that wait is free
// and saves a frame of latency; once the GPU cannot reach the limit, as in a crowded lobby, it costs up to 30 FPS.
// The queue is let go while frames run clearly over the target and taken back after they hold it again for a while;
// each relapse soon after doubles that while, so it never flips back and forth.
static bool starved(float target) {
    static int64_t seen = 0;
    static double avg = 0.0;
    static bool released = false;
    static ULONGLONG since = 0, hold = 10000;
    auto& f = dx::frame();
    if (target < 1.f) return released = false;
    if (f.presentQpc == seen || f.frameMs <= 0.0 || f.frameMs > 100.0) return released;
    seen = f.presentQpc;
    avg = avg > 0.0 ? avg * 0.95 + f.frameMs * 0.05 : f.frameMs;
    double period = 1000.0 / double(target);
    ULONGLONG now = GetTickCount64();
    if (!released && avg > period * 1.08) {
        hold = now - since < 20000 ? std::min<ULONGLONG>(hold * 2, 160000) : 10000;
        released = true;
        since = now;
        logger::info("low latency: frames at {:.2f} ms against {:.2f} ms, the short queue waits for {} s", avg, period, hold / 1000);
    } else if (released && avg < period * 1.02 && now - since > hold) {
        released = false;
        since = now;
    } else if (released && avg >= period * 1.02) {
        since = std::max(since, now - hold / 2);
    }
    return released;
}

void apply() {
    bool foreign = foreignPacer();
    if (foreign) gpuMode = gpuLatency::Mode::Off;
    if (sync && under && !fixedRate && !foreign && input::focused()) {
        limit(underRefreshCap());
        aligned = true;
        watchPacing();
    }
    auto& t = dx::tuning();
    // with another tool limiting the frame rate the queue is left to the game, as without Monchi
    t.lowLatency = queue && !foreign && !starved(cap > 0.f ? cap : underRefreshCap());
    t.allowTearing = tear && !sync;
    t.syncToDisplay = sync;
    bool focused = input::focused();
    t.presentFloor = sync && under && !fixedRate && !foreign && focused ? underRefreshCap() : 0.f;
    gpuLatency::set(focused ? gpuMode : gpuLatency::Mode::Off);
    float activeCap = localCap(cap, foreign, focused);
    // one place waits, never both: in front of the input when the frame start is known, after Present otherwise
    bool atInput = focused && aligned && activeCap >= 10.f && gameinput::frameStartVerified();
    t.fpsLimit = atInput ? 0.f : activeCap;
    gameinput::pace(atInput ? activeCap : 0.f);
}

void lowLatency() { queue = true; }
void gpu(gpuLatency::Mode mode) { if (int(mode) > int(gpuMode)) gpuMode = mode; }
void syncToDisplay(bool underRefresh) {
    sync = true;
    under = under || underRefresh;
}

void tearing() { tear = true; }

void alignToInput() { aligned = true; }

void limit(float fps) {
    if (fps < 1.f) return;
    if (cap == 0.f || fps < cap) cap = fps;
}

}
