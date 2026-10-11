#include "I18n.hpp"
#include "Manager.hpp"
#include "SelfTest.hpp"
#include "common/Sounds.hpp"
#include "Check.hpp"
#include "core/Guard.hpp"
#include "core/Log.hpp"
#include "gui/Gui.hpp"
#include "gui/Notify.hpp"
#include "gui/Profile.hpp"
#include "gui/Widgets.hpp"
#include "hook/GameInput.hpp"
#include "hook/Input.hpp"
#include "server/HiveApi.hpp"
#include "server/Rules.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"
#include "sdk/Inject.hpp"
#include "sdk/Memory.hpp"
#include "Tiers.hpp"

#include <atomic>
#include "sig/Sigs.hpp"

#include "camera/Camera.hpp"
#include "flarial/FlarialModules.hpp"
#include "client/ClickGui.hpp"
#include "client/ClientSettings.hpp"
#include "client/SigStatus.hpp"
#include "combat/Counters.hpp"
#include "combat/Feedback.hpp"
#include "combat/TotemHelper.hpp"
#include "visual/ItemSize.hpp"
#include "combat/Target.hpp"
#include "combat/Pvp.hpp"
#include "combat/Tweaks.hpp"
#include "comfort/Chat.hpp"
#include "comfort/FasterInventory.hpp"
#include "comfort/Music.hpp"
#include "comfort/Lock.hpp"
#include "comfort/Nick.hpp"
#include "comfort/Screenshot.hpp"
#include "comfort/Streamer.hpp"
#include "comfort/Toggles.hpp"
#include "fun/BlockGame.hpp"
#include "fun/DvdScreen.hpp"
#include "fun/EyeBreak.hpp"
#include "fun/Flappy.hpp"
#include "fun/Pets.hpp"
#include "fun/Snake.hpp"
#include "camera/McGuiScale.hpp"
#include "hud/Clock.hpp"
#include "hud/Extras.hpp"
#include "hud/HotbarAnim.hpp"
#include "hud/Cps.hpp"
#include "hud/Compose.hpp"
#include "hud/Fps.hpp"
#include "hud/GameInfo.hpp"
#include "hud/ItemTracker.hpp"
#include "hud/Inventory.hpp"
#include "hud/Keystrokes.hpp"
#include "hud/Paperdoll.hpp"
#include "hud/Latency.hpp"
#include "hud/Memory.hpp"
#include "hud/MouseStrokes.hpp"
#include "hud/Pomodoro.hpp"
#include "hud/ServerInfo.hpp"
#include "hud/SessionTimer.hpp"
#include "hud/Stopwatch.hpp"
#include "input/CpsLimiter.hpp"
#include "input/Gamemode.hpp"
#include "input/HotbarKeys.hpp"
#include "input/NoScroll.hpp"
#include "network/LatencyBlame.hpp"
#include "network/Network.hpp"
#include "network/PingCounter.hpp"
#include "network/Probe.hpp"
#include "online/MonchiOnline.hpp"
#include "perf/Auto.hpp"
#include "perf/FrameLimiter.hpp"
#include "perf/LowLatency.hpp"
#include "perf/Boost.hpp"
#include "perf/MouseSync.hpp"
#include "perf/PerformanceLock.hpp"
#include "perf/Tuning.hpp"
#include "platform/Scripts.hpp"
#include "platform/Share.hpp"
#include "post/Capture.hpp"
#include "post/Effects.hpp"
#include "post/FunEffects.hpp"
#include "post/PostFx.hpp"
#include "perf/GameOptions.hpp"
#include "world/Particles.hpp"
#include "perf/SystemBoost.hpp"
#include "server/Hive.hpp"
#include "server/HiveStats.hpp"
#include "server/MatchSummary.hpp"
#include "server/ServerProfiles.hpp"
#include "server/Zeqa.hpp"
#include "visual/Crosshair.hpp"
#include "visual/PitchDisplay.hpp"
#include "visual/Trail.hpp"
#include "world/Entities.hpp"
#include "world/HealthAbove.hpp"
#include "world/Waypoints.hpp"
#include "world/World.hpp"

#include <windows.h>

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <utility>

namespace modules {

static std::vector<std::unique_ptr<Module>> list;
static std::atomic<unsigned> hudHidden{0};
static std::atomic<bool> inputInWorld{false};

// Keys and mouse buttons arrive on the game's window thread while the modules run on the render thread. Switching a
// module there would run its onEnable and onDisable next to its own onFrame, so a switch asked for by a key waits
// here and is carried out at the start of the next frame.
enum class Switch { Off, On, Flip, Fault };
static std::mutex switchLock;
static std::vector<std::pair<Module*, Switch>> switches;

static void later(Module* m, Switch what) {
    std::scoped_lock g(switchLock);
    switches.push_back({m, what});
}
static float cost = 0.f;
static Motion motion;

template <class T>
static void add() {
    list.push_back(std::make_unique<T>());
    list.back()->captureDefaults();
}

void init() {
    // the list is read from the window thread too; it never moves once it has room for everything
    list.reserve(1024);
    game::init();
    add<ClickGui>();
    add<ClientSettings>();

    add<Fps>();
    add<Cps>();
    add<Keystrokes>();
    add<MouseStrokes>();
    add<Clock>();
    add<Stopwatch>();
    add<Pomodoro>();
    add<SessionTimer>();
    add<Memory>();
    add<LatencyHud>();
    add<ServerInfo>();
    add<IpDisplay>();
    add<Coordinates>();
    add<DirectionHud>();
    add<SpeedDisplay>();
    add<LookAngles>();
    add<PitchDisplay>();
    add<HealthDisplay>();
    add<ExperienceInfo>();
    add<DayCounter>();
    add<PackDisplay>();
    add<HeldItem>();
    add<ItemTracker>();
    add<Paperdoll>();
    add<StatsHud>();
    add<Watermark>();
    add<DebugMenu>();
    add<ArmorHud>();
    add<PotionHud>();
    add<PotCounter>();
    add<ArrowCounter>();
    add<TotemCounter>();
    add<ItemCounter>();
    add<DurabilityWarning>();
    add<LowHealth>();
    add<PingCounter>();
    add<Network>();
    add<LatencyBlame>();

    add<SaturationHue>();
    add<BrightnessContrast>();
    add<ScreenTint>();
    add<Sharpen>();
    add<DepthOfField>();
    add<Blur>();
    add<ShaderPacks>();
    add<ColorFilter>();
    add<NightShift>();

    add<FovChanger>();
    add<JavaDynamicFov>();
    add<Zoom>();
    add<Freelook>();
    add<CinematicCamera>();
    add<NoViewBobbing>();
    add<McGuiScale>();
    add<NoHurtCam>();
    add<AutoPerspective>();
    add<Fullbright>();
    add<BlockOutline>();
    add<ChunkBorder>();
    add<Waypoints>();
    add<HideHand>();
    add<ViewModel>();

    add<BreakProgress>();
    add<Crosshair>();

    add<ReachCounter>();
    add<OpponentReach>();
    add<ComboCounter>();
    add<HitCounter>();
    add<HitPing>();
    add<SessionStats>();
    add<HitInfo>();
    add<EntityCounter>();
    add<TargetHud>();
    add<Waila>();
    add<BowCharge>();
    add<CooldownIndicator>();
    add<DamageIndicator>();
    add<HitMarker>();
    add<HitEffects>();
    add<KillEffects>();
    add<HitSound>();
    add<TotemPop>();
    add<TotemHelper>();
    add<ItemSize>();
    add<Hitbox>();
    add<SensMultiplier>();
    add<BowSensitivity>();
    add<SnapLook>();
    add<NullMovement>();
    add<CrystalOptimizer>();
    add<KillCleanup>();
    add<CpsLimiter>();
    add<NoScroll>();
    add<HotbarKeys>();
    add<ToggleSprint>();
    add<ToggleSneak>();
    add<CommandHotkey>();
    add<TextHotkey>();
    add<ProfileHotkeys>();
    add<AutoGG>();
    add<MessageLogger>();
    add<ChatPlus>();
    add<MusicControl>();
    add<DeathLogger>();
    add<PlayerNotifier>();
    add<ScoreboardPlus>();
    add<TabList>();
    add<StreamerMode>();

    add<ServerProfiles>();
    add<MatchSummary>();
    add<InventoryLock>();
    add<FasterInventory>();
    add<ModernKeybinds>();
    add<JavaInventoryHotkeys>();
    add<Nick>();
    add<HotbarAnimation>();
    add<TntTimer>();
    add<LuaScripts>();
    add<ConfigSharing>();
    add<MonchiOnline>();
    add<FallPredictor>();
    add<InventoryView>();
    add<ArrowTrail>();
    add<BlackBars>();
    add<GamemodeHotkeys>();
    add<ThirdPersonNametag>();
    add<HealthAbove>();
    add<HiveUtils>();
    add<ZeqaUtils>();
    add<HiveStats>();
    add<HiveLeaderboard>();

    add<Screenshot>();

    add<LowLatency>();
    add<FpsBoost>();
    add<UltraInput>();
    add<FrameLimiter>();
    add<AutoProfile>();
    add<BackgroundLoad>();
    add<SystemBoost>();
    add<PerformanceMode>();
    add<ParticleFilter>();
    add<PerformanceLock>();
    add<MouseSync>();
    add<SigStatus>();

    add<Snake>();
    add<Flappy>();
    add<DvdScreen>();
    add<EyeBreak>();
    add<BlockGame>();
    add<Pet>();
    add<Petals>();
    add<Deepfry>();
    add<UpsideDown>();

    refreshSigs();
    for (auto& m : list)
        if (m->alwaysOn()) m->setEnabled(true);

    if (const char* dump = std::getenv("MONCHI_DUMP_MODULES")) {
        nlohmann::json out = nlohmann::json::array();
        for (auto& m : list) {
            int visible = 0;
            for (auto& st : m->settings()) visible += !st.hidden;
            out.push_back({{"name", m->name()},
                           {"category", int(m->category())},
                           {"sub", m->sub()},
                           {"description", m->description()},
                           {"tags", m->tags()},
                           {"sigs", m->sigs()},
                           {"anySig", m->anySigs()},
                           {"tier", tierOf(m->name())},
                           {"settings", visible},
                           {"risky", m->risky()},
                           {"hud", m->isHud()}});
        }
        std::ofstream(dump) << out.dump(1);
    }
    logger::info("{} modules registered", list.size());
    std::string locked, open;
    for (auto& m : list) (m->available() ? open : locked) += m->name() + "; ";
    logger::info("usable: {}", open);
    logger::info("locked: {}", locked);
}

void shutdown() {
    check::write();
    // Monchi's own modules undo their work while the core is still there (MC GUI Scale asks it to lay the screen out
    // again); the stand-ins for the core's modules follow once it is gone, so they do not switch anything off in it
    auto off = [](bool core) {
        for (auto& m : list)
            if (m->enabled() && m->hasTag("flarial") == core) guard::call(m->name().c_str(), [&] { m->onDisable(); });
    };
    off(false);
    flarialModules::shutdown();
    off(true);
    fx::shutdown();
    inject::shutdown();
    probe::shutdown();
    hive::shutdown();
    online::shutdown();
    post::shutdown();
    capture::shutdown();
    sounds::shutdown();
    game::shutdown();
}

const std::vector<std::unique_ptr<Module>>& all() { return list; }

void adopt(std::unique_ptr<Module> m) {
    m->captureDefaults();
    list.push_back(std::move(m));
}

Module* find(const std::string& name) {
    for (auto& m : list)
        if (m->name() == name && !m->replaced()) return m.get();
    return nullptr;
}

static void fault(Module& m) {
    logger::error("module fault: {}", m.name());
    m.setEnabled(false);
    notify::push(i18n::tr("Module disabled"), i18n::fmt("{} had an error. See the log for details.", m.name()), notify::Kind::Error);
}

static void applySwitches() {
    std::vector<std::pair<Module*, Switch>> now;
    {
        std::scoped_lock g(switchLock);
        now.swap(switches);
    }
    for (auto& [m, what] : now) {
        switch (what) {
        case Switch::Off: m->setEnabled(false); break;
        case Switch::On: m->setEnabled(true); break;
        case Switch::Fault: fault(*m); break;
        case Switch::Flip:
            m->toggle();
            if (m->rule() == RuleLevel::Block) notify::push(m->name(), i18n::tr("Not allowed on this server."), notify::Kind::Warn);
            break;
        }
    }
}

void frame(ImDrawList* hud) {
    applySwitches();
    input::reconcile();
    selftest::tick();
    check::tick();
    bool sigsChanged = sigs::takeChanged();
    bool effectsChanged = fx::takeChanged();
    if (sigsChanged || effectsChanged) refreshSigs();
    rules::tick();
    guard::call("flarial modules", [] { flarialModules::sync(); });

    LARGE_INTEGER t0, t1, qpf;
    QueryPerformanceCounter(&t0);
    perf::begin();
    mem::nextFrame();
    post::begin();
    fx::begin();
    // the game's own key bindings: read at the start and again whenever a game screen closes, off this thread
    {
        static bool wasFree = true;
        static unsigned seenOptions = ~0u;
        bool free = mcopt::cursorFree();
        if (wasFree && !free) mcopt::refresh();
        wasFree = free;
        if (unsigned v = mcopt::version(); v != seenOptions) {
            seenOptions = v;
            int chat = mcopt::gameKey("chat");
            inject::gameChatKey(chat > VK_XBUTTON2 ? chat : 0);
        }
    }
    gameinput::tick();
    gameinput::beginFrame();
    input::sample();
    bool lostFocus = input::takeFocusLost() || !input::focused();
    if (lostFocus) {
        gui::hideOverlays();
        gameinput::clearLatches();
    }
    static unsigned liveFrame = 0;
    // a crash report names its thread: this line tells whether that was the one Monchi draws on
    if (!liveFrame) logger::info("frames are drawn on thread {}", GetCurrentThreadId());
    bool measuredLive = (liveFrame++ & 7) == 0;
    double liveAt = measuredLive ? gui::profile::stamp() : 0;
    guard::call("sdk", [] { game::update(); });
    if (measuredLive) gui::profile::sample(gui::profile::Stage::Live, gui::profile::since(liveAt));
    inputInWorld = game::state().inWorld;
    for (auto& m : list) m->publishBinding();
    if (lostFocus || game::state().screen != game::Screen::None) {
        gui::hideOverlays();
        for (auto& m : list)
            if (m->enabled() && (m->name() == "Snake" || m->name() == "Flappy Heart" || m->name() == "Block Game"))
                m->setEnabled(false);
    }
    input::consumeMotion(motion.x, motion.y);

    bool editing = gui::editingHud();
    static std::vector<float> spent;
    // the cost shown per module is an average, one frame in eight is enough for it
    static unsigned frameNo = 0;
    bool timed = (frameNo++ & 7) == 0;
    static const double perMs = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return 1000.0 / double(f.QuadPart);
    }();
    if (timed) spent.assign(list.size(), 0.f);
    auto stamp = [&] {
        LARGE_INTEGER t{};
        if (timed) QueryPerformanceCounter(&t);
        return t;
    };
    auto elapsed = [&](LARGE_INTEGER from) {
        if (!timed) return 0.f;
        LARGE_INTEGER to;
        QueryPerformanceCounter(&to);
        return float(double(to.QuadPart - from.QuadPart) * perMs);
    };
    for (size_t i = 0; i < list.size(); i++) {
        auto& m = list[i];
        if (!m->enabled()) continue;
        LARGE_INTEGER from = stamp();
        fx::owner(m->name().c_str());
        if (!guard::call(m->name().c_str(), [&] { m->onFrame(); })) fault(*m);
        fx::owner(nullptr);
        if (timed) spent[i] += elapsed(from);
    }

    post::submit(hud);
    capture::submit(hud, capture::Stage::Game);
    bool inWorld = game::state().inWorld;
    // chat, pause menu, inventory and the game's settings belong to the game, the HUD only shows while playing
    bool gameScreen = game::state().screen != game::Screen::None;
    static bool wasInWorld = false;
    if (inWorld != wasInWorld) {
        wasInWorld = inWorld;
        logger::info("in world: {}", inWorld);
    }
    for (size_t i = 0; i < list.size(); i++) {
        auto& m = list[i];
        if (!m->enabled()) continue;
        if (!inWorld && !editing) continue;
        if (ctx::cinematicHide && !editing && m->name() != "Cinematic Camera") continue;
        if (m->isHud() && (hudHidden || gameScreen) && !editing) continue;
        LARGE_INTEGER from = stamp();
        if (!guard::call(m->name().c_str(), [&] { m->onRender(hud); })) fault(*m);
        if (timed) spent[i] += elapsed(from);
    }
    if (timed)
        for (size_t i = 0; i < list.size() && i < spent.size(); i++) list[i]->costMs += (spent[i] - list[i]->costMs) * 0.2f;

    capture::submit(hud, capture::Stage::Overlay);
    gameinput::publish();
    gameinput::notePresent();
    perf::apply();
    guard::call("effects", [] { fx::apply(); });
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&qpf);
    cost += (float(double(t1.QuadPart - t0.QuadPart) * 1000.0 / double(qpf.QuadPart)) - cost) * 0.05f;
}

float costMs() { return cost; }

const Module* slowest() {
    const Module* worst = nullptr;
    for (auto& m : list)
        if (m->enabled() && (!worst || m->costMs > worst->costMs)) worst = m.get();
    return worst;
}

Motion mouseDelta() { return motion; }

static bool active() { return inputInWorld.load() || gui::open() || gui::editingHud(); }

// a module's own key; the side and middle mouse buttons count as keys here
static void binds(int vk, bool down, bool repeat) {
    bool hotkeys = !gui::capturesKeyboard() && input::grabbed();
    for (auto& m : list) {
        unsigned binding = m->inputBinding();
        int key = binding & 0xffffu;
        if (!key || key != vk || m->alwaysOn()) continue;
        if (binding & (1u << 16)) {
            if (!down) later(m.get(), Switch::Off);
            else if (!repeat && hotkeys) later(m.get(), Switch::On);
            continue;
        }
        if (!down || repeat || !hotkeys) continue;
        later(m.get(), Switch::Flip);
    }
}

void dispatchKey(KeyEvent& ev) {
    if (widgets::capturingKey() || inject::ours() || !active()) return;
    if (ev.down && !ev.repeat && ev.vk == VK_F1) hudHidden.fetch_xor(1);

    bool captured = gui::capturesKeyboard();
    binds(ev.vk, ev.down, ev.repeat);

    // with the cursor free the player is typing in chat or clicking through a game screen: presses stay there,
    // releases still go through so nothing is left held
    bool gameScreen = !input::grabbed() && !gui::open() && !gui::editingHud();
    for (auto& m : list) {
        // with the menu open a press stays in the menu; the release still reaches the module, or what it held
        // when the menu opened would stay held
        if (!m->enabled() || (captured && ev.down && !m->alwaysOn())) continue;
        if (gameScreen && ev.down && !m->alwaysOn() && !m->screenKeys()) continue;
        if (!guard::call(m->name().c_str(), [&] { m->onKey(ev); })) later(m.get(), Switch::Fault);
        if (ev.cancel) return;
    }
}

void dispatchMouse(MouseEvent& ev) {
    if (!active()) return;
    if (!widgets::capturingKey()) {
        int vk = ev.button == MouseButton::Middle ? VK_MBUTTON : ev.button == MouseButton::X1 ? VK_XBUTTON1 : ev.button == MouseButton::X2 ? VK_XBUTTON2 : 0;
        if (vk) binds(vk, ev.down, false);
    }
    bool gameScreen = !input::grabbed() && !gui::open() && !gui::editingHud();
    for (auto& m : list) {
        if (!m->enabled()) continue;
        if (gameScreen && (ev.down || ev.wheel) && !m->alwaysOn()) continue;
        if (!guard::call(m->name().c_str(), [&] { m->onMouse(ev); })) later(m.get(), Switch::Fault);
        if (ev.cancel) return;
    }
    if (!gui::open() && !gui::editingHud() && flarialModules::mouse(ev)) ev.cancel = true;
}

void dispatchServer(const ServerEvent& ev) {
    for (auto& m : list) {
        if (!m->enabled()) continue;
        guard::call(m->name().c_str(), [&] { m->onServer(ev); });
    }
}

void refreshSigs() {
    int open = 0;
    std::string locked;
    for (auto& m : list) {
        m->checkSigs();
        m->setEnabled(m->userEnabled());
        open += m->available();
        if (!m->available()) locked += m->name() + "; ";
    }
    logger::info("modules: {} usable, {} locked", open, int(list.size()) - open);
    if (open < int(list.size())) logger::info("still locked: {}", locked);
}

}
