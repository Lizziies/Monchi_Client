#include "../../launcher/src/Update.cpp"
#include "FileHash.hpp"

int main(int argc, char** argv) {
    if (argc == 4) {
        auto target = std::filesystem::path(argv[2]);
        bool ok = net::download(argv[1], target, [](float) {}, {}, std::stoull(argv[3]));
        return ok ? 0 : std::filesystem::exists(target) ? 22 : 21;
    }
    std::string digest(64, 'a');
    if (update::expectedHash(digest + "  MonchiLauncher.exe\n", "MonchiLauncher.exe") != digest) return 1;
    if (update::expectedHash(digest + " *MonchiLauncher.exe\n", "MonchiLauncher.exe") != digest) return 2;
    if (!update::expectedHash(digest + "  MonchiLauncher.exe.old\n", "MonchiLauncher.exe").empty()) return 3;
    if (!update::expectedHash("invalid  MonchiLauncher.exe\n", "MonchiLauncher.exe").empty()) return 4;
    if (!update::newer("v0.2.0", "0.1.9") || update::newer("v0.1.0", "0.1.0")) return 5;
    if (update::newer("2oops.0", "1.0") || update::newer("999999999999999999999.0", "1.0") ||
        update::newer("1.0.", "0.0")) return 12;
    if (update::parse(nlohmann::json{{"tag_name", 42}}) ||
        update::parse(nlohmann::json{{"tag_name", "v0.2.0"}, {"assets", false}})) return 13;
    auto release = update::parse(nlohmann::json{{"tag_name", "v0.2.0"}, {"body", nullptr},
        {"assets", nlohmann::json::array({false, {{"name", "Monchi.dll"}, {"browser_download_url", "https://example.com/Monchi.dll"}}})}});
    if (!release || release->dllUrl.empty() || !release->notes.empty()) return 14;
    if (!update::newer("0.2.0", "0.2.0-beta.2") || update::newer("0.2.0-beta.2", "0.2.0") ||
        !update::newer("0.2.0-beta.10", "0.2.0-beta.2") || update::newer("0.2.0+build.2", "0.2.0+build.1")) return 15;
    auto root = std::filesystem::temp_directory_path() / (L"monchi-update-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    auto hashFile = root / L"hash-test";
    files::write(hashFile, "abc");
    if (files::sha256(hashFile) != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") return 16;
    files::write(hashFile, "abd");
    if (files::sha256(hashFile) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") return 17;
    auto dll = root / L"client.dll", core = root / L"core.dll";
    auto nextDll = root / L"client.part", nextCore = root / L"core.part";
    files::write(dll, "old client");
    files::write(core, "old core");
    files::write(nextDll, "new client");
    files::write(nextCore, "new core");
    HANDLE locked = CreateFileW(core.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (locked == INVALID_HANDLE_VALUE) return 6;
    std::string error;
    bool ok = update::replaceFiles({{nextDll, dll}, {nextCore, core}}, error);
    CloseHandle(locked);
    if (ok || files::read(dll) != "old client" || files::read(core) != "old core") return 7;
    files::write(nextDll, "new client");
    if (!update::replaceFiles({{nextDll, dll}, {nextCore, core}}, error)) return 8;
    if (files::read(dll) != "new client" || files::read(core) != "new core") return 9;
    auto fresh = root / L"fresh.dll";
    files::write(nextDll, "fresh");
    if (update::replaceFiles({{nextDll, fresh}, {root / L"missing.part", core}}, error)) return 10;
    if (std::filesystem::exists(fresh) || files::read(core) != "new core") return 11;
    std::filesystem::remove_all(root);
    return 0;
}
