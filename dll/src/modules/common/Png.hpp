#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace png {

inline uint32_t crc(const uint8_t* data, size_t len, uint32_t c = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t v = n;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            table[n] = v;
        }
        ready = true;
    }
    for (size_t i = 0; i < len; i++) c = table[(c ^ data[i]) & 255] ^ (c >> 8);
    return c;
}

inline void put32(std::vector<uint8_t>& out, uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) out.push_back(uint8_t(v >> s));
}

inline void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& body) {
    put32(out, uint32_t(body.size()));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), body.begin(), body.end());
    put32(out, ~crc(out.data() + start, out.size() - start));
}

inline bool write(const std::filesystem::path& path, int w, int h, const std::vector<uint32_t>& rgba) {
    if (w <= 0 || h <= 0 || rgba.size() != size_t(w) * size_t(h)) return false;
    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) {
            uint32_t c = rgba[size_t(y * w + x)];
            raw.push_back(uint8_t(c));
            raw.push_back(uint8_t(c >> 8));
            raw.push_back(uint8_t(c >> 16));
            raw.push_back(uint8_t(c >> 24));
        }
    }
    std::vector<uint8_t> z{0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t pos = 0; pos < raw.size();) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(uint8_t(n));
        z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n));
        z.push_back(uint8_t((~n) >> 8));
        z.insert(z.end(), raw.begin() + long(pos), raw.begin() + long(pos + n));
        pos += n;
    }
    put32(z, (b << 16) | a);

    std::vector<uint8_t> out{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> head;
    put32(head, uint32_t(w));
    put32(head, uint32_t(h));
    head.insert(head.end(), {8, 6, 0, 0, 0});
    chunk(out, "IHDR", head);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
    return bool(file);
}

}
