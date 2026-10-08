#pragma once
#include "Files.hpp"
#include <windows.h>
#include <bcrypt.h>

namespace files {
inline std::string sha256(const fs::path& path) {
    std::string data = read(path);
    if (data.empty()) return {};
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    unsigned char digest[32]{};
    auto status = BCryptHash(alg, nullptr, 0, reinterpret_cast<PUCHAR>(data.data()), ULONG(data.size()), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) return {};
    static const char* hex = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
}
