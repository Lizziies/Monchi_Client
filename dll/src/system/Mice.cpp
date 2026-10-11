#include "Mice.hpp"
#include "core/Bg.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>

namespace mice {

namespace {

constexpr int slotCount = 8;
constexpr int ring = 128;

struct Slot {
    std::atomic<void*> handle{nullptr};
    std::array<std::atomic<int64_t>, ring> stamps{};
    std::atomic<unsigned> head{0};
    std::atomic<int64_t> lastUse{0};
    std::atomic<long> sum{0};
};

std::array<Slot, slotCount> slots;
std::atomic<bool> counting{false};
std::atomic<int> users{0};
std::atomic<int> generation{0};
std::mutex lock;
Snapshot shown;
std::string activeId;
int64_t frequency = 0;

struct Info {
    void* handle;
    std::string key;
    std::string name;
    std::string vendor;
};

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

const char* vendorOf(unsigned vid) {
    switch (vid) {
    case 0x046D: return "Logitech";
    case 0x1532: return "Razer";
    case 0x1038: return "SteelSeries";
    case 0x1B1C: return "Corsair";
    case 0x045E: return "Microsoft";
    case 0x1E7D: return "Roccat";
    case 0x0951:
    case 0x03F0: return "HyperX";
    case 0x258A: return "Glorious";
    case 0x2516: return "Cooler Master";
    case 0x0B05: return "ASUS";
    case 0x0DB0: return "MSI";
    case 0x093A: return "PixArt";
    case 0x04D9: return "Holtek";
    default: return "";
    }
}

using HidStringFn = BOOLEAN(WINAPI*)(HANDLE, PVOID, ULONG);

std::string hidString(HidStringFn fn, HANDLE h) {
    wchar_t buf[128] = {};
    if (!fn || !fn(h, buf, sizeof(buf))) return {};
    return narrow(buf);
}

std::vector<Info> enumerate() {
    static HMODULE hid = LoadLibraryW(L"hid.dll");
    static auto product = hid ? reinterpret_cast<HidStringFn>(reinterpret_cast<void*>(GetProcAddress(hid, "HidD_GetProductString"))) : nullptr;
    static auto maker = hid ? reinterpret_cast<HidStringFn>(reinterpret_cast<void*>(GetProcAddress(hid, "HidD_GetManufacturerString"))) : nullptr;

    std::vector<Info> out;
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) return out;
    std::vector<RAWINPUTDEVICELIST> list(count);
    if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1)) return out;

    for (UINT i = 0; i < count; i++) {
        if (list[i].dwType != RIM_TYPEMOUSE) continue;
        UINT len = 0;
        GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, nullptr, &len);
        if (len < 8) continue;
        std::wstring path(len, L'\0');
        GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, path.data(), &len);
        path.resize(wcslen(path.c_str()));

        unsigned vid = 0, pid = 0;
        size_t v = path.find(L"VID_"), p = path.find(L"PID_");
        if (v == std::wstring::npos || p == std::wstring::npos) continue;
        vid = (unsigned)wcstoul(path.c_str() + v + 4, nullptr, 16);
        pid = (unsigned)wcstoul(path.c_str() + p + 4, nullptr, 16);

        Info info;
        info.handle = list[i].hDevice;
        char key[16];
        snprintf(key, sizeof(key), "%04X:%04X", vid, pid);
        info.key = key;
        info.vendor = vendorOf(vid);

        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            info.name = hidString(product, h);
            std::string maker1 = hidString(maker, h);
            if (info.vendor.empty()) info.vendor = maker1;
            CloseHandle(h);
        }
        auto has = [&](const char* s) {
            auto low = [](std::string t) {
                std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                return t;
            };
            return low(info.name + info.vendor).find(s) != std::string::npos;
        };
        if (has("attack shark")) info.vendor = "Attack Shark";
        if (info.name.empty()) info.name = info.vendor.empty() ? info.key : info.vendor + " " + info.key;
        out.push_back(std::move(info));
    }
    return out;
}

std::vector<std::string> runningSoftware() {
    static const std::pair<const char*, const char*> tools[] = {
        {"lghub", "Logitech G HUB"},       {"synapse", "Razer Synapse"}, {"razercentral", "Razer Synapse"}, {"steelseriesgg", "SteelSeries GG"},
        {"icue", "Corsair iCUE"},          {"glorious", "Glorious Core"}, {"attackshark", "Attack Shark"},   {"attack shark", "Attack Shark"},
        {"hyperx", "HyperX NGENUITY"},     {"roccat", "ROCCAT Swarm"},    {"armourycrate", "ASUS Armoury Crate"}};
    std::vector<std::string> found;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return found;
    PROCESSENTRY32W e{sizeof(e)};
    for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e)) {
        std::string name = narrow(e.szExeFile);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        for (auto& [needle, label] : tools)
            if (name.find(needle) != std::string::npos && std::find(found.begin(), found.end(), label) == found.end()) found.push_back(label);
    }
    CloseHandle(snap);
    return found;
}

int pollingRate(Slot& s) {
    std::vector<int64_t> stamps;
    stamps.reserve(ring);
    for (auto& v : s.stamps) {
        int64_t t = v.load(std::memory_order_relaxed);
        if (t) stamps.push_back(t);
    }
    if (stamps.size() < 24) return 0;
    std::sort(stamps.begin(), stamps.end());
    std::vector<double> gaps;
    for (size_t i = 1; i < stamps.size(); i++) {
        double ms = double(stamps[i] - stamps[i - 1]) * 1000.0 / double(frequency);
        if (ms > 0.05 && ms < 30.0) gaps.push_back(ms);
    }
    if (gaps.size() < 16) return 0;
    std::nth_element(gaps.begin(), gaps.begin() + long(gaps.size() / 2), gaps.end());
    double hz = 1000.0 / gaps[gaps.size() / 2];
    static const int rates[] = {125, 250, 500, 1000, 2000, 4000, 8000};
    int best = rates[0];
    for (int r : rates)
        if (std::fabs(std::log(hz / r)) < std::fabs(std::log(hz / best))) best = r;
    return best;
}

void loop(int gen) {
    int tick = 0;
    std::vector<Info> infos;
    std::vector<std::string> software;
    while (generation == gen && users > 0) {
        if (tick % 6 == 0) infos = enumerate();
        if (tick % 10 == 0) software = runningSoftware();
        tick++;

        int64_t now = 0;
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        now = t.QuadPart;

        Snapshot snap;
        snap.software = software;
        int64_t newest = 0;
        std::string newestKey;
        for (auto& info : infos) {
            Slot* slot = nullptr;
            for (auto& s : slots)
                if (s.handle.load() == info.handle) slot = &s;
            Device d{info.key, info.name, info.vendor, slot ? pollingRate(*slot) : 0, false};
            int64_t last = slot ? slot->lastUse.load() : 0;
            if (last > newest && now - last < frequency * 3) {
                newest = last;
                newestKey = info.key;
            }
            snap.devices.push_back(std::move(d));
        }
        for (auto& d : snap.devices) d.active = d.key == newestKey;
        {
            std::scoped_lock g(lock);
            shown = std::move(snap);
            if (!newestKey.empty()) activeId = newestKey;
        }
        for (int i = 0; i < 5 && generation == gen && users > 0; i++) Sleep(100);
    }
}

}

void onRaw(void* device, int dx, int dy, int64_t qpc) {
    Slot* slot = nullptr;
    for (auto& s : slots) {
        void* h = s.handle.load(std::memory_order_relaxed);
        if (h == device) {
            slot = &s;
            break;
        }
    }
    if (!slot) {
        for (auto& s : slots) {
            void* expected = nullptr;
            if (s.handle.compare_exchange_strong(expected, device)) {
                slot = &s;
                break;
            }
        }
    }
    if (!slot) return;
    slot->lastUse.store(qpc, std::memory_order_relaxed);
    if (dx == 0 && dy == 0) return;
    unsigned i = slot->head.fetch_add(1, std::memory_order_relaxed) % ring;
    slot->stamps[i].store(qpc, std::memory_order_relaxed);
    if (counting.load(std::memory_order_relaxed)) slot->sum.fetch_add(dx < 0 ? -dx : dx, std::memory_order_relaxed);
}

// The mouse reports this module measures used to come from the game's window. The game reads its mouse through
// GameInput on this version and asks Windows for no raw reports, so nothing arrived and every mouse stayed at
// "measuring". When nobody in the process has asked for raw mouse reports, a hidden window of our
// own does, in the background, and feeds the measuring. A registration the game makes itself is never replaced.
std::thread listener;
std::atomic<DWORD> listenerThread{0};

LRESULT CALLBACK listen(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INPUT) {
        alignas(8) BYTE buf[256];
        UINT size = sizeof(buf);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != UINT(-1)) {
            auto* raw = reinterpret_cast<RAWINPUT*>(buf);
            if (raw->header.dwType == RIM_TYPEMOUSE) {
                LARGE_INTEGER t;
                QueryPerformanceCounter(&t);
                onRaw(raw->header.hDevice, raw->data.mouse.lLastX, raw->data.mouse.lLastY, t.QuadPart);
            }
        }
    }
    return DefWindowProcW(w, msg, wp, lp);
}

bool mouseRegistered() {
    UINT count = 0;
    GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE));
    if (!count || count > 32) return false;
    std::array<RAWINPUTDEVICE, 32> list{};
    UINT got = GetRegisteredRawInputDevices(list.data(), &count, sizeof(RAWINPUTDEVICE));
    for (UINT i = 0; i < got && i < list.size(); i++)
        if (list[i].usUsagePage == 1 && list[i].usUsage == 2) return true;
    return false;
}

void listenLoop() {
    listenerThread = GetCurrentThreadId();
    if (mouseRegistered()) return;
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = listen;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"MonchiMouseListener";
    RegisterClassExW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!w) return;
    RAWINPUTDEVICE want{1, 2, RIDEV_INPUTSINK, w};
    bool ours = RegisterRawInputDevices(&want, 1, sizeof(want)) != FALSE;
    MSG m;
    while (ours && GetMessageW(&m, nullptr, 0, 0) > 0) DispatchMessageW(&m);
    if (ours) {
        // only what is still ours is taken back: the game may have asked for the reports itself since
        std::array<RAWINPUTDEVICE, 32> list{};
        UINT count = UINT(list.size());
        UINT got = GetRegisteredRawInputDevices(list.data(), &count, sizeof(RAWINPUTDEVICE));
        for (UINT i = 0; i < got && i < list.size(); i++)
            if (list[i].usUsagePage == 1 && list[i].usUsage == 2 && list[i].hwndTarget == w) {
                RAWINPUTDEVICE gone{1, 2, RIDEV_REMOVE, nullptr};
                RegisterRawInputDevices(&gone, 1, sizeof(gone));
            }
    }
    DestroyWindow(w);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

void use(bool on) {
    if (on) {
        if (users++ == 0) {
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            frequency = f.QuadPart;
            int gen = ++generation;
            bg::run([gen] { loop(gen); });
            if (!listener.joinable()) listener = std::thread(listenLoop);
        }
        return;
    }
    if (users > 0 && --users == 0 && listener.joinable()) {
        for (int i = 0; i < 100 && !listenerThread.load(); i++) Sleep(5);
        PostThreadMessageW(listenerThread.load(), WM_QUIT, 0, 0);
        listener.join();
        listenerThread = 0;
    }
}

Snapshot snapshot() {
    std::scoped_lock g(lock);
    return shown;
}

void calibrateBegin() {
    for (auto& s : slots) s.sum = 0;
    counting = true;
}

long calibrateCounts() {
    counting = false;
    int64_t newest = 0;
    long sum = 0;
    for (auto& s : slots) {
        int64_t last = s.lastUse.load();
        if (last > newest) {
            newest = last;
            sum = s.sum.load();
        }
    }
    return sum;
}

std::string activeKey() {
    std::scoped_lock g(lock);
    return activeId;
}

}
