#include "hook/Hook.hpp"
#include "hook/Sealed.hpp"
#include "Vault.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace logger {
void write(std::string_view, std::string_view) {}
}
namespace paths {
fs::path dataRoot;
const fs::path& root() { return dataRoot; }
}
namespace cosmetics {
thread_local int reading = 0;
}

namespace {

void check(bool ok, int line) {
    if (ok) return;
    std::printf("failed at line %d\n", line);
    std::exit(1);
}
#define CHECK(x) check(x, __LINE__)

std::string viaStdio(const std::filesystem::path& file) {
    FILE* f = _wfopen(file.c_str(), L"rb");
    CHECK(f != nullptr);
    std::string out;
    char buf[777];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) out.append(buf, n);
    std::fclose(f);
    return out;
}

std::string viaHandle(const std::filesystem::path& file, bool ansi) {
    HANDLE h = ansi ? CreateFileA(file.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr)
                    : CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    CHECK(h != INVALID_HANDLE_VALUE);
    std::string out;
    char buf[1000];
    DWORD n = 0;
    while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n) out.append(buf, n);
    CloseHandle(h);
    return out;
}

}

int main() {
    auto base = std::filesystem::temp_directory_path() / L"monchi-sealed-test";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    paths::dataRoot = base / L"Monchi";
    auto dir = paths::dataRoot / L"cosmetics" / L"Cape";
    std::filesystem::create_directories(dir);

    std::string png("\x89PNG\r\n\x1a\n", 8);
    for (int i = 0; i < 20000; i++) png += char(i * 7 + i / 251);
    std::string stored = png;
    vault::apply("cape/tex.png", 0, reinterpret_cast<uint8_t*>(stored.data()), stored.size());
    auto sealedFile = dir / L"tex.png", openFile = dir / L"open.png", other = paths::dataRoot / L"notes.png";
    std::ofstream(sealedFile, std::ios::binary).write(stored.data(), std::streamsize(stored.size()));
    std::ofstream(openFile, std::ios::binary).write(png.data(), std::streamsize(png.size()));
    std::ofstream(other, std::ios::binary).write(stored.data(), std::streamsize(stored.size()));

    CHECK(viaStdio(sealedFile) == stored);
    CHECK(hook::init());
    sealed::install();
    CHECK(hook::enableAll());

    // the ways a program reads a file: buffered, by handle, by handle with an ansi name, in a stream, at an offset
    CHECK(viaStdio(sealedFile) == png);
    CHECK(viaHandle(sealedFile, false) == png);
    CHECK(viaHandle(sealedFile, true) == png);
    {
        std::ifstream in(sealedFile, std::ios::binary);
        std::string all{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        CHECK(all == png);
    }
    {
        FILE* f = _wfopen(sealedFile.c_str(), L"rb");
        char part[300];
        std::fseek(f, 12345, SEEK_SET);
        CHECK(std::fread(part, 1, sizeof(part), f) == sizeof(part));
        CHECK(std::string(part, sizeof(part)) == png.substr(12345, sizeof(part)));
        std::fclose(f);
    }
    {
        HANDLE h = CreateFileW(sealedFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        char part[500];
        OVERLAPPED at{};
        at.Offset = 4097;
        DWORD n = 0;
        CHECK(ReadFile(h, part, sizeof(part), &n, &at) && n == sizeof(part));
        CHECK(std::string(part, n) == png.substr(4097, n));
        CloseHandle(h);
    }
    // forward slashes and another spelling of the folder
    {
        std::wstring spelled = sealedFile.wstring();
        for (auto& c : spelled) c = c == L'\\' ? L'/' : wchar_t(towupper(c));
        CHECK(viaStdio(spelled) == png);
    }

    // a picture that is not sealed, a file elsewhere and the client's own reads come through untouched
    CHECK(viaStdio(openFile) == png);
    CHECK(viaHandle(openFile, false) == png);
    CHECK(viaStdio(other) == stored);
    cosmetics::reading++;
    CHECK(viaStdio(sealedFile) == stored);
    cosmetics::reading--;

    // handle numbers come back for other files after a close; those reads must stay as they are
    for (int i = 0; i < 64; i++) {
        CHECK(viaHandle(sealedFile, false) == png);
        CHECK(viaHandle(other, false) == stored);
        CHECK(viaStdio(other) == stored);
    }

    hook::disableAll();
    std::filesystem::remove_all(base, ec);
    std::printf("sealed reads ok\n");
}
