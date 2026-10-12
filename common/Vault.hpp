#pragma once

#include "VaultKey.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

// Cosmetics are stored sealed: every file is ChaCha20 over its bytes, keyed by the build and by its own name, so a
// file can be opened at any offset and is as long sealed as it is open. Nothing here writes an open file anywhere.
namespace vault {

inline uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

inline void quarter(uint32_t* s, int a, int b, int c, int d) {
    s[a] += s[b]; s[d] = rotl(s[d] ^ s[a], 16);
    s[c] += s[d]; s[b] = rotl(s[b] ^ s[c], 12);
    s[a] += s[b]; s[d] = rotl(s[d] ^ s[a], 8);
    s[c] += s[d]; s[b] = rotl(s[b] ^ s[c], 7);
}

inline void block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, uint8_t out[64]) {
    uint32_t in[16] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
    std::memcpy(in + 4, key, 32);
    in[12] = counter;
    std::memcpy(in + 13, nonce, 12);
    uint32_t s[16];
    std::memcpy(s, in, sizeof(s));
    for (int round = 0; round < 10; round++) {
        quarter(s, 0, 4, 8, 12); quarter(s, 1, 5, 9, 13); quarter(s, 2, 6, 10, 14); quarter(s, 3, 7, 11, 15);
        quarter(s, 0, 5, 10, 15); quarter(s, 1, 6, 11, 12); quarter(s, 2, 7, 8, 13); quarter(s, 3, 4, 9, 14);
    }
    for (int i = 0; i < 16; i++) s[i] += in[i];
    std::memcpy(out, s, 64);
}

inline void crypt(const uint8_t key[32], const uint8_t nonce[12], uint64_t offset, uint8_t* data, size_t size) {
    uint8_t stream[64];
    uint64_t have = ~0ull;
    for (size_t i = 0; i < size; i++, offset++) {
        if (offset / 64 != have) {
            have = offset / 64;
            block(key, nonce, uint32_t(have), stream);
        }
        data[i] ^= stream[offset % 64];
    }
}

// names are compared the way the packer writes them: lower case, forward slashes
inline std::string named(std::string_view name) {
    std::string out(name);
    for (char& c : out) c = c == '\\' ? '/' : c >= 'A' && c <= 'Z' ? char(c + 32) : c;
    return out;
}

inline std::array<uint8_t, 12> nonceOf(std::string_view name) {
    uint64_t a = 0xcbf29ce484222325ull, b = 0x84222325cbf29ce4ull;
    for (unsigned char c : named(name)) {
        a = (a ^ c) * 0x100000001b3ull;
        b = (b ^ c) * 0x100000001b3ull + 0x9e3779b97f4a7c15ull;
    }
    std::array<uint8_t, 12> out;
    std::memcpy(out.data(), &a, 8);
    std::memcpy(out.data() + 8, &b, 4);
    return out;
}

inline void key(uint8_t out[32]) {
    const volatile unsigned char* sealed = vaultkey::sealed;
    for (int i = 0; i < 32; i++) out[i] = uint8_t(sealed[i] ^ uint8_t(vaultkey::base + i * vaultkey::step));
}

inline void apply(std::string_view name, uint64_t offset, uint8_t* data, size_t size) {
    uint8_t k[32];
    key(k);
    crypt(k, nonceOf(name).data(), offset, data, size);
    volatile uint8_t* wipe = k;
    for (int i = 0; i < 32; i++) wipe[i] = 0;
}

inline bool isPng(const uint8_t* data, size_t size) {
    static const uint8_t magic[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    return size >= 8 && std::memcmp(data, magic, 8) == 0;
}

inline bool isJson(std::string_view text) {
    size_t a = text.find_first_not_of(" \t\r\n"), b = text.find_last_not_of(" \t\r\n");
    return a != std::string_view::npos && text[a] == '{' && text[b] == '}';
}

// The bytes of a file as they are on disk, opened in memory. A file someone put there unsealed is taken as it is.
inline std::string open(std::string_view name, std::string data) {
    auto* bytes = reinterpret_cast<uint8_t*>(data.data());
    if (isPng(bytes, data.size())) return data;
    std::string opened = data;
    apply(name, 0, reinterpret_cast<uint8_t*>(opened.data()), opened.size());
    if (isPng(reinterpret_cast<const uint8_t*>(opened.data()), opened.size()) || isJson(opened)) return opened;
    return data;
}

}
