#pragma once
#include "ReleaseKey.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <string_view>
#include <string>

namespace update {
inline bool verifySignature(std::string_view manifest, std::string_view signature, std::string_view tag) {
    if (manifest.size() > 16384 || signature.size() != 64 || !manifest.starts_with(std::string("version ") + std::string(tag) + "\n")) return false;
    BCRYPT_ALG_HANDLE hashAlg = nullptr, ecAlg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    unsigned char digest[32]{};
    if (BCryptOpenAlgorithmProvider(&hashAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    auto result = BCryptHash(hashAlg, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(manifest.data())), ULONG(manifest.size()), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(hashAlg, 0);
    if (result < 0 || BCryptOpenAlgorithmProvider(&ecAlg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) < 0) return false;
    unsigned char blob[sizeof(BCRYPT_ECCKEY_BLOB) + 64]{};
    BCRYPT_ECCKEY_BLOB head{BCRYPT_ECDSA_PUBLIC_P256_MAGIC, 32};
    std::memcpy(blob, &head, sizeof(head));
    std::memcpy(blob + sizeof(head), releaseKey.data(), releaseKey.size());
    result = BCryptImportKeyPair(ecAlg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob, sizeof(blob), 0);
    if (result >= 0) {
        result = BCryptVerifySignature(key, nullptr, digest, sizeof(digest), reinterpret_cast<PUCHAR>(const_cast<char*>(signature.data())), ULONG(signature.size()), 0);
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(ecAlg, 0);
    return result >= 0;
}
}
