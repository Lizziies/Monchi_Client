#pragma once

#include <windows.h>
#include <dxgi.h>

#include <string>
#include <thread>

namespace sysinfo {

inline std::string gpuName() {
    static std::string name = [] {
        std::string out = "–";
        IDXGIFactory1* factory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
            IDXGIAdapter1* adapter = nullptr;
            if (SUCCEEDED(factory->EnumAdapters1(0, &adapter)) && adapter) {
                DXGI_ADAPTER_DESC1 d{};
                if (SUCCEEDED(adapter->GetDesc1(&d))) {
                    std::wstring w = d.Description;
                    out.assign(w.begin(), w.end());
                }
                adapter->Release();
            }
            factory->Release();
        }
        return out;
    }();
    return name;
}

inline int cores() { return int(std::thread::hardware_concurrency()); }

class Cpu {
public:
    float percent() {
        ULONGLONG now = GetTickCount64();
        if (now - at_ < 500) return last_;
        FILETIME create, exit, kernel, user;
        if (!GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user)) return last_;
        auto to64 = [](const FILETIME& f) { return (ULONGLONG(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
        ULONGLONG busy = to64(kernel) + to64(user);
        if (at_) {
            double wall = double(now - at_) * 10000.0, used = double(busy - busy_);
            last_ = float(std::min(100.0, used / wall / std::max(1, cores()) * 100.0));
        }
        at_ = now;
        busy_ = busy;
        return last_;
    }

private:
    ULONGLONG at_ = 0, busy_ = 0;
    float last_ = 0.f;
};

inline float systemRamPercent() {
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    return GlobalMemoryStatusEx(&m) ? float(m.dwMemoryLoad) : 0.f;
}

inline double processUptimeSeconds() {
    FILETIME create, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user)) return 0.0;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    auto to64 = [](const FILETIME& f) { return (ULONGLONG(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
    return double(to64(now) - to64(create)) / 1e7;
}

}
