#include "modules/online/Transport.hpp"

using monchiOnline::safeTransport;

static_assert(safeTransport(true, false, L"example.com"));
static_assert(safeTransport(false, true, L"127.0.0.1"));
static_assert(safeTransport(false, true, L"localhost"));
static_assert(safeTransport(false, true, L"[::1]"));
static_assert(!safeTransport(false, true, L"example.com"));
static_assert(!safeTransport(false, true, L"localhost.example.com"));
static_assert(!safeTransport(false, true, L"127.0.0.1.example.com"));
static_assert(!safeTransport(false, true, L"192.168.1.10"));
static_assert(!safeTransport(false, false, L"localhost"));

int main() {}
