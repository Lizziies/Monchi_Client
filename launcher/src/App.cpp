#include "App.hpp"
#include "Build.hpp"
#include "Embedded.hpp"
#include "Files.hpp"
#include "Game.hpp"
#include "I18n.hpp"
#include "Levi.hpp"
#include "Net.hpp"
#include "Update.hpp"
#include "Versions.hpp"
#include "Downgrade.hpp"
#include "../res/resource.h"

#include <json.hpp>

#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <thread>

using i18n::tr;

namespace app {

namespace {

// the launcher starts with the accent the client menu uses, read from the active client profile
int clientAccent() {
    auto settings = nlohmann::json::parse(files::read(files::root() / L"settings.json"), nullptr, false);
    std::string profile = settings.is_object() ? settings.value("profile", "default") : "default";
    auto config = nlohmann::json::parse(files::read(files::root() / L"configs" / files::widen(profile + ".json")), nullptr, false);
    if (!config.is_object() || !config.contains("theme") || !config["theme"].is_object()) return 0;
    auto& a = config["theme"]["accent"];
    if (!a.is_array() || a.size() < 3 || !a[0].is_number()) return 0;
    return ui::nearestAccent(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());
}

struct Shared {
    std::mutex lock;
    ui::Phase phase = ui::Phase::Idle;
    float progress = 0.f;
    std::string status;
    std::string latest;
    std::string notes;
    bool updatePrompt = false;
    bool updateKnown = false;
    bool updateAvailable = false;
    bool launcherUpdate = false;
    std::optional<update::Release> pending;
    std::string gameVersion;
    bool gameSupported = true;
    bool managerInstalled = false;
    bool managerBusy = false;
    float managerProgress = 0.f;
    std::string managerStatus;
    std::vector<versions::Install> installs;
    std::vector<std::string> sigVersions;
    bool sigsKnown = false;
    std::vector<versions::Download> downloads;
    bool resetPinned = false;
    bool versionBusy = false;
    bool versionInstalling = false;
    float versionProgress = 0.f;
    std::string versionStatus;
};

Shared shared;
std::atomic<bool> busy{false};
std::atomic<bool> quit{false};
std::atomic<DWORD> lastInjected{0};
std::atomic<bool> versionBusy{false};
std::atomic<bool> cancelVersion{false};
std::atomic<bool> closePending{false};
std::atomic<bool> betaChannel{false};
Settings current;
std::mutex settingsLock;

Settings settingsSnapshot() {
    std::lock_guard g(settingsLock);
    return current;
}

void set(ui::Phase phase, const std::string& status, float progress = 0.f) {
    std::lock_guard g(shared.lock);
    shared.phase = phase;
    shared.status = status;
    shared.progress = progress;
}

void setProgress(float p) {
    std::lock_guard g(shared.lock);
    shared.progress = p;
}

void fail(const std::string& error) { set(ui::Phase::Failed, tr(error.c_str())); }

void loadSigIndex() {
    auto body = net::get(std::string("https://raw.githubusercontent.com/") + files::narrow(build::repoOwner) + "/" +
                         files::narrow(build::repoName) + "/" + files::narrow(build::repoBranch) + "/sigs/index.json");
    if (!body) {
        auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_SIG_INDEX), RT_RCDATA);
        auto data = resource ? LoadResource(nullptr, resource) : nullptr;
        if (data) body = std::string(static_cast<const char*>(LockResource(data)), SizeofResource(nullptr, resource));
    }
    if (!body) return;
    auto j = nlohmann::json::parse(*body, nullptr, false);
    if (j.is_discarded() || !j.contains("versions")) return;
    std::lock_guard g(shared.lock);
    shared.sigVersions.clear();
    for (auto& v : j["versions"])
        if (v.is_string()) shared.sigVersions.push_back(v.get<std::string>());
    shared.sigsKnown = true;
}

bool supportedLocked(const std::string& version) {
    if (version.empty() || !shared.sigsKnown) return false;
    auto family = version;
    if (std::count(family.begin(), family.end(), '.') == 3) family.erase(family.find_last_of('.'));
    return std::find(shared.sigVersions.begin(), shared.sigVersions.end(), family) != shared.sigVersions.end();
}

void rescan() {
    std::vector<files::fs::path> extra;
    for (auto& f : settingsSnapshot().folders) extra.push_back(files::fs::path(files::widen(f)));
    auto found = versions::scan(extra);
    std::lock_guard g(shared.lock);
    shared.installs = std::move(found);
}

void refreshGame() {
    std::string version = game::installedVersion();
    loadSigIndex();
    std::lock_guard g(shared.lock);
    shared.gameVersion = version;
    shared.gameSupported = supportedLocked(version);
}

bool checkUpdate(bool startup = false) {
    auto began = GetTickCount64();
    auto release = update::latest(betaChannel.load(), startup ? 2000 : 8000);
    if (startup && GetTickCount64() - began > 10000) return false;
    std::lock_guard g(shared.lock);
    if (!release) return false;
    shared.updateKnown = true;
    shared.pending = release;
    shared.latest = release->tag;
    shared.notes = release->notes;
    shared.launcherUpdate = !release->launcherUrl.empty() && update::newer(release->tag, build::version);
    auto installed = update::installedTag();
    if (installed.empty()) installed = build::version;
    shared.updateAvailable = shared.launcherUpdate ||
        (!release->dllUrl.empty() && update::newer(release->tag, installed));
    if (startup) shared.updatePrompt = shared.updateAvailable;
    return true;
}

std::filesystem::path clientDll() {
    auto settings = settingsSnapshot();
    if (!settings.customDll.empty()) return files::fs::path(files::widen(settings.customDll));
    std::string error;
    if (!embedded::install(error)) {
        fail(error);
        return {};
    }
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    auto beside = files::fs::path(self).parent_path() / L"Monchi.dll";
    if (!files::fs::exists(files::dll()) && files::fs::exists(beside)) {
        std::error_code ec;
        files::fs::copy_file(beside, files::dll(), files::fs::copy_options::overwrite_existing, ec);
    }
    auto core = beside.parent_path() / L"MonchiFlarial.dll";
    if (!files::fs::exists(files::core()) && files::fs::exists(core)) {
        std::error_code ec;
        files::fs::copy_file(core, files::core(), files::fs::copy_options::overwrite_existing, ec);
    }
    return files::dll();
}

std::optional<update::Release> pending() {
    std::lock_guard g(shared.lock);
    return shared.pending;
}

bool installUpdate(bool strict = false) {
    auto release = pending();
    if (!release || release->dllUrl.empty() || !settingsSnapshot().customDll.empty()) return true;
    if (!update::newer(release->tag, update::installedTag()) && files::fs::exists(files::dll())) return true;
    set(ui::Phase::Updating, i18n::fmt("Downloading {}", release->tag));
    std::string error;
    if (!update::installDll(*release, setProgress, error)) {
        if (strict || !files::fs::exists(files::dll())) {
            fail(error);
            return false;
        }
        return true;
    }
    std::lock_guard g(shared.lock);
    shared.updateAvailable = false;
    return true;
}

bool connect(DWORD pid) {
    if (game::injected(pid)) {
        lastInjected = pid;
        return true;
    }
    set(ui::Phase::Injecting, tr("Connecting the client"));
    std::string error;
    auto dll = clientDll();
    if (dll.empty()) return false;
    if (!game::inject(pid, dll, error)) {
        fail(error);
        return false;
    }
    lastInjected = pid;
    return true;
}

bool sameFile(const std::wstring& a, const std::string& b) {
    std::error_code ec;
    return !a.empty() && std::filesystem::equivalent(files::fs::path(a), files::fs::path(files::widen(b)), ec);
}

std::optional<DWORD> startGame() {
    auto settings = settingsSnapshot();
    if (auto pid = game::running()) {
        if (!settings.pinned.empty() && !sameFile(game::runningPath(*pid), settings.pinned)) {
            fail("Minecraft is already running with another version. Close it first.");
            return std::nullopt;
        }
        return pid;
    }
    set(ui::Phase::Starting, tr("Starting Minecraft"));
    std::string error = "Minecraft could not be started";
    bool started = settings.pinned.empty() ? game::launch() : game::launchExe(files::fs::path(files::widen(settings.pinned)), error);
    if (!started) {
        fail(error);
        return std::nullopt;
    }
    for (int i = 0; i < 180; i++) {
        if (auto pid = game::running()) return pid;
        Sleep(250);
    }
    fail("Minecraft did not start");
    return std::nullopt;
}

void play() {
    set(ui::Phase::Updating, tr("Checking for updates"));
    auto pid = startGame();
    if (!pid) return;

    set(ui::Phase::Waiting, tr("Waiting for the game to load"));
    if (!game::waitReady(*pid, 120000)) {
        fail(settingsSnapshot().pinned.empty() ? "Minecraft closed before it was ready"
                                    : "The selected version closed before it was ready. Pick another version or use the Store one.");
        return;
    }
    if (!connect(*pid)) return;

    set(ui::Phase::Done, tr("Monchi is connected. Have fun!"));
    if (settingsSnapshot().closeAfterInject) quit = true;
}

void updateAll() {
    set(ui::Phase::Updating, tr("Checking for updates"));
    if (!pending() && !checkUpdate()) { fail("Could not check for updates"); return; }
    auto release = pending();
    bool restart = false;
    {
        std::lock_guard g(shared.lock);
        restart = shared.launcherUpdate && release && !release->launcherUrl.empty();
    }
    bool needsDll = release && !release->dllUrl.empty() && update::newer(release->tag, update::installedTag());
    if (!restart && needsDll && game::running()) { fail("Close Minecraft to update the client"); return; }
    if (restart) {
        set(ui::Phase::Updating, tr("Updating the launcher"));
        std::string error;
        if (update::swapLauncher(*release, setProgress, error)) {
            quit = true;
            return;
        }
        fail(error);
        return;
    }
    if (!installUpdate(true)) return;
    set(ui::Phase::Idle, tr("Monchi is up to date"));
}

void watch() {
    DWORD attempted = 0;
    while (!quit) {
        Sleep(1500);
        if (!versionBusy) {
            auto installed = game::installedVersion();
            std::lock_guard g(shared.lock);
            shared.gameVersion = installed;
            shared.gameSupported = supportedLocked(installed);
        }
        auto pid = game::running();
        if (!pid) attempted = 0;
        bool connected = pid && game::injected(*pid);
        if (!connected && !busy) {
            lastInjected = 0;
            std::lock_guard g(shared.lock);
            if (shared.phase == ui::Phase::Done) {
                shared.phase = ui::Phase::Idle;
                shared.status.clear();
                shared.progress = 0.f;
            }
        }
        auto settings = settingsSnapshot();
        if (versionBusy || busy || !settings.autoInject || !pid || connected) continue;
        if (!settings.pinned.empty() && !sameFile(game::runningPath(*pid), settings.pinned)) continue;
        if (attempted == *pid) continue;
        if (busy.exchange(true)) continue;
        attempted = *pid;
        if (game::waitReady(*pid, 120000) && connect(*pid)) {
            set(ui::Phase::Done, tr("Monchi is connected. Have fun!"));
            if (settings.closeAfterInject) quit = true;
        }
        busy = false;
    }
}

void installManager() {
    {
        std::lock_guard g(shared.lock);
        shared.managerBusy = true;
        shared.managerProgress = 0.f;
        shared.managerStatus = tr("Downloading LeviLauncher");
    }
    std::string error;
    bool ok = levi::install(
        [](float p) {
            std::lock_guard g(shared.lock);
            shared.managerProgress = p;
        },
        error);
    {
        std::lock_guard g(shared.lock);
        shared.managerBusy = false;
        shared.managerInstalled = !levi::find().empty();
        shared.managerStatus = ok ? "" : tr(error.c_str());
    }
    if (ok) levi::open();
}

std::optional<files::fs::path> pickFolder(HWND owner) {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return std::nullopt;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    std::optional<files::fs::path> result;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        PWSTR name = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name))) {
            result = files::fs::path(name);
            CoTaskMemFree(name);
        }
        if (item) item->Release();
    }
    dialog->Release();
    return result;
}

void spawn(void (*job)()) {
    if (busy.exchange(true)) return;
    std::thread([job] {
        job();
        busy = false;
    }).detach();
}

}

void init(ui::State& state) {
    i18n::load();
    update::cleanup();
    current = Settings::load();
    betaChannel = current.beta;
    state.settings = current;
    state.clientAccent = clientAccent();
    std::snprintf(state.dllPath, sizeof(state.dllPath), "%s", current.customDll.c_str());
    state.clientVersion = build::version;

    std::string tag = update::installedTag();
    shared.latest = tag;
    std::thread([] {
        refreshGame();
        rescan();
    }).detach();
    std::thread([] {
        if (!checkUpdate(true) || game::running()) return;
        bool available = false;
        { std::lock_guard g(shared.lock); available = shared.updateAvailable; }
        if (available && !busy.exchange(true)) {
            updateAll();
            busy = false;
        }
    }).detach();
    shared.managerInstalled = !levi::find().empty();
    std::thread(watch).detach();
}

void handle(ui::State& state, const ui::Events& ev, HWND window) {
    std::lock_guard settingsGuard(settingsLock);
    if (ev.settingsChanged) {
        current = state.settings;
        betaChannel = current.beta;
        current.save();
    }
    if (ev.play && !versionBusy) spawn(play);
    if (ev.dismissUpdate || ev.update) { std::lock_guard g(shared.lock); shared.updatePrompt = false; }
    if (ev.update) spawn(updateAll);
    if (ev.checkUpdate) spawn([] {
        set(ui::Phase::Updating, tr("Checking for updates"));
        if (!checkUpdate()) { fail("Could not check for updates"); return; }
        { std::lock_guard g(shared.lock); shared.updatePrompt = shared.updateAvailable; }
        set(ui::Phase::Idle, "");
    });
    if (ev.installManager) {
        static std::atomic<bool> installing{false};
        if (!installing.exchange(true)) std::thread([] { installManager(); installing = false; }).detach();
    }
    if (ev.openManager) levi::open();
    if (ev.pick >= 0) {
        std::string path;
        {
            std::lock_guard g(shared.lock);
            size_t i = size_t(ev.pick);
            if (i >= 1 && i <= shared.installs.size()) path = files::narrow(shared.installs[i - 1].exe.wstring());
        }
        current.pinned = path;
        state.settings.pinned = path;
        current.save();
    }
    if (ev.cancelVersion) cancelVersion = true;
    if (ev.loadVersions && !versionBusy.exchange(true)) {
        cancelVersion = false;
        { std::lock_guard g(shared.lock); shared.versionBusy = true; shared.versionStatus = tr("Loading Minecraft versions"); }
        std::thread([] {
            std::string error;
            rescan();
            auto list = versions::catalog(error);
            std::erase_if(list, [](auto& d) { return d.preview; });
            if (cancelVersion) { list.clear(); error = "Version download cancelled"; }
            std::lock_guard g(shared.lock);
            shared.downloads = std::move(list);
            shared.versionStatus = tr(error.c_str());
            shared.versionBusy = false;
            versionBusy = false;
        }).detach();
    }
    if (ev.installVersion >= 0 && !versionBusy && !busy.exchange(true)) {
        versionBusy = true;
        std::optional<versions::Download> selected;
        {
            std::lock_guard g(shared.lock);
            if (size_t(ev.installVersion) < shared.downloads.size()) selected = shared.downloads[ev.installVersion];
            shared.versionBusy = selected.has_value();
            shared.versionInstalling = false;
            shared.versionProgress = 0.f;
            shared.versionStatus = tr("Checking Minecraft installation");
        }
        cancelVersion = false;
        if (!selected) { versionBusy = false; busy = false; }
        else std::thread([version = *selected] {
            std::string error;
            bool ok = versions::install(version, [](const char* status, float progress) {
                std::lock_guard g(shared.lock);
                shared.versionStatus = tr(status);
                shared.versionProgress = std::max(0.f, progress);
                shared.versionInstalling = progress < 0.f;
            }, cancelVersion, error);
            {
                std::lock_guard g(shared.lock);
                shared.versionBusy = false;
                shared.versionInstalling = false;
                shared.versionStatus = tr(ok ? "Minecraft version installed" : error.c_str());
                if (ok) {
                    shared.resetPinned = true;
                    shared.gameVersion = game::installedVersion();
                    shared.gameSupported = supportedLocked(shared.gameVersion);
                }
            }
            versionBusy = false;
            busy = false;
        }).detach();
    }
    if (ev.rescan) std::thread([] { rescan(); }).detach();
    if (ev.addFolder) {
        if (auto folder = pickFolder(window)) {
            std::string path = files::narrow(folder->wstring());
            if (std::find(current.folders.begin(), current.folders.end(), path) == current.folders.end()) current.folders.push_back(path);
            state.settings.folders = current.folders;
            current.save();
            std::thread([] { rescan(); }).detach();
        }
    }
    if (ev.minimize) ShowWindow(window, SW_MINIMIZE);
    if (ev.close) PostMessageW(window, WM_CLOSE, 0, 0);
    if (ev.openLogs) ShellExecuteW(nullptr, L"open", files::log().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (ev.openFolder) ShellExecuteW(nullptr, L"open", files::root().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (ev.browseDll) {
        wchar_t path[MAX_PATH] = {};
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = window;
        ofn.lpstrFilter = L"DLL\0*.dll\0\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            std::snprintf(state.dllPath, sizeof(state.dllPath), "%s", files::narrow(path).c_str());
            state.settings.customDll = state.dllPath;
            current = state.settings;
            current.save();
        }
    }
}

void sync(ui::State& state) {
    std::lock_guard settingsGuard(settingsLock);
    std::lock_guard g(shared.lock);
    if (shared.resetPinned) {
        current.pinned.clear();
        state.settings.pinned.clear();
        current.save();
        shared.resetPinned = false;
    }
    state.phase = shared.phase;
    state.progress = shared.progress;
    state.status = shared.status;
    state.latestVersion = shared.latest;
    state.updateAvailable = shared.updateAvailable;
    state.updateKnown = shared.updateKnown;
    state.updatePrompt = shared.updatePrompt;
    state.gameVersion = shared.gameVersion;
    state.gameSupported = shared.gameSupported;
    state.managerInstalled = shared.managerInstalled;
    state.managerBusy = shared.managerBusy;
    state.managerProgress = shared.managerProgress;
    state.managerStatus = shared.managerStatus;
    if (!shared.notes.empty()) state.changelog = shared.notes;

    state.versionBusy = shared.versionBusy;
    state.versionInstalling = shared.versionInstalling;
    state.versionProgress = shared.versionProgress;
    state.versionStatus = shared.versionStatus;
    state.downloads.clear();
    for (auto& d : shared.downloads) {
        bool installed = !d.preview && d.version == shared.gameVersion;
        for (auto& copy : shared.installs)
            installed = installed || (copy.preview == d.preview && copy.name == d.version);
        state.downloads.push_back({d.version, d.preview, installed, supportedLocked(d.name), false, true, {}});
    }
    state.versions.clear();
    bool pinnedFound = false;
    for (auto& i : shared.installs)
        if (!current.pinned.empty() && sameFile(i.exe.wstring(), current.pinned)) pinnedFound = true;
    bool storeActive = current.pinned.empty() || !pinnedFound;
    state.versions.push_back({shared.gameVersion, false, !shared.gameVersion.empty(), shared.gameSupported, storeActive, true, {}});
    for (auto& i : shared.installs) {
        bool active = !storeActive && sameFile(i.exe.wstring(), current.pinned);
        state.versions.push_back({i.name, i.preview, true, supportedLocked(i.name), active, false, files::narrow(i.exe.wstring())});
    }
}

bool wantsQuit() { return quit || (closePending && !versionBusy); }

bool canClose() {
    if (!versionBusy) return true;
    closePending = true;
    cancelVersion = true;
    return false;
}

}
