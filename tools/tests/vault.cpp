#include "Vault.hpp"

#include <cstdlib>
#include <string>

namespace {
void check(bool ok) { if (!ok) std::abort(); }
}

int main() {
    // RFC 8439, 2.4.2: the keystream starts at block 1, which is byte 64 here
    uint8_t key[32], nonce[12] = {0, 0, 0, 0, 0, 0, 0, 0x4a, 0, 0, 0, 0};
    for (int i = 0; i < 32; i++) key[i] = uint8_t(i);
    std::string text = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    std::string sealed = text;
    vault::crypt(key, nonce, 64, reinterpret_cast<uint8_t*>(sealed.data()), sealed.size());
    const uint8_t head[16] = {0x6e, 0x2e, 0x35, 0x9a, 0x25, 0x68, 0xf9, 0x80, 0x41, 0xba, 0x07, 0x28, 0xdd, 0x0d, 0x69, 0x81};
    check(std::memcmp(sealed.data(), head, 16) == 0);
    check(uint8_t(sealed[sealed.size() - 2]) == 0x87 && uint8_t(sealed.back()) == 0x4d);

    // a part read from the middle opens to the same bytes as the whole
    std::string part = sealed.substr(37, 50);
    vault::crypt(key, nonce, 64 + 37, reinterpret_cast<uint8_t*>(part.data()), part.size());
    check(part == text.substr(37, 50));

    check(vault::named("Line\\Cape/Tex.PNG") == "line/cape/tex.png");
    check(vault::nonceOf("line/cape/tex.png") == vault::nonceOf("LINE\\cape\\tex.png"));
    check(vault::nonceOf("a/tex.png") != vault::nonceOf("b/tex.png"));

    std::string json = "{\"id\": \"cape\"}\n", stored = json;
    vault::apply("cape/item.json", 0, reinterpret_cast<uint8_t*>(stored.data()), stored.size());
    check(stored != json && stored.size() == json.size());
    check(vault::open("cape/item.json", stored) == json);
    check(vault::open("cape/item.json", json) == json);

    std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "pixels", sealedPng = png;
    vault::apply("cape/tex.png", 0, reinterpret_cast<uint8_t*>(sealedPng.data()), sealedPng.size());
    check(!vault::isPng(reinterpret_cast<const uint8_t*>(sealedPng.data()), sealedPng.size()));
    check(vault::open("cape/tex.png", sealedPng) == png);
    check(vault::open("cape/tex.png", png) == png);
    // sealed under another name it does not open
    check(vault::open("other/tex.png", sealedPng) == sealedPng);
}
