#include "Music.hpp"
#include "MediaSession.hpp"
#include "MusicState.hpp"
#include "core/Bg.hpp"

#include <windows.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace music {

namespace {

struct Player {
    const char* name;
    const wchar_t* exe;
    const char* idle;
    const char* appId;
};

constexpr Player players[] = {
    {"Spotify", L"spotify.exe", "spotify", "spotify"},
    {"Apple Music", L"applemusic.exe", "apple music", "applemusic"},
    {"Deezer", L"deezer.exe", "deezer", "deezer"},
    {"Amazon Music", L"amazon music.exe", "amazon music", "amazon"},
};

std::atomic<int> users{0};
std::atomic<int> generation{0};
std::atomic<int> pending{-1};
std::mutex lock;
Track current;

std::wstring exeOf(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD n = MAX_PATH;
    std::wstring out;
    if (QueryFullProcessImageNameW(h, 0, buf, &n)) {
        out = buf;
        auto slash = out.find_last_of(L"\\/");
        if (slash != std::wstring::npos) out = out.substr(slash + 1);
        std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
    }
    CloseHandle(h);
    return out;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

struct Search {
    Track best;
};

BOOL CALLBACK visit(HWND w, LPARAM lp) {
    if (!IsWindowVisible(w)) return TRUE;
    auto* search = reinterpret_cast<Search*>(lp);
    wchar_t title[256] = {};
    if (GetWindowTextW(w, title, 256) <= 0) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    std::wstring exe = exeOf(pid);
    for (auto& p : players) {
        if (exe != p.exe) continue;
        std::string t = narrow(title);
        std::string low = t;
        std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        bool idle = low.find(p.idle) == 0 && low.find(" - ") == std::string::npos;
        if (!songTitle(t)) continue;
        if (!search->best.found || (!idle && !search->best.playing)) {
            search->best = {p.name, idle ? std::string() : t, !idle, true};
        }
    }
    return TRUE;
}

// Amazon Music and others keep their window title fixed; the song only shows in the Windows media controls
Track fromSessions() {
    std::vector<mediasession::Session> all;
    if (!mediasession::list(all)) return {};
    Track best;
    int bestScore = 0;
    for (auto& s : all) {
        std::string id = s.app;
        std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        for (auto& p : players) {
            if (id.find(p.appId) == std::string::npos) continue;
            bool titled = songTitle(s.title);
            int score = (s.playing ? 2 : 1) + (titled ? 4 : 0) + (s.current ? 8 : 0);
            if (score <= bestScore) break;
            bestScore = score;
            std::string title = !titled ? std::string() : s.artist.empty() ? s.title : s.artist + " - " + s.title;
            best = {p.name, title, s.playing, true};
            break;
        }
    }
    return best;
}

void sendKey(WORD vk) {
    INPUT in[2] = {};
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wVk = in[1].ki.wVk = vk;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

void adjustVolume(float delta, bool toggleMute, const std::string& playerName) {
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) return;
    IMMDevice* dev = nullptr;
    IAudioSessionManager2* mgr = nullptr;
    IAudioSessionEnumerator* se = nullptr;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) &&
        SUCCEEDED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&mgr))) &&
        SUCCEEDED(mgr->GetSessionEnumerator(&se))) {
        int count = 0;
        se->GetCount(&count);
        for (int i = 0; i < count; i++) {
            IAudioSessionControl* ctl = nullptr;
            if (FAILED(se->GetSession(i, &ctl))) continue;
            IAudioSessionControl2* ctl2 = nullptr;
            ISimpleAudioVolume* vol = nullptr;
            DWORD pid = 0;
            if (SUCCEEDED(ctl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&ctl2))) && SUCCEEDED(ctl2->GetProcessId(&pid)) &&
                SUCCEEDED(ctl->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&vol)))) {
                std::wstring exe = exeOf(pid);
                for (auto& p : players) {
                    if (exe != p.exe || (!playerName.empty() && playerName != p.name)) continue;
                    if (toggleMute) {
                        BOOL m = FALSE;
                        vol->GetMute(&m);
                        vol->SetMute(!m, nullptr);
                    } else {
                        float v = 0.f;
                        vol->GetMasterVolume(&v);
                        vol->SetMasterVolume(std::clamp(v + delta, 0.f, 1.f), nullptr);
                    }
                }
            }
            if (vol) vol->Release();
            if (ctl2) ctl2->Release();
            ctl->Release();
        }
    }
    if (se) se->Release();
    if (mgr) mgr->Release();
    if (dev) dev->Release();
    en->Release();
}

void loop(int gen) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    TrackState state;
    Search search;
    uint64_t windowAt = 0;
    while (generation == gen && users > 0) {
        uint64_t now = GetTickCount64();
        if (!windowAt || now - windowAt >= 1000) {
            search = {};
            EnumWindows(visit, reinterpret_cast<LPARAM>(&search));
            windowAt = now;
        }
        Track media = fromSessions();
        if (generation != gen || users <= 0) break;
        {
            std::scoped_lock g(lock);
            if (generation != gen || users <= 0) break;
            Track next = media.found ? media : search.best;
            current = state.update(std::move(next), GetTickCount64());
        }
        for (int i = 0; i < 5 && generation == gen && users > 0; i++) {
            int action = pending.exchange(-1);
            if (action >= 0) {
                std::string who;
                {
                    std::scoped_lock g(lock);
                    who = current.player;
                }
                switch (Action(action)) {
                case Action::PlayPause: sendKey(VK_MEDIA_PLAY_PAUSE); break;
                case Action::Next: sendKey(VK_MEDIA_NEXT_TRACK); break;
                case Action::Previous: sendKey(VK_MEDIA_PREV_TRACK); break;
                case Action::VolumeUp: adjustVolume(0.05f, false, who); break;
                case Action::VolumeDown: adjustVolume(-0.05f, false, who); break;
                case Action::Mute: adjustVolume(0.f, true, who); break;
                }
            }
            Sleep(50);
        }
    }
    mediasession::reset();
    CoUninitialize();
}

}

void use(bool on) {
    if (on) {
        if (users++ == 0) {
            int gen = ++generation;
            bg::run([gen] { loop(gen); });
        }
        return;
    }
    if (users > 0) users--;
}

Track now() {
    std::scoped_lock g(lock);
    return current;
}

void send(Action a) { pending = int(a); }

}
