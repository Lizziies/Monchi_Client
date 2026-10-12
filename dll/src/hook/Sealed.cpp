#include "Sealed.hpp"
#include "Hook.hpp"
#include "Vault.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "cosmetics/Cosmetics.hpp"

#include <windows.h>

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace sealed {

namespace {

using CreateW = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateA = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using Read = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);

CreateW oCreateW = nullptr;
CreateA oCreateA = nullptr;
Read oRead = nullptr;

struct Entry {
    HANDLE handle;
    FILE_ID_INFO file;
    std::string name;
    bool unsealed = false;
};

// Handles are never seen closing, so a number can come back for another file. Each entry remembers which file it
// was opened on and is dropped the moment the handle turns out to be something else.
constexpr size_t most = 256;
std::array<std::atomic<HANDLE>, most> known{};
std::atomic<int> count{0};
std::mutex lock;
std::vector<Entry> entries;

void publish() {
    for (size_t i = 0; i < most; i++) known[i].store(i < entries.size() ? entries[i].handle : nullptr, std::memory_order_relaxed);
    count.store(int(entries.size()), std::memory_order_release);
}

// "\\monchi\\cosmetics\\": the end of the data folder is enough to know the path, whatever stands before it
std::string folder;

template <class Ch>
std::string sealedName(const Ch* path) {
    std::string lower;
    for (; *path; path++) {
        auto c = *path;
        lower += c == '/' ? '\\' : c >= 'A' && c <= 'Z' ? char(c + 32) : c > 0 && c < 128 ? char(c) : '?';
    }
    size_t at = lower.rfind(folder);
    if (at == std::string::npos || !lower.ends_with(".png")) return {};
    return vault::named(lower.substr(at + folder.size()));
}

void note(HANDLE handle, std::string name) {
    Entry e{handle, {}, std::move(name)};
    if (!GetFileInformationByHandleEx(handle, FileIdInfo, &e.file, sizeof(e.file))) return;
    std::scoped_lock g(lock);
    std::erase_if(entries, [&](const Entry& old) { return old.handle == handle; });
    if (entries.size() >= most) entries.erase(entries.begin());
    entries.push_back(std::move(e));
    publish();
}

HANDLE WINAPI createW(LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD how, DWORD flags, HANDLE tmpl) {
    HANDLE handle = oCreateW(path, access, share, sa, how, flags, tmpl);
    if (handle == INVALID_HANDLE_VALUE || !path || cosmetics::reading) return handle;
    DWORD error = GetLastError();
    if (auto name = sealedName(path); !name.empty()) note(handle, std::move(name));
    SetLastError(error);
    return handle;
}

HANDLE WINAPI createA(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD how, DWORD flags, HANDLE tmpl) {
    HANDLE handle = oCreateA(path, access, share, sa, how, flags, tmpl);
    if (handle == INVALID_HANDLE_VALUE || !path || cosmetics::reading) return handle;
    DWORD error = GetLastError();
    if (auto name = sealedName(path); !name.empty()) note(handle, std::move(name));
    SetLastError(error);
    return handle;
}

bool tracked(HANDLE handle) {
    int n = count.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++)
        if (known[size_t(i)].load(std::memory_order_relaxed) == handle) return true;
    return false;
}

// the entry for this handle if it still is the file it was opened on
bool entryOf(HANDLE handle, std::string& name, bool& unsealed) {
    FILE_ID_INFO now{};
    bool same = GetFileInformationByHandleEx(handle, FileIdInfo, &now, sizeof(now)) != FALSE;
    std::scoped_lock g(lock);
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->handle != handle) continue;
        if (!same || std::memcmp(&now, &it->file, sizeof(now)) != 0) {
            entries.erase(it);
            publish();
            return false;
        }
        name = it->name;
        unsealed = it->unsealed;
        return true;
    }
    return false;
}

void markUnsealed(HANDLE handle) {
    std::scoped_lock g(lock);
    for (auto& e : entries)
        if (e.handle == handle) e.unsealed = true;
}

BOOL WINAPI readFile(HANDLE handle, LPVOID buffer, DWORD size, LPDWORD got, LPOVERLAPPED overlapped) {
    std::string name;
    bool unsealed = false;
    if (!count.load(std::memory_order_relaxed) || !tracked(handle) || !entryOf(handle, name, unsealed) || unsealed)
        return oRead(handle, buffer, size, got, overlapped);

    uint64_t at = 0;
    if (overlapped) at = uint64_t(overlapped->Offset) | uint64_t(overlapped->OffsetHigh) << 32;
    else {
        LARGE_INTEGER here{};
        if (!SetFilePointerEx(handle, {}, &here, FILE_CURRENT)) return oRead(handle, buffer, size, got, overlapped);
        at = uint64_t(here.QuadPart);
    }
    BOOL ok = oRead(handle, buffer, size, got, overlapped);
    DWORD error = GetLastError();
    if (!ok) {
        static std::atomic<bool> told{false};
        if (overlapped && error == ERROR_IO_PENDING && !told.exchange(true))
            logger::warn("cosmetics: the game reads a sealed texture in the background, it cannot be opened for it");
        SetLastError(error);
        return ok;
    }
    size_t read = got ? *got : overlapped ? size_t(overlapped->InternalHigh) : 0;
    auto* bytes = static_cast<uint8_t*>(buffer);
    if (at == 0 && vault::isPng(bytes, read)) {
        markUnsealed(handle);
    } else if (read) {
        vault::apply(name, at, bytes, read);
        static std::atomic<bool> told{false};
        if (!told.exchange(true)) logger::info("cosmetics: sealed textures are opened in memory as the game reads them");
    }
    SetLastError(error);
    return ok;
}

}

void install() {
    folder = "\\";
    for (wchar_t c : paths::root().filename().wstring()) folder += c >= 'A' && c <= 'Z' ? char(c + 32) : c < 128 ? char(c) : '?';
    folder += "\\cosmetics\\";
    hook::create("CreateFileW", hook::exported(L"kernelbase.dll", "CreateFileW"), createW, &oCreateW);
    hook::create("CreateFileA", hook::exported(L"kernelbase.dll", "CreateFileA"), createA, &oCreateA);
    hook::create("ReadFile", hook::exported(L"kernelbase.dll", "ReadFile"), readFile, &oRead);
}

}
