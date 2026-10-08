#include "GuiScaleOption.hpp"
#include "Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include <windows.h>
#include <cstdint>

namespace guiScaleOption {

bool read(ClientInstance* client, int& value) {
    uintptr_t getter = GET_SIG_ADDRESS("ClientInstance::getGuiScaleOption");
    if (!client || !getter) return false;
    __try {
        uintptr_t option = reinterpret_cast<uintptr_t (*)(ClientInstance*)>(getter)(client);
        for (int depth = 0; option && depth < 32; depth++) {
            uintptr_t metadata = *reinterpret_cast<uintptr_t*>(option + 8);
            if (!metadata) return false;
            uintptr_t inherited = *reinterpret_cast<uintptr_t*>(metadata + 0x238);
            if (!inherited) {
                value = *reinterpret_cast<int*>(option + 0x18);
                return true;
            }
            option = inherited;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

bool set(ClientInstance* client, int value) {
    uintptr_t setter = GET_SIG_ADDRESS("ClientInstance::setGuiScaleOption");
    if (!client || !setter) return false;
    int before = 0;
    if (!read(client, before)) return false;
    if (before == value) return true;
    __try {
        // Slot 215 forwards edx to the integer option setter with change notification enabled.
        reinterpret_cast<void (*)(ClientInstance*, int)>(setter)(client, value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    int after = 0;
    return read(client, after) && after == value;
}

bool refresh(ClientInstance* client) {
    uintptr_t refresh = GET_SIG_ADDRESS("ClientInstance::refreshGuiLayout");
    if (!client || !refresh) return false;
    __try {
        // 1.26.52 wrapper 0x5dbc390 reads GuiData dimensions +0x40, obtains the screen factors from slot 263,
        // and calls the three-argument layout function at 0x5dbc530. It takes only ClientInstance in rcx.
        reinterpret_cast<void (*)(ClientInstance*)>(refresh)(client);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool repair(ClientInstance* client) {
    int value = 0;
    if (!read(client, value)) return false;
    if (value >= -16 && value <= 16) return true;
    return set(client, 0);
}

bool Override::apply(ClientInstance* client, int value) {
    if (owner_ && owner_ != client) return false;
    int before = 0;
    if (!read(client, before)) return false;
    if (owner_ && before != applied_) reset();
    const bool accepted = set(client, value);
    int after = 0;
    if (!read(client, after)) return false;
    // A rejected request may still have been clamped by the game's integer setter.
    if (after != before) {
        if (!owner_) {
            owner_ = client;
            original_ = before;
        }
        applied_ = after;
    }
    return accepted;
}

bool Override::restore(ClientInstance* client) {
    if (!owner_) return true;
    if (owner_ != client) return false;
    int current = 0;
    if (!read(client, current)) return false;
    // Preserve a manual Minecraft setting change made after the override.
    if (current == applied_ && !set(client, original_)) return false;
    reset();
    return true;
}

}
