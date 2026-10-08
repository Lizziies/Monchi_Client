#include "sdk/Dimension.hpp"
#include "modules/common/WaypointScope.hpp"
#include <array>
#include <cstring>
#include <cstdlib>

namespace {
void check(bool ok) { if (!ok) std::abort(); }
template<class T> void put(uint8_t* at, T value) { std::memcpy(at, &value, sizeof(value)); }
}

int main() {
    alignas(8) std::array<uint8_t, 0x200> actor{}, dimension{};
    std::array<uintptr_t, 3> table{};
    const auto player = reinterpret_cast<uintptr_t>(actor.data());
    put(actor.data() + 0x1c8, reinterpret_cast<uintptr_t>(dimension.data()));
    put(dimension.data(), reinterpret_cast<uintptr_t>(table.data()));
    table[2] = 101;
    for (int id = 0; id < 3; id++) {
        put(dimension.data() + 0x1a0, id);
        auto got = dimensionRead::id(player, 100, 0x1c8, 1, 0x1a0);
        check(got && *got == id);
    }
    put(dimension.data() + 0x1a0, -1);
    check(!dimensionRead::id(player, 100, 0x1c8, 1, 0x1a0));
    put(dimension.data() + 0x1a0, 3);
    check(!dimensionRead::id(player, 100, 0x1c8, 1, 0x1a0));
    table[2] = 0;
    check(!dimensionRead::id(player, 100, 0x1c8, 1, 0x1a0));
    check(!dimensionRead::id(1, 100, 0x1c8, 1, 0x1a0));
    check(!dimensionRead::id(player, 100, -1, 1, 0x1a0));
    check(waypointScope::matches("world:a", "world:a", true));
    check(!waypointScope::matches("world:a", "world:b", true));
    check(!waypointScope::matches("server-a", "server-b", true));
    check(!waypointScope::matches("world:a", "", true));
    check(waypointScope::matches("", "world:a", true));
    check(waypointScope::matches("world:a", "world:b", false));
}
