#include "Settings.hpp"
#include "Files.hpp"
#include <windows.h>

int main() {
    auto root = std::filesystem::temp_directory_path() / (L"monchi-settings-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    auto path = root / L"launcher.json";
    Settings original;
    original.autoInject = false;
    original.closeAfterInject = true;
    original.pinned = "C:/Games/Minecraft.Windows.exe";
    original.folders = {"C:/Games", "D:/Minecraft"};
    original.accent = 3;
    if (!original.save(path)) return 1;
    auto read = Settings::load(path);
    if (read.autoInject || !read.closeAfterInject || read.pinned != original.pinned ||
        read.folders != original.folders || read.accent != 3) return 2;
    HANDLE lock = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (lock == INVALID_HANDLE_VALUE) return 3;
    Settings changed;
    bool written = changed.save(path);
    CloseHandle(lock);
    if (written || Settings::load(path).pinned != original.pinned) return 4;
    files::write(path, R"({"beta":"wrong","autoInject":false,"pinned":null,"accent":999,"folders":[42,"C:/valid"]})");
    read = Settings::load(path);
    if (read.beta || read.autoInject || !read.pinned.empty() || read.accent != -1 || read.folders.size() != 1) return 5;
    files::write(path, "broken json");
    if (!Settings::load(path).autoInject) return 6;
    std::filesystem::remove_all(root);
}
