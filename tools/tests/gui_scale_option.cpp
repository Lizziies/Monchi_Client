#include "flarial/SDK/Client/Core/GuiScaleOption.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace {
alignas(8) std::array<uint8_t, 0x240> metadata{};
alignas(8) std::array<uint8_t, 0x20> option{};
bool getterPresent = true;
bool setterPresent = true;
int setterCalls = 0;
int refreshCalls = 0;
bool refreshPresent = true;
ClientInstance* refreshed = nullptr;
void refresh(ClientInstance* client) { refreshCalls++; refreshed = client; }
bool clampRequests = false;

template<class T> void put(uint8_t* at, T value) { std::memcpy(at, &value, sizeof(value)); }
uintptr_t get(ClientInstance*) { return reinterpret_cast<uintptr_t>(option.data()); }
void set(ClientInstance*, int value) {
    setterCalls++;
    if (clampRequests) value = std::clamp(value, -2, 0);
    if (value < -2 || value > 0) return;
    put(option.data() + 0x18, value);
}
void check(bool ok) { if (!ok) std::abort(); }
}

uintptr_t testGuiSignature(const char* name) {
    if (std::strcmp(name, "ClientInstance::getGuiScaleOption") == 0)
        return getterPresent ? reinterpret_cast<uintptr_t>(&get) : 0;
    if (std::strcmp(name, "ClientInstance::refreshGuiLayout") == 0)
        return refreshPresent ? reinterpret_cast<uintptr_t>(&refresh) : 0;
    return setterPresent ? reinterpret_cast<uintptr_t>(&set) : 0;
}

int main() {
    auto* client = reinterpret_cast<ClientInstance*>(1);
    check(guiScaleOption::refresh(client) && refreshCalls == 1 && refreshed == client);
    check(!guiScaleOption::refresh(nullptr) && refreshCalls == 1);
    refreshPresent = false;
    check(!guiScaleOption::refresh(client) && refreshCalls == 1);
    refreshPresent = true;
    put(option.data() + 8, reinterpret_cast<uintptr_t>(metadata.data()));
    put(option.data() + 0x18, -1);
    int value = 42;
    check(guiScaleOption::read(client, value) && value == -1);
    check(!guiScaleOption::read(nullptr, value));
    getterPresent = false;
    check(!guiScaleOption::read(client, value));
    getterPresent = true;
    check(guiScaleOption::set(client, -1) && setterCalls == 0);
    check(guiScaleOption::set(client, -2) && setterCalls == 1);
    check(!guiScaleOption::set(client, 8) && setterCalls == 2);
    setterPresent = false;
    check(!guiScaleOption::set(client, 0));
    setterPresent = true;
    put(metadata.data() + 0x238, reinterpret_cast<uintptr_t>(option.data()));
    check(!guiScaleOption::read(client, value));
    check(!guiScaleOption::set(client, 0) && setterCalls == 2);
    put(metadata.data() + 0x238, uintptr_t(0));
    put(option.data() + 8, uintptr_t(1));
    check(!guiScaleOption::read(client, value));
    check(!guiScaleOption::set(client, 0) && setterCalls == 2);
    put(option.data() + 8, reinterpret_cast<uintptr_t>(metadata.data()));
    put(option.data() + 0x18, -1);
    put(option.data() + 0x18, -219);
    check(guiScaleOption::repair(client));
    check(guiScaleOption::read(client, value) && value == 0);
    int callsAfterRepair = setterCalls;
    check(guiScaleOption::repair(client) && setterCalls == callsAfterRepair);
    put(option.data() + 0x18, -335);
    setterPresent = false;
    check(!guiScaleOption::repair(client));
    setterPresent = true;
    check(guiScaleOption::repair(client));
    put(option.data() + 0x18, -1);
    guiScaleOption::Override change;
    check(change.apply(client, -2) && change.active());
    check(change.apply(client, 0));
    check(change.restore(client) && !change.active());
    check(guiScaleOption::read(client, value) && value == -1);
    check(change.apply(client, -2));
    setterPresent = false;
    check(!change.restore(client) && change.active());
    setterPresent = true;
    check(change.restore(client) && !change.active());
    check(change.apply(client, -2));
    put(option.data() + 0x18, 0);
    check(change.restore(client) && !change.active());
    check(guiScaleOption::read(client, value) && value == 0);
    check(change.apply(client, -1));
    check(!change.apply(reinterpret_cast<ClientInstance*>(2), -2));
    check(!change.restore(reinterpret_cast<ClientInstance*>(2)) && change.active());
    check(change.restore(client));
    put(option.data() + 0x18, -1);
    check(change.apply(client, -1) && !change.active());
    check(!change.apply(client, 9) && !change.active());
    clampRequests = true;
    check(!change.apply(client, 9) && change.active());
    check(guiScaleOption::read(client, value) && value == 0);
    check(change.restore(client));
    check(guiScaleOption::read(client, value) && value == -1);
}
