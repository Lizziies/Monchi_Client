#pragma once

namespace guard {
inline void withCleanup(void (*work)(void*), void (*cleanup)(void*), void* context) {
#ifdef _MSC_VER
    __try { work(context); }
    __finally { cleanup(context); }
#else
    try { work(context); }
    catch (...) { cleanup(context); throw; }
    cleanup(context);
#endif
}
}
