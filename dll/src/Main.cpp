#include "core/Client.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) client::start(module);
    return TRUE;
}
