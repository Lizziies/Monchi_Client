#pragma once

#include <string_view>

namespace monchiOnline {

constexpr bool safeTransport(bool https, bool http, std::wstring_view host) {
    return https || (http && (host == L"localhost" || host == L"127.0.0.1" || host == L"[::1]" || host == L"::1"));
}

}
