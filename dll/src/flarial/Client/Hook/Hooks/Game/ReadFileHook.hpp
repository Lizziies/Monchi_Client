// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Game/ReadFileHook.hpp: on 1.26.52 the path argument is no longer a Core::Path
// holding a std::string but a 16 byte view {const char* data, size_t size} (the callers build it from a string's pointer
// and size, checked at several call sites of the wrapper at 0x2cbc20), and the result is a struct whose first 0x20 bytes
// are the string with a success flag at +0x40, not a bare std::string.
#pragma once

#include "../Hook.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include "src/Utils/path.hpp"
#include <string>
#include <cstring>
#include "filesystem"
#include "src/Client/Events/Game/ReadFileEvent.hpp"

class ReadFileHook : public Hook {
private:
    struct PathView {
        const char *data;
        size_t size;
    };

    static bool copyPath(const PathView *view, char *out, size_t cap, size_t &len) {
        if (!view) return false;
        __try {
            len = view->size;
            if (!view->data || len == 0 || len >= cap) return false;
            memcpy(out, view->data, len);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static std::string *readFile(void *This, std::string *retstr, const PathView *path) {
        std::string *result = funcOriginal(This, retstr, path);

        char buf[1024];
        size_t len = 0;
        if (!copyPath(path, buf, sizeof(buf), len)) return result;

        Core::Path corePath(std::string(buf, len));
        auto event = nes::make_holder<ReadFileEvent>(This, retstr, corePath, result);
        eventMgr.trigger(event);

        return event->result;
    }

public:
    typedef std::string *(__thiscall *original)(void *This, std::string *retstr, const PathView *path);
    static inline original funcOriginal = nullptr;

    ReadFileHook() : Hook("ReadFileHook", GET_SIG_ADDRESS("AppPlatform::readAssetFile")) {}

    void enableHook() override {
        this->autoHook((void *)readFile, (void **)&funcOriginal);
    }
};
