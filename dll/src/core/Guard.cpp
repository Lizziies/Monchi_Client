#include "Guard.hpp"
#include "Log.hpp"
#include "sdk/Memory.hpp"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>

namespace guard {

static std::mutex lock;
static std::optional<Fault> lastFault;
static thread_local bool failed = false;
static PVOID netHandle = nullptr;
static HMODULE self = nullptr;

static void store(const char* where, std::string what) {
    logger::error("fault in {}: {}", where, what);
    std::scoped_lock g(lock);
    lastFault = Fault{where, std::move(what)};
}

void report(const char* where, const char* what) {
    failed = true;
    store(where, what);
}

#ifdef _MSC_VER
static int filter(unsigned code, const char* where) {
    store(where, std::format("exception 0x{:08X}", code));
    return EXCEPTION_EXECUTE_HANDLER;
}

bool run(const char* where, void (*fn)(void*), void* ctx) {
    failed = false;
    __try {
        fn(ctx);
    } __except (filter(GetExceptionCode(), where)) {
        return false;
    }
    return !failed;
}
#else
bool run(const char*, void (*fn)(void*), void* ctx) {
    failed = false;
    fn(ctx);
    return !failed;
}
#endif

const Fault* last() {
    std::scoped_lock g(lock);
    return lastFault ? &*lastFault : nullptr;
}

void clearLast() {
    std::scoped_lock g(lock);
    lastFault.reset();
}

static std::atomic<int> reported{0};

// Faults inside the game or a system DLL often start in our code. Unwinding is not reliable from a vectored
// handler, so the stack is scanned for values that point into our image; offsets map to symbols with objdump.
static std::string callers(uintptr_t sp, HMODULE image = self, const char* tag = "monchi") {
    if (!image) return {};
    auto base = reinterpret_cast<uintptr_t>(image);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    auto top = reinterpret_cast<uintptr_t>(tib->StackBase);
    std::string out;
    int found = 0;
    for (uintptr_t p = sp & ~uintptr_t(7); p + 8 <= top && p < sp + 0x4000 && found < 10; p += 8) {
        uintptr_t v = *reinterpret_cast<uintptr_t*>(p);
        if (v <= base + 0x1000 || v >= end) continue;
        out += std::format("{}{}+0x{:X}", found ? " " : "", tag, v - base);
        found++;
    }
    return out;
}

static std::atomic<int> thrown{0};

// MSVC C++ throw: ExceptionInformation = {magic, object, ThrowInfo rva-based, image base}. The Flarial core is not
// under Monchi's guard on game threads, so an exception nobody catches there ends the game; log where it came from.
static void cppThrow(const EXCEPTION_RECORD& rec, uintptr_t sp) {
    if (rec.NumberParameters < 4) return;
    auto image = reinterpret_cast<HMODULE>(rec.ExceptionInformation[3]);
    HMODULE core = GetModuleHandleW(L"MonchiFlarial.dll");
    if (!core || image != core || ++thrown > 30) return;
    auto base = rec.ExceptionInformation[3];
    auto* info = reinterpret_cast<const int*>(rec.ExceptionInformation[2]);
    std::string type = "?";
    if (info && info[3]) {
        auto* types = reinterpret_cast<const int*>(base + info[3]);
        if (types[0] > 0) {
            auto* catchable = reinterpret_cast<const int*>(base + types[1]);
            type = reinterpret_cast<const char*>(base + catchable[1] + 0x10);
        }
    }
    // only a std::exception keeps its text at +8 (the list of catchable types names every base); other types that
    // merely have "error" in their name (winrt::hresult_error) hold something else there
    bool standard = false;
    if (info && info[3]) {
        auto* types = reinterpret_cast<const int*>(base + info[3]);
        for (int k = 0; k < types[0] && k < 8; k++) {
            auto* catchable = reinterpret_cast<const int*>(base + types[1 + k]);
            if (std::strcmp(reinterpret_cast<const char*>(base + catchable[1] + 0x10), ".?AVexception@std@@") == 0) standard = true;
        }
    }
    std::string what;
    if (standard) {
        auto* text = *reinterpret_cast<const char* const*>(rec.ExceptionInformation[1] + 8);
        if (text) what.assign(text, strnlen(text, 200));
    }
    void* frames[48];
    USHORT count = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
    auto lo = reinterpret_cast<uintptr_t>(core);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(lo + reinterpret_cast<IMAGE_DOS_HEADER*>(lo)->e_lfanew);
    uintptr_t hi = lo + nt->OptionalHeader.SizeOfImage;
    std::string trail;
    for (USHORT i = 0; i < count; i++) {
        auto at = reinterpret_cast<uintptr_t>(frames[i]);
        if (at >= lo && at < hi) trail += std::format(" core+0x{:X}", at - lo);
    }
    logger::warn("flarial core throws {} \"{}\" from{}", type, what, trail.empty() ? callers(sp, core, "core") : trail);
}

static void logThrow(const EXCEPTION_RECORD* rec, uintptr_t sp) {
    __try {
        cppThrow(*rec, sp);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Anything raised while this handler runs comes straight back into it. Without the flag a fault in the logging
// itself would recurse; with it the nested exception is passed on untouched.
static thread_local bool handling = false;

static LONG CALLBACK netBody(EXCEPTION_POINTERS* info);

static LONG CALLBACK net(EXCEPTION_POINTERS* info) {
    if (handling) return EXCEPTION_CONTINUE_SEARCH;
    handling = true;
    LONG r = EXCEPTION_CONTINUE_SEARCH;
    __try {
        r = netBody(info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    handling = false;
    return r;
}

static LONG CALLBACK netBody(EXCEPTION_POINTERS* info) {
    auto code = info->ExceptionRecord->ExceptionCode;
    if (code == 0xE06D7363) {
        logThrow(info->ExceptionRecord, info->ContextRecord->Rsp);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code == 0xC0000374) {
        static std::atomic<int> heapReports{0};
        if (heapReports++ == 0) {
            HMODULE core = GetModuleHandleW(L"MonchiFlarial.dll");
            logger::error("heap corruption detected on thread {}, monchi {} core {}", GetCurrentThreadId(), callers(info->ContextRecord->Rsp),
                          core ? callers(info->ContextRecord->Rsp, core, "core") : std::string("-"));
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;
    // a guarded read that tried an address which is gone; its own handler takes it
    if (mem::probing) return EXCEPTION_CONTINUE_SEARCH;

    HMODULE owner = nullptr;
    auto at = reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress);
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(at), &owner);
    std::string trail = callers(info->ContextRecord->Rsp);
    if (owner != self && trail.empty()) return EXCEPTION_CONTINUE_SEARCH;
    if (++reported > 4) return EXCEPTION_CONTINUE_SEARCH;

    wchar_t path[MAX_PATH] = L"?";
    if (owner) GetModuleFileNameW(owner, path, MAX_PATH);
    std::wstring name = path;
    name = name.substr(name.find_last_of(L"\\/") + 1);
    logger::error("fault 0x{:08X} at {}+0x{:X}, access {} 0x{:X}, called from {}", (unsigned)code,
                  logger::narrow(name), at - reinterpret_cast<uintptr_t>(owner),
                  info->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
                  info->ExceptionRecord->ExceptionInformation[1], trail.empty() ? "-" : trail);
    return EXCEPTION_CONTINUE_SEARCH;
}

static std::string place(uintptr_t at) {
    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(at), &owner) || !owner)
        return std::format("0x{:X}", at);
    wchar_t path[MAX_PATH] = L"?";
    GetModuleFileNameW(owner, path, MAX_PATH);
    std::wstring name = path;
    name = name.substr(name.find_last_of(L"\\/") + 1);
    return std::format("{}+0x{:X}", logger::narrow(name), at - reinterpret_cast<uintptr_t>(owner));
}

static std::string trail(void* const* frames, int count) {
    std::string out;
    for (int i = 0; i < count; i++) out += (i ? " " : "") + place(reinterpret_cast<uintptr_t>(frames[i]));
    return out;
}

// abort() ends the game with a fast fail that no exception handler sees (0xC0000409 in ucrtbase, no dump, no log
// line). The runtime raises SIGABRT first, so a handler there is the one place the caller can still be written down.
using SignalHandler = void (*)(int);
using SignalFn = SignalHandler (*)(int, SignalHandler);
static SignalFn runtimeSignal = nullptr;
static SignalHandler previousAbort = nullptr;
constexpr int abortSignal = 22;

static void onAbort(int) {
    void* frames[48];
    USHORT count = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
    logger::error("the game is being aborted on thread {}: {}", GetCurrentThreadId(), trail(frames, count));
}

static std::atomic<ULONGLONG> lastBeat{0};
static std::atomic<DWORD> beatThread{0};
static std::atomic<bool> watching{false};
static HANDLE watcher = nullptr;

static int walk(DWORD thread, void** frames, int capacity) {
    HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, thread);
    if (!t) return 0;
    int count = 0;
    if (SuspendThread(t) != DWORD(-1)) {
        CONTEXT c{};
        c.ContextFlags = CONTEXT_FULL;
        bool have = GetThreadContext(t, &c);
        // Let it run again before the stack is read. The lookup below takes the lock of the system's function tables;
        // a thread stopped while holding it would stop this one too, and the game would never come back. A thread
        // that has stood still for eight seconds leaves its stack as it is, one that moves on gives a wrong trail,
        // which is the cheaper mistake.
        ResumeThread(t);
        if (have) {
            __try {
                while (count < capacity && c.Rip) {
                    frames[count++] = reinterpret_cast<void*>(c.Rip);
                    DWORD64 base = 0;
                    if (auto* fn = RtlLookupFunctionEntry(c.Rip, &base, nullptr)) {
                        void* data = nullptr;
                        DWORD64 frame = 0;
                        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, fn, &c, &data, &frame, nullptr);
                    } else {
                        c.Rip = *reinterpret_cast<DWORD64*>(c.Rsp);
                        c.Rsp += 8;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }
    CloseHandle(t);
    return count;
}

// nothing is allocated while the other thread is suspended (it may hold the heap), the text is built afterwards
static DWORD WINAPI watch(void*) {
    int reports = 0;
    bool stalled = false;
    ULONGLONG since = 0;
    while (watching) {
        Sleep(500);
        ULONGLONG last = lastBeat.load(), now = GetTickCount64();
        if (!last) continue;
        if (now - last < 8000) {
            if (stalled) logger::warn("frames are back after {} s", (now - since) / 1000);
            stalled = false;
            continue;
        }
        if (stalled) continue;
        stalled = true;
        since = last;
        if (reports++ >= 5) continue;
        void* frames[48];
        int count = walk(beatThread.load(), frames, 48);
        logger::warn("no frame for {} s, the drawing thread {} stands at: {}", (now - last) / 1000, beatThread.load(), trail(frames, count));
    }
    return 0;
}

void beat() {
    lastBeat = GetTickCount64();
    beatThread = GetCurrentThreadId();
}

void installNet() {
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&installNet), &self);
    netHandle = AddVectoredExceptionHandler(1, net);
    if (HMODULE runtime = GetModuleHandleW(L"ucrtbase.dll")) runtimeSignal = reinterpret_cast<SignalFn>(GetProcAddress(runtime, "signal"));
    if (runtimeSignal) previousAbort = runtimeSignal(abortSignal, onAbort);
    watching = true;
    watcher = CreateThread(nullptr, 0, watch, nullptr, 0, nullptr);
}

void removeNet() {
    watching = false;
    if (watcher) {
        WaitForSingleObject(watcher, 2000);
        CloseHandle(watcher);
        watcher = nullptr;
    }
    if (runtimeSignal) runtimeSignal(abortSignal, previousAbort);
    runtimeSignal = nullptr;
    if (netHandle) RemoveVectoredExceptionHandler(netHandle);
    netHandle = nullptr;
}

}
