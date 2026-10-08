#pragma once

namespace perf {

constexpr float refreshCap(float hz) {
    return hz > 12.f ? hz - 2.f : hz;
}

constexpr float localCap(float requested, bool foreign, bool focused) {
    return foreign && focused ? 0.f : requested;
}

}
