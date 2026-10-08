#pragma once
#include <cstdint>
uintptr_t testGuiSignature(const char* name);
#define GET_SIG_ADDRESS(name) testGuiSignature(name)
