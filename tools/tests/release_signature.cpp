#include "ReleaseSignature.hpp"
#include <cassert>
#include <string>
#include <fstream>
#include <filesystem>

int main(int argc, char** argv) {
    if (argc == 4) {
        auto read = [](const char* path) {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), {});
        };
        return update::verifySignature(read(argv[1]), read(argv[2]), argv[3]) ? 0 : 1;
    }
    std::string manifest = "version v0.0.0\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  MonchiLauncher.exe\n";
    unsigned char bytes[]{0x6b,0xd3,0x74,0xa6,0x34,0x50,0x72,0x2c,0xf2,0x4e,0x5c,0x4e,0xf2,0x9e,0x88,0x3d,0xd6,0xc4,0xc3,0x17,0x63,0xfb,0x56,0xd7,0x27,0x81,0x5e,0xf5,0xac,0x88,0xe3,0x6d,0xb5,0x61,0x38,0x4c,0xd9,0x52,0x50,0xdc,0xf8,0xa8,0xaa,0xec,0xda,0xc6,0x5b,0x10,0xaa,0x89,0xf6,0x11,0xa8,0x8e,0xdd,0x9,0x9d,0xc6,0xbd,0x78,0xa5,0xe5,0x62,0x9d};
    std::string signature(reinterpret_cast<char*>(bytes), sizeof(bytes));
    assert(update::verifySignature(manifest, signature, "v0.0.0"));
    assert(!update::verifySignature(manifest, signature, "v0.1.1"));
    manifest.back() = 'x';
    assert(!update::verifySignature(manifest, signature, "v0.0.0"));
    manifest.back() = '\n';
    signature[0] ^= 1;
    assert(!update::verifySignature(manifest, signature, "v0.0.0"));
    signature.pop_back();
    assert(!update::verifySignature(manifest, signature, "v0.0.0"));
    assert(!update::verifySignature(manifest, "", "v0.0.0"));
}
