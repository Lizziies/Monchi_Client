#pragma once

#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace capture {

enum class Stage { Game, Overlay, Final };

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgra;
};

bool request(Stage stage);
bool busy();
bool poll(Image& out);
void submit(ImDrawList* dl, Stage stage);
void grabFinal(::ID3D11Device* device, ::ID3D11DeviceContext* context);
void shutdown();

enum class Format { Png, Jpeg };

void save(Image image, std::filesystem::path path, Format format, int quality);
bool takeSaved(std::filesystem::path& path, bool& ok);
bool copyToClipboard(const Image& image, void* window);

}
