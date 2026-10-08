#include "Hook.hpp"
#include "core/Log.hpp"

#include <windows.h>
#include <MinHook.h>

namespace hook {

bool init() {
    auto s = MH_Initialize();
    if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) {
        logger::error("minhook init failed: {}", MH_StatusToString(s));
        return false;
    }
    return true;
}

// Overlays such as RTSS copy our patched prologue (a jump to a MinHook relay) into their own trampolines. Freeing
// the relays on unload sent those copies into freed memory; parked relays forward to the original instead, at the
// cost of a few kilobytes that stay allocated.
void shutdown() {
    disableAll();
    MH_Park();
}

bool create(const char* name, void* target, void* detour, void** original) {
    if (!target) {
        logger::warn("hook {}: no target", name);
        return false;
    }
    auto s = MH_CreateHook(target, detour, original);
    if (s != MH_OK) {
        logger::error("hook {}: {}", name, MH_StatusToString(s));
        return false;
    }
    logger::info("hook {} at {}", name, target);
    return true;
}

bool enableAll() {
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}

void disableAll() {
    MH_DisableHook(MH_ALL_HOOKS);
}

void* vfunc(void* object, int index) {
    if (!object) return nullptr;
    auto vtable = *static_cast<void***>(object);
    return vtable[index];
}

void* exported(const wchar_t* module, const char* name) {
    HMODULE m = GetModuleHandleW(module);
    if (!m) m = LoadLibraryW(module);
    if (!m) return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(m, name));
}

}
