#pragma once

#include <string>

namespace hook {

bool init();
void shutdown();

bool create(const char* name, void* target, void* detour, void** original);

template <class T>
bool create(const char* name, void* target, T* detour, T** original) {
    return create(name, target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(original));
}

bool enableAll();
void disableAll();

void* vfunc(void* object, int index);
void* exported(const wchar_t* module, const char* name);

}
