#include "I18n.hpp"
#include "ClientClaim.hpp"
#include "Client.hpp"
#include "Bg.hpp"
#include "Build.hpp"
#include "Config.hpp"
#include "Guard.hpp"
#include "Log.hpp"
#include "Paths.hpp"
#include "hook/Dx.hpp"
#include "hook/Hook.hpp"
#include "hook/Input.hpp"
#include "gui/Notify.hpp"
#include "hook/Net.hpp"
#include "modules/Manager.hpp"
#include "modules/post/PostFx.hpp"
#include "sdk/Explore.hpp"
#include "server/Rules.hpp"
#include "sig/Sigs.hpp"
#include "system/Tweaks.hpp"
#include "system/GpuLatency.hpp"

#include <atomic>

namespace client {

static HMODULE self = nullptr;
static std::atomic<bool> leaving{false};
static ClientClaim claim;

static void boot() {
    paths::init(self);
    i18n::load();
    logger::open();
    guard::installNet();
    logger::info("{} {} loading", build::name, build::version);

    if (!hook::init()) return;

    sigs::init();
    rules::init();
    modules::init();
    config::load();

    if (!dx::install()) {
        logger::error("renderer hooks failed, nothing to draw on");
    }
    net::install();
    hook::enableAll();

    for (int i = 0; i < 600 && !dx::window() && !leaving; i++) Sleep(50);
    if (dx::window()) input::install(dx::window());

    notify::push(i18n::tr("Monchi loaded"), i18n::tr("Right Shift opens the menu."), notify::Kind::Ok, 6.f);
    logger::info("ready");
}

// returns false when a background job is still running; the dll then has to stay loaded
static bool teardown() {
    logger::info("unloading");
    explore::stop();
    bool inputDetached = input::uninstall();
    if (!inputDetached) logger::warn("another overlay owns the window procedure; keeping the DLL loaded");
    hook::disableAll();
    dx::unhookTables();
    // the frame that was being drawn when the hooks went has to leave Monchi before anything is taken apart
    Sleep(100);
    for (int i = 0; i < 100 && dx::busy(); i++) Sleep(20);
    Sleep(200);
    config::save();
    net::waitIdle(6000);
    modules::shutdown();
    bool drained = bg::drain(8000);
    bool latencyRestored = false;
    guard::call("GPU latency shutdown", [&] { latencyRestored = gpuLatency::shutdown(); });
    if (!latencyRestored) logger::warn("GPU latency reset failed; keeping the DLL loaded");
    if (drained) post::releaseCompiled();
    tweaks::restore();
    bool rendererStopped = false;
    guard::call("renderer shutdown", [&] { rendererStopped = dx::uninstall(); });
    if (!rendererStopped) logger::warn("core still needs the renderer; keeping both DLLs loaded");
    bool clean = inputDetached && drained && latencyRestored && rendererStopped;
    if (!clean) {
        logger::warn("unload incomplete, retaining hook storage and both DLLs until the game closes");
        return false;
    }
    hook::shutdown();
    guard::removeNet();
    logger::info("bye");
    logger::close();
    return true;
}

static DWORD WINAPI mainThread(LPVOID) {
    if (!claim.acquire()) FreeLibraryAndExitThread(self, 0);
    guard::call("boot", boot);

    while (!leaving) Sleep(50);

    if (teardown()) {
        claim.release();
        FreeLibraryAndExitThread(self, 0);
    }
    ExitThread(0);
    return 0;
}

void start(HMODULE module) {
    self = module;
    DisableThreadLibraryCalls(module);
    HANDLE t = CreateThread(nullptr, 0, mainThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
}

void requestUnload() { leaving = true; }
bool unloading() { return leaving; }
HMODULE module() { return self; }

}
