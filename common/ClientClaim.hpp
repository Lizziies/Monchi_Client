#pragma once

#include <windows.h>
#include <string>

class ClientClaim {
public:
    bool acquire() {
        auto name = L"Local\\MonchiClient-" + std::to_wstring(GetCurrentProcessId());
        handle_ = CreateMutexW(nullptr, FALSE, name.c_str());
        auto error = GetLastError();
        if (!handle_) return false;
        if (error != ERROR_ALREADY_EXISTS) return true;
        release();
        return false;
    }
    void release() {
        if (handle_) CloseHandle(handle_);
        handle_ = nullptr;
    }
    ~ClientClaim() { release(); }
    ClientClaim() = default;
    ClientClaim(const ClientClaim&) = delete;
    ClientClaim& operator=(const ClientClaim&) = delete;
private:
    HANDLE handle_ = nullptr;
};
