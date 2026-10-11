#include "../../dll/src/modules/common/ItemIcons.cpp"
#include <cassert>
#include <chrono>
#include <iostream>

namespace bg { void run(std::function<void()> work) { work(); } }

int main(int argc, char** argv) {
    itemicon::Bytes tga(18 + 16, 0);
    tga[2] = 2; tga[12] = 2; tga[14] = 2; tga[16] = 32; tga[17] = 0x20;
    for (int i = 0; i < 4; ++i) {
        tga[18 + i * 4] = uint8_t(i + 1);
        tga[21 + i * 4] = 255;
    }
    img::Pixels pixels;
    assert(itemicon::tga(tga, pixels));
    assert(pixels.rgba.front() == IM_COL32(0,0,1,255));
    tga[17] = 0;
    assert(itemicon::tga(tga, pixels));
    assert(pixels.rgba.front() == IM_COL32(0,0,3,255));
    auto original = tga;
    tga.resize(20);
    assert(!itemicon::tga(tga, pixels));
    tga.resize(23); tga[2] = 10; tga[18] = 0x84;
    assert(!itemicon::tga(tga, pixels));
    itemicon::Bytes data(16 + 256 + original.size(), 0);
    auto put = [&](size_t at, uint32_t value) { std::memcpy(data.data() + at, &value, 4); };
    put(0, 0xb125277d); put(4, 0x267052a0); put(8, 1); put(12, 1);
    data[16] = 8;
    std::memcpy(data.data() + 17, "test.tga", 8);
    put(16 + 252, uint32_t(original.size()));
    std::copy(original.begin(), original.end(), data.begin() + 272);
    auto path = std::filesystem::temp_directory_path() / "monchi-item-icons-test.brarchive";
    auto write = [&] { std::ofstream f(path, std::ios::binary); f.write(reinterpret_cast<char*>(data.data()), data.size()); };
    write(); itemicon::archive(path); itemicon::loaded = true;
    assert(itemicon::decode("minecraft:test").rgba.front() == IM_COL32(0,0,3,255));
    itemicon::files.clear(); put(16 + 248, UINT32_MAX); write(); itemicon::archive(path);
    assert(itemicon::files.empty());
    put(8, UINT32_MAX); write(); itemicon::archive(path);
    assert(itemicon::files.empty());
    std::filesystem::remove(path);
    if (argc > 1) {
        auto start = std::chrono::steady_clock::now();
        itemicon::archive(argv[1]);
        int decoded = 0, failed = 0;
        for (const auto& [name, bytes] : itemicon::files) {
            img::Pixels image;
            bool ok = name.ends_with(".tga") ? itemicon::tga(bytes, image) : img::load(bytes, 64, image);
            ok ? ++decoded : ++failed;
        }
        std::cout << decoded << " decoded, " << failed << " failed, "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms\n";
        assert(decoded > 0 && failed == 0);
    }
}
