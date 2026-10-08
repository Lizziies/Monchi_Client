#include "flarial/Bridge/FontMemory.hpp"

#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<char> data{std::istreambuf_iterator<char>(input), {}};
    if (data.size() < 100) return 2;
    ImGui::CreateContext();
    auto& atlas = *ImGui::GetIO().Fonts;
    ImFontConfig config;
    for (float pixels : {16.f, 32.f})
        if (!addOwnedFont(atlas, data.data(), int(data.size()), pixels, config)) return 3;
    if (atlas.Sources[0].FontData == atlas.Sources[1].FontData) return 4;
    for (const auto& source : atlas.Sources)
        if (!source.FontDataOwnedByAtlas || source.FontData == data.data()) return 5;
    data.clear();
    data.shrink_to_fit();
    unsigned char* pixels;
    int width, height;
    atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0) return 6;
    ImGui::DestroyContext();
    return 0;
}
