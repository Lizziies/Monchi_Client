#include "Log.hpp"
#include "Paths.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <vector>

namespace logger {

static std::ofstream file;
static std::ofstream session;
static std::mutex lock;

static void prune(const std::filesystem::path& dir) {
    std::error_code ec;
    std::vector<std::filesystem::path> old;
    for (auto& e : std::filesystem::directory_iterator(dir, ec))
        if (e.path().filename().wstring().starts_with(L"session-")) old.push_back(e.path());
    std::sort(old.begin(), old.end());
    while (old.size() > 10) {
        std::filesystem::remove(old.front(), ec);
        old.erase(old.begin());
    }
}

void open() {
    auto dir = paths::logs();
    std::error_code ec;
    if (std::filesystem::exists(dir / L"latest.log"))
        std::filesystem::rename(dir / L"latest.log", dir / L"previous.log", ec);
    file.open(dir / L"latest.log", std::ios::out | std::ios::trunc);
    auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    auto name = std::format(L"session-{:%Y%m%d-%H%M%S}-{}.log", now, GetCurrentProcessId());
    session.open(dir / name, std::ios::out | std::ios::trunc);
    prune(dir);
}

void close() {
    std::scoped_lock g(lock);
    file.close();
    session.close();
}

void write(std::string_view level, std::string_view msg) {
    // the exception handlers log too; a thread already in here (a fault while formatting or writing) is turned away
    // instead of locking the same mutex twice
    thread_local bool writing = false;
    if (writing) return;
    struct Busy {
        bool& flag;
        ~Busy() { flag = false; }
    } busy{writing};
    writing = true;
    auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    auto line = std::format("[{:%H:%M:%S}] [{}] {}\n", now, level, msg);
    std::scoped_lock g(lock);
    if (file.is_open()) {
        file << line;
        file.flush();
    }
    if (session.is_open()) {
        session << line;
        session.flush();
    }
    OutputDebugStringA(line.c_str());
}

std::string narrow(std::wstring_view w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring widen(std::string_view s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

}
