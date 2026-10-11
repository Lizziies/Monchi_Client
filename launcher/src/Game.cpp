#include "Game.hpp"
#include "RemoteLoad.hpp"
#include "ClientImage.hpp"
#include "Files.hpp"

#include <shellapi.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <aclapi.h>
#include <sddl.h>

#include <vector>

namespace game {

namespace {

constexpr const wchar_t* exeName = L"Minecraft.Windows.exe";
constexpr const wchar_t* family = L"Microsoft.MinecraftUWP_8wekyb3d8bbwe";
constexpr const wchar_t* appIds[] = {L"Microsoft.MinecraftUWP_8wekyb3d8bbwe!Game", L"Microsoft.MinecraftUWP_8wekyb3d8bbwe!App"};

bool hasModule(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W m{sizeof(m)};
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &m); ok && !found; ok = Module32NextW(snap, &m))
        found = _wcsicmp(m.szModule, name) == 0;
    CloseHandle(snap);
    return found;
}

struct Search {
    DWORD pid;
    bool visible;
};

BOOL CALLBACK checkWindow(HWND w, LPARAM lp) {
    auto* s = reinterpret_cast<Search*>(lp);
    DWORD owner = 0;
    GetWindowThreadProcessId(w, &owner);
    if (owner == s->pid && IsWindowVisible(w)) s->visible = true;
    return !s->visible;
}

bool hasWindow(DWORD pid) {
    Search s{pid, false};
    EnumWindows(checkWindow, reinterpret_cast<LPARAM>(&s));
    return s.visible;
}

bool grantAppPackages(const std::filesystem::path& file) {
    PSID sid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-15-2-1", &sid)) return false;

    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
    access.grfAccessMode = GRANT_ACCESS;
    access.grfInheritance = NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);

    PACL oldAcl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    PACL newAcl = nullptr;
    bool ok = GetNamedSecurityInfoW(file.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &oldAcl,
                                    nullptr, &sd) == ERROR_SUCCESS &&
              SetEntriesInAclW(1, &access, oldAcl, &newAcl) == ERROR_SUCCESS &&
              SetNamedSecurityInfoW(const_cast<LPWSTR>(file.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr,
                                    nullptr, newAcl, nullptr) == ERROR_SUCCESS;
    if (newAcl) LocalFree(newAcl);
    if (sd) LocalFree(sd);
    LocalFree(sid);
    return ok;
}

}

std::string readableVersion(const std::string& raw) {
    int a = 0, b = 0, c = 0, d = 0;
    int count = sscanf(raw.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d);
    if (count < 3) return raw;
    if (c >= 100) { d = c % 100; c /= 100; count = 4; }
    auto version = std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c);
    return count == 4 ? version + "." + std::to_string(d) : version;
}

std::wstring runningPath(DWORD pid) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return {};
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD n = DWORD(std::size(buf));
    std::wstring out = QueryFullProcessImageNameW(proc, 0, buf, &n) ? std::wstring(buf, n) : std::wstring();
    CloseHandle(proc);
    return out;
}

std::optional<DWORD> running() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return std::nullopt;
    PROCESSENTRY32W p{sizeof(p)};
    std::optional<DWORD> pid;
    for (BOOL ok = Process32FirstW(snap, &p); ok && !pid; ok = Process32NextW(snap, &p))
        if (_wcsicmp(p.szExeFile, exeName) == 0) pid = p.th32ProcessID;
    CloseHandle(snap);
    return pid;
}

static std::wstring packageName() {
    using Fn = LONG(WINAPI*)(PCWSTR, UINT32*, PWSTR*, UINT32*, PWSTR);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetPackagesByPackageFamily"));
    if (!fn) return {};

    UINT32 count = 0, chars = 0;
    fn(family, &count, nullptr, &chars, nullptr);
    if (!count || !chars) return {};

    std::vector<PWSTR> names(count);
    std::vector<wchar_t> buffer(chars);
    if (fn(family, &count, names.data(), &chars, buffer.data()) != ERROR_SUCCESS || !count) return {};
    return names[0];
}

std::filesystem::path installFolder() {
    using Fn = LONG(WINAPI*)(PCWSTR, UINT32*, PWSTR);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetPackagePathByFullName"));
    std::wstring name = packageName();
    if (!fn || name.empty()) return {};
    UINT32 chars = 0;
    fn(name.c_str(), &chars, nullptr);
    if (!chars) return {};
    std::wstring path(chars, L'\0');
    if (fn(name.c_str(), &chars, path.data()) != ERROR_SUCCESS) return {};
    path.resize(wcslen(path.c_str()));
    return path;
}

std::string installedVersion() {
    std::string full = files::narrow(packageName());
    auto a = full.find('_');
    auto b = full.find('_', a + 1);
    if (a == std::string::npos || b == std::string::npos) return {};
    return readableVersion(full.substr(a + 1, b - a - 1));
}

bool launch() {
    IApplicationActivationManager* manager = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&manager));
    if (SUCCEEDED(hr)) {
        for (const wchar_t* id : appIds) {
            DWORD pid = 0;
            if (SUCCEEDED(manager->ActivateApplication(id, nullptr, AO_NOERRORUI, &pid))) {
                manager->Release();
                return true;
            }
        }
        manager->Release();
    }
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"minecraft:", nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool launchExe(const std::filesystem::path& exe, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(exe, ec)) {
        error = "The selected Minecraft version is not there anymore";
        return false;
    }
    std::wstring cmd = L"\"" + exe.wstring() + L"\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    auto dir = exe.parent_path();
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        error = "The selected Minecraft version could not be started";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool waitReady(DWORD pid, int timeoutMs) {
    HANDLE proc = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!proc) return false;
    DWORD start = GetTickCount();
    bool ready = false;
    while (GetTickCount() - start < DWORD(timeoutMs)) {
        if (WaitForSingleObject(proc, 0) == WAIT_OBJECT_0) break;
        if (hasWindow(pid) && (hasModule(pid, L"d3d12.dll") || hasModule(pid, L"d3d11.dll"))) {
            Sleep(2000);
            ready = true;
            break;
        }
        Sleep(250);
    }
    CloseHandle(proc);
    return ready;
}

bool injected(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W m{sizeof(m)};
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &m); ok && !found; ok = Module32NextW(snap, &m))
        found = isClientImage(m.szModule);
    CloseHandle(snap);
    return found;
}

bool inject(DWORD pid, const std::filesystem::path& dll, std::string& error) {
    if (injected(pid)) return true;
    if (!std::filesystem::exists(dll)) {
        error = "The client file is missing. Put Monchi.dll next to the launcher.";
        return false;
    }
    grantAppPackages(dll);
    // the client loads the Flarial core from its own folder inside the game process, which needs read access too
    if (auto core = dll.parent_path() / L"MonchiFlarial.dll"; std::filesystem::exists(core)) grantAppPackages(core);

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                  PROCESS_VM_WRITE | PROCESS_VM_READ,
                              FALSE, pid);
    if (!proc) {
        error = "Could not open the Minecraft process";
        return false;
    }

    std::wstring path = dll.wstring();
    size_t bytes = (path.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool ok = remote && WriteProcessMemory(proc, remote, path.c_str(), bytes, nullptr);

    HANDLE thread = nullptr;
    if (ok) {
        auto loader = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        thread = CreateRemoteThread(proc, nullptr, 0, loader, remote, 0, nullptr);
    }

    auto state = waitLoad(thread, 15000);
    if (thread) CloseHandle(thread);
    // A timed-out LoadLibrary may still be reading its argument in the game process.
    if (remote && state.finished) VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (!state.finished) {
        error = "The client is still loading. Wait before trying again.";
        return false;
    }
    if (!state.loaded) {
        error = "Minecraft refused the client";
        return false;
    }
    return true;
}

}
