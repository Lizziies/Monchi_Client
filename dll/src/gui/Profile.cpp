#include "Profile.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "gui/Gui.hpp"
#include "hook/GameInput.hpp"
#include "hook/Dx.hpp"
#include "modules/Manager.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <imgui.h>
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>
#include <vector>

namespace gui::profile {

static Stats current, shown;
static double stageSum[3]{}, stageWorst[3]{};
static unsigned stageCount[3]{};

void sample(Stage stage, double us) {
    auto i = static_cast<unsigned>(stage);
    stageSum[i] += us;
    stageWorst[i] = std::max(stageWorst[i], us);
    stageCount[i]++;
}

static float body = 0.f;
static float probeY = 0.f;
static float scrollY = 0.f;
static std::string label;
static FILE* out = nullptr;
static int frames = 0;
static bool wanted = std::getenv("MONCHI_PROFILE") != nullptr;
static double freq = [] {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return double(f.QuadPart);
}();

const Stats& stats() { return shown; }

bool recording() { return wanted; }

double stamp() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return double(t.QuadPart) * 1e6 / freq;
}

double since(double from) { return stamp() - from; }

void row() { current.rows++; }

void height(float h) { body = h; }

void probe(float y, float scroll) {
    probeY = y;
    scrollY = scroll;
}

void phase(const char* name) { label = name; }

void menu(float listUs, float detailsUs) {
    current.listUs = listUs;
    current.detailsUs = detailsUs;
    current.seen = ui::time();
}

// MONCHI_BENCH: every two seconds the cost of Monchi's whole frame and of the most expensive modules goes to the log, so
// a change can be measured in the test host instead of guessed at.
static bool bench = std::getenv("MONCHI_BENCH") != nullptr;
static std::vector<float> benchSamples;
static double benchAt = 0.0;

static void benchReport() {
    std::sort(benchSamples.begin(), benchSamples.end());
    double sum = 0;
    for (float v : benchSamples) sum += v;
    size_t n = benchSamples.size();
    std::vector<const Module*> mods;
    for (auto& m : modules::all())
        if (m->enabled() && m->costMs > 0.0005f) mods.push_back(m.get());
    std::sort(mods.begin(), mods.end(), [](const Module* a, const Module* b) { return a->costMs > b->costMs; });
    std::string top;
    for (size_t i = 0; i < mods.size() && i < 10; i++) top += std::format(" {}={:.0f}", mods[i]->name(), mods[i]->costMs * 1000.f);
    auto* data = ImGui::GetDrawData();
    logger::info("bench: frame us mean {:.0f} median {:.0f} p99 {:.0f} max {:.0f} over {} frames, modules {:.0f} us, {} vertices, menu {}, world {};{}",
                 sum / double(n), benchSamples[n / 2], benchSamples[size_t(double(n) * 0.99)], benchSamples.back(), n, modules::costMs() * 1000.f,
                 data ? data->TotalVtxCount : 0, gui::open() ? "open" : "closed", game::state().inWorld ? "yes" : "no", top);
    benchSamples.clear();
}

// Once a minute, in every build: what Monchi's own frame costs in the real game and where the frame limit waits. Two
// additions and a compare per frame, so a log from a play session carries numbers instead of impressions.
static void minuteReport(float frameUs) {
    static double sum = 0.0, since = 0.0;
    static float worst = 0.f;
    static int frames = 0;
    sum += frameUs;
    worst = std::max(worst, frameUs);
    frames++;
    double now = stamp();
    if (since == 0.0) since = now;
    if (now - since < 60e6) return;
    const Module* slow = modules::slowest();
    auto start = gameinput::frameStart();
    logger::info("perf: {:.0f} fps, monchi frame {:.0f} us mean, {:.1f} ms worst, modules {:.0f} us, slowest {} ({:.0f} us), input poll {}, input {:.1f} ms old at the frame, limit waits {:.1f} ms in front of it, sync {} -> {}, flags 0x{:X}, early frames held {}",
                 double(frames) * 1e6 / (now - since), sum / frames, worst / 1000.f, modules::costMs() * 1000.f, slow ? slow->name() : "-",
                 slow ? slow->costMs * 1000.f : 0.f, start.verified ? "found" : start.hooked ? "not steady" : "not hooked", start.sampleAgeMs, start.waitMs, dx::frame().nativeSync, dx::frame().presentSync, dx::frame().presentFlags, dx::frame().heldPresents);
    logger::info("perf stages: live {:.0f} us mean / {:.1f} ms worst, core {:.0f} us mean / {:.1f} ms worst, overlay submission {:.0f} us mean / {:.1f} ms worst (one frame in eight)",
                 stageSum[0] / std::max(1u, stageCount[0]), stageWorst[0] / 1000.0,
                 stageSum[1] / std::max(1u, stageCount[1]), stageWorst[1] / 1000.0,
                 stageSum[2] / std::max(1u, stageCount[2]), stageWorst[2] / 1000.0);
    for (int i = 0; i < 3; i++) { stageSum[i] = stageWorst[i] = 0; stageCount[i] = 0; }
    sum = 0.0;
    worst = 0.f;
    frames = 0;
    since = now;
}

void frame(float frameUs) {
    current.frameUs = frameUs;
    minuteReport(frameUs);
    if (!bench) return;
    // a single slow frame is what a hitch is; it gets its own line with what was on screen
    if (frameUs > 2500.f)
        logger::info("bench: spike {:.1f} ms at {:.2f} s, menu {}, hud editor {}", frameUs / 1000.f, ui::time(), gui::open() ? "open" : "closed", gui::editingHud() ? "on" : "off");
    benchSamples.push_back(frameUs);
    double now = stamp();
    if (benchAt == 0.0) benchAt = now;
    if (now - benchAt < 2e6) return;
    benchAt = now;
    benchReport();
}

void finish(float dt) {
    auto* data = ImGui::GetDrawData();
    current.windows = ImGui::GetIO().MetricsRenderWindows;
    current.vertices = data ? data->TotalVtxCount : 0;
    current.cmds = 0;
    if (data)
        for (int i = 0; i < data->CmdListsCount; i++) current.cmds += data->CmdLists[i]->CmdBuffer.Size;

    if (current.seen > 0.0 && ui::time() - current.seen < 1.0) {
        float k = 0.1f;
        shown.listUs += (current.listUs - shown.listUs) * k;
        shown.detailsUs += (current.detailsUs - shown.detailsUs) * k;
        shown.frameUs += (current.frameUs - shown.frameUs) * k;
        shown.rows = current.rows;
        shown.cmds = current.cmds;
        shown.vertices = current.vertices;
        shown.windows = current.windows;
        shown.seen = current.seen;
    }

    if (wanted && current.seen > 0.0 && ui::time() - current.seen < 1.0) {
        if (!out) {
            auto file = paths::logs() / L"profile.csv";
            out = _wfopen(file.c_str(), L"w");
            if (out) std::fputs("frame,time,dt_ms,frame_us,list_us,details_us,rows,cmds,vertices,windows,body_h,probe_y,scroll_y,phase\n", out);
        }
        if (out) {
            std::fprintf(out, "%d,%.4f,%.3f,%.1f,%.1f,%.1f,%d,%d,%d,%d,%.2f,%.2f,%.2f,%s\n", frames, ui::time(), dt * 1000.f, current.frameUs, current.listUs,
                         current.detailsUs, current.rows, current.cmds, current.vertices, current.windows, body, probeY, scrollY, label.c_str());
            if (++frames % 120 == 0) std::fflush(out);
        }
    }
    current.rows = 0;
    body = 0.f;
}

}
