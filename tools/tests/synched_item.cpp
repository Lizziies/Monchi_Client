#include "SynchedItem.hpp"

#include <array>
#include <cstring>

int main() {
    std::array<uint8_t, 16> item{};
    item[8] = 2;
    item[10] = 55;
    int32_t ticks = 37;
    std::memcpy(item.data() + 12, &ticks, sizeof(ticks));
    if (synchedItem::integer(item, 55) != ticks) return 1;
    if (synchedItem::integer(item, 4)) return 2;
    if (synchedItem::integer(std::span(item).first(15), 55)) return 3;
    item[8] = 3;
    if (synchedItem::integer(item, 55)) return 4;
    item[8] = 2;
    ticks = -1;
    std::memcpy(item.data() + 12, &ticks, sizeof(ticks));
    if (synchedItem::integer(item, 55) != -1) return 5;
    return 0;
}
