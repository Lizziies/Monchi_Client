#pragma once

#include "core/Config.hpp"
#include "core/Log.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "hook/Dx.hpp"
#include "modules/HudModule.hpp"
#include "modules/Manager.hpp"
#include "modules/common/Text.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <windows.h>
#include <dxgi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <format>
#include <future>
#include <chrono>
#include <map>
#include <memory>
#include <thread>

class AutoProfile : public Module {
public:
    AutoProfile()
        : Module("Auto Profile", "Detects your hardware, recommends a performance profile and applies it.",
                 Category::Performance, {"performance"}) {
        sub("Diagnostics");
        lowFps_.visible = [this] { return hint_.b; };
        detect();
    }

    void onFrame() override {
        double ms = dx::frame().frameMs;
        if (ms <= 0.0 || !hint_.b) return;
        sum_ += ms;
        frames_++;
        if (sum_ < 30000.0) return;
        double fps = frames_ * 1000.0 / sum_;
        sum_ = 0.0;
        frames_ = 0;
        if (fps < lowFps_.f && !hinted_) {
            hinted_ = true;
            notify::push(i18n::tr("Low FPS"), i18n::fmt("{:.0f} FPS on average. Turn on the performance profile in the \"Auto Profile\" module.", fps), notify::Kind::Warn, 8.f);
        }
    }

    void drawSettings() override {
        auto& t = theme::current();
        ImGui::Spacing();
        ImGui::Text(i18n::tr("Graphics card: %s"), gpu_.c_str());
        ImGui::Text(i18n::tr("Processor: %d cores  ·  Memory: %.0f GB  ·  Video memory: %.0f GB"), cores_, ramGb_, vramGb_);
        const char* names[] = {"Low (maximum FPS)", "Medium", "High (nice graphics)"};
        ImGui::TextColored(t.accent, i18n::tr("Recommendation: %s"), i18n::tr(names[tier()]));
        if (ImGui::Button(i18n::tr("Apply recommendation"))) apply(tier());
        ImGui::SameLine();
        if (ImGui::Button(i18n::tr("Low"))) apply(0);
        ImGui::SameLine();
        if (ImGui::Button(i18n::tr("Medium"))) apply(1);
        ImGui::SameLine();
        if (ImGui::Button(i18n::tr("High"))) apply(2);
    }

private:
    void detect() {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        cores_ = int(si.dwNumberOfProcessors);
        MEMORYSTATUSEX ms{sizeof(ms)};
        if (GlobalMemoryStatusEx(&ms)) ramGb_ = float(double(ms.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0));
        IDXGIFactory* f = nullptr;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&f))) && f) {
            IDXGIAdapter* a = nullptr;
            if (SUCCEEDED(f->EnumAdapters(0, &a)) && a) {
                DXGI_ADAPTER_DESC d{};
                a->GetDesc(&d);
                std::wstring w = d.Description;
                gpu_ = logger::narrow(w);
                vramGb_ = float(double(d.DedicatedVideoMemory) / (1024.0 * 1024.0 * 1024.0));
                a->Release();
            }
            f->Release();
        }
    }

    int tier() const {
        if (cores_ <= 4 || ramGb_ <= 8.f || vramGb_ < 2.f) return 0;
        if (cores_ <= 8 || vramGb_ < 6.f) return 1;
        return 2;
    }

    void apply(int level) {
        auto set = [&](const char* name, bool on) {
            if (auto* m = modules::find(name)) m->setEnabled(on);
        };
        set("Frame Limiter", true);
        set("Low Latency", true);
        set("System Boost", true);
        set("Depth of Field", false);
        set("Sharpen", level == 2);
        if (level == 0) lighten();
        config::markDirty();
        notify::push(i18n::tr("Profile applied"), level == 0 ? i18n::tr("Low") : level == 1 ? i18n::tr("Medium") : i18n::tr("High"), notify::Kind::Ok);
    }

    // weak PCs: no blur, glow or shadows in the HUD and menu, and the decorative extras off
    static void lighten() {
        for (auto& m : modules::all()) {
            if (!m->isHud()) continue;
            for (auto& st : m->settings())
                if (st.id == "blur" || st.id == "glow" || st.id == "dropShadow" || st.id == "glowPressed") st.b = false;
        }
        if (auto* cs = modules::find("Client Settings"))
            for (auto& st : cs->settings())
                if (st.id == "menuBlur") st.f = 0.f;
        for (const char* name : {"Background Load", "Pet", "Petals", "Arrow Trail", "Kill Effects", "Hit Effects", "Blur"})
            if (auto* m = modules::find(name)) m->setEnabled(false);
    }

    Setting& hint_ = toggleSetting("hint", "Notice on low FPS", true);
    Setting& lowFps_ = slider("lowFps", "Low below (FPS)", 60.f, 20.f, 144.f, "%.0f");
    std::string gpu_ = "unknown";
    int cores_ = 0;
    float ramGb_ = 0.f;
    float vramGb_ = 0.f;
    double sum_ = 0.0;
    int frames_ = 0;
    bool hinted_ = false;
};

class BackgroundLoad : public HudModule {
public:
    BackgroundLoad()
        : HudModule("Background Load", "Shows programs that use a lot of CPU and hurt FPS or ping.",
                    {"hud-self"}, {0.845f, 0.95f}) {
        sub("Diagnostics");
        cores_ = std::max(1, int(std::thread::hardware_concurrency()));
    }

    void onFrame() override {
        double now = ui::time();
        if (pending_.valid()) {
            if (pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
            applySample(pending_.get());
        }
        if (now - last_ < interval_.f) return;
        pending_ = std::async(std::launch::async, sample);
        last_ = now;
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        float y = 0.f, w = minWidth_.f * s;
        if (total_show_.b) {
            std::string head = headText_.text.empty() ? i18n::fmt("Background {:.0f}%", total_) : std::format("{}{:.0f}%", headText_.text, total_);
            auto sz = drawText(dl, o, s, head, total_ > warn_.f ? ImGui::GetColorU32(theme::current().warn) : textColor());
            y += sz.y;
            w = std::max(w, sz.x);
        }
        for (auto& r : top_) {
            if (r.percent < hideBelow_.f) continue;
            std::string line = numberFirst_.b ? std::format("{:.0f}%{}{}", r.percent, sep_.text, r.name) : std::format("{}{}{:.0f}%", r.name, sep_.text, r.percent);
            auto t = drawText(dl, o + ImVec2(0, y), s, line, dimRows_.b ? ImGui::GetColorU32(theme::current().textDim) : textColor());
            w = std::max(w, t.x);
            y += t.y;
        }
        return {w, std::max(y, 4.f * s)};
    }

private:
    struct Row {
        std::string name;
        float percent;
    };

    static uint64_t cpuTime(HANDLE h) {
        FILETIME c, e, k, u;
        if (!GetProcessTimes(h, &c, &e, &k, &u)) return 0;
        return ((uint64_t(k.dwHighDateTime) << 32) | k.dwLowDateTime) + ((uint64_t(u.dwHighDateTime) << 32) | u.dwLowDateTime);
    }

    struct Sample {
        std::map<DWORD, uint64_t> times;
        std::map<DWORD, std::wstring> names;
        double at = 0;
    };

    static Sample sample() {
        Sample out;
        out.at = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return out;
        std::unique_ptr<void, decltype(&CloseHandle)> snapshot(snap, CloseHandle);
        PROCESSENTRY32W pe{sizeof(pe)};
        auto& times = out.times;
        auto& names = out.names;
        DWORD self = GetCurrentProcessId();
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == self || pe.th32ProcessID == 0) continue;
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (!h) continue;
                std::unique_ptr<void, decltype(&CloseHandle)> process(h, CloseHandle);
                times[pe.th32ProcessID] = cpuTime(h);
                names[pe.th32ProcessID] = pe.szExeFile;
            } while (Process32NextW(snap, &pe));
        }
        return out;
    }

    void applySample(Sample data) {
        double now = data.at;
        auto& times = data.times;
        auto& names = data.names;
        double dt = now - prevAt_;
        top_.clear();
        total_ = 0.f;
        if (prevAt_ > 0.0 && dt > 0.1) {
            std::vector<Row> rows;
            for (auto& [pid, t] : times) {
                auto it = prev_.find(pid);
                if (it == prev_.end() || t < it->second) continue;
                float pct = float(double(t - it->second) / 1e7 / dt / cores_ * 100.0);
                if (pct < 0.3f) continue;
                total_ += pct;
                std::wstring w = names[pid];
                rows.push_back({logger::narrow(w), pct});
            }
            std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.percent > b.percent; });
            if ((int)rows.size() > count_.i) rows.resize(size_t(count_.i));
            top_ = std::move(rows);
            if (toast_.b && total_ > warn_.f && now - lastToast_ > 120.0) {
                lastToast_ = now;
                notify::push(i18n::tr("High background load"), i18n::fmt("Other programs use {:.0f}% of the CPU.", total_), notify::Kind::Warn, 6.f);
            }
        }
        prev_ = std::move(times);
        prevAt_ = now;
    }

    Setting& interval_ = slider("interval", "Measurement interval (s)", 3.f, 1.f, 10.f, "%.0f s");
    Setting& count_ = intSlider("count", "Shown programs", 3, 1, 6);
    Setting& warn_ = slider("warn", "Warn from (% CPU)", 25.f, 5.f, 80.f, "%.0f");
    Setting& toast_ = toggleSetting("toast", "Notice on high load", true);
    Setting& total_show_ = toggleSetting("total", "Total line", true);
    Setting& headText_ = textSetting("headText", "Custom label (empty uses default)", "");
    Setting& numberFirst_ = toggleSetting("numberFirst", "Percent in front of the name", false);
    Setting& sep_ = textSetting("sep", "Separator", "  ");
    Setting& hideBelow_ = slider("hideBelow", "Hide programs below (% CPU)", 0.f, 0.f, 20.f, "%.0f");
    Setting& dimRows_ = toggleSetting("dimRows", "Programs in a dimmer color", true);
    Setting& minWidth_ = slider("rowWidth", "Minimum width", 140.f, 0.f, 320.f, "%.0f");
    std::future<Sample> pending_;
    std::vector<Row> top_;
    std::map<DWORD, uint64_t> prev_;
    double prevAt_ = 0.0;
    double last_ = -10.0;
    double lastToast_ = -1000.0;
    float total_ = 0.f;
    int cores_ = 1;
};
