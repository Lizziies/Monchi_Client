#include "Update.hpp"
#include "Build.hpp"
#include "Files.hpp"
#include "Net.hpp"
#include "ReleaseSignature.hpp"

#include <json.hpp>

#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <sstream>
#include <vector>

using nlohmann::json;

namespace update {

namespace {

std::string apiBase() {
    return "https://api.github.com/repos/" + files::narrow(build::repoOwner) + "/" + files::narrow(build::repoName);
}

std::vector<int> parts(std::string v) {
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(0, 1);
    auto suffix = v.find_first_of("-+");
    if (suffix != std::string::npos) {
        auto text = v.substr(suffix + 1);
        if (text.empty() || text.front() == '.' || text.back() == '.' || text.find("..") != std::string::npos ||
            text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-+") != std::string::npos) return {};
        v.erase(suffix);
    }
    std::vector<int> out;
    std::stringstream ss(v);
    std::string item;
    while (std::getline(ss, item, '.')) {
        int value = 0;
        auto parsed = std::from_chars(item.data(), item.data() + item.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != item.data() + item.size() || value < 0) return {};
        out.push_back(value);
    }
    if (v.empty() || v.back() == '.') return {};
    return out;
}

std::optional<Release> parse(const json& j) {
    if (!j.is_object() || !j.contains("tag_name") || !j["tag_name"].is_string()) return std::nullopt;
    Release r;
    r.tag = j.value("tag_name", "");
    if (parts(r.tag).empty()) return std::nullopt;
    if (j.contains("body") && j["body"].is_string()) r.notes = j["body"].get<std::string>();
    if (!j.contains("assets") || !j["assets"].is_array()) return std::nullopt;
    for (auto& a : j["assets"]) {
        if (!a.is_object() || !a.contains("name") || !a["name"].is_string() ||
            !a.contains("browser_download_url") || !a["browser_download_url"].is_string()) continue;
        std::string name = a["name"].get<std::string>();
        std::string url = a["browser_download_url"].get<std::string>();
        if (!url.starts_with("https://")) continue;
        if (name == "Monchi.dll") r.dllUrl = url;
        else if (name == "MonchiFlarial.dll") r.coreUrl = url;
        else if (name == "MonchiLauncher.exe") r.launcherUrl = url;
        else if (name == "checksums.txt") r.sumsUrl = url;
        else if (name == "checksums.sig") r.signatureUrl = url;
    }
    return r;
}

std::string sha256(const std::filesystem::path& p) {
    std::string data = files::read(p);
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    unsigned char digest[32] = {};
    auto status = BCryptHash(alg, nullptr, 0, (PUCHAR)data.data(), (ULONG)data.size(), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) return {};
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (unsigned char b : digest) {
        out += hex[b >> 4];
        out += hex[b & 15];
    }
    return out;
}

std::string expectedHash(const std::string& sums, const std::string& file) {
    std::stringstream ss(sums);
    std::string line;
    while (std::getline(ss, line)) {
        std::istringstream entry(line);
        std::string hash, name;
        if (!(entry >> hash >> name)) continue;
        if (!name.empty() && name[0] == '*') name.erase(0, 1);
        if (name != file || hash.size() != 64 || hash.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) continue;
        std::transform(hash.begin(), hash.end(), hash.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return hash;
    }
    return {};
}

bool fetch(const std::string& url, const std::string& name, const std::string& sums,
           const std::filesystem::path& to, const std::function<void(float)>& progress, std::string& error) {
    if (url.empty()) {
        error = "The release is incomplete";
        return false;
    }
    if (!net::download(url, to, progress, {}, 128 * 1024 * 1024)) {
        error = "Download failed";
        return false;
    }
    std::string want = expectedHash(sums, name);
    if (want.empty() || want != sha256(to)) {
        std::error_code ec;
        std::filesystem::remove(to, ec);
        error = "Checksum does not match";
        return false;
    }
    return true;
}

}

bool replaceFiles(const std::vector<std::pair<std::filesystem::path, std::filesystem::path>>& files,
                  std::string& error) {
    struct Saved { std::filesystem::path backup; bool existed; };
    std::vector<Saved> saved;
    auto cleanup = [&] {
        for (auto& item : saved) {
            std::error_code ec;
            std::filesystem::remove(item.backup, ec);
        }
    };
    for (auto& [source, target] : files) {
        auto backup = target;
        backup += L".update-" + std::to_wstring(GetCurrentProcessId()) + L".bak";
        std::error_code ec;
        bool exists = std::filesystem::exists(target, ec);
        if (ec || (exists && !CopyFileW(target.c_str(), backup.c_str(), FALSE))) {
            cleanup();
            error = "Could not back up the client update";
            return false;
        }
        saved.push_back({backup, exists});
    }
    for (size_t i = 0; i < files.size(); i++) {
        auto& [source, target] = files[i];
        if (MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) continue;
        bool restored = true;
        for (size_t j = i; j-- > 0;) {
            auto& previous = files[j].second;
            if (saved[j].existed)
                restored = MoveFileExW(saved[j].backup.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) && restored;
            else
                restored = DeleteFileW(previous.c_str()) && restored;
        }
        if (restored) cleanup();
        error = restored ? "Close Minecraft to update the client" : "Could not restore the previous client update";
        return false;
    }
    cleanup();
    return true;
}

std::optional<Release> latest(bool beta, int timeoutMs) {
    auto body = net::get(apiBase() + (beta ? "/releases?per_page=5" : "/releases/latest"), timeoutMs);
    if (!body) return std::nullopt;
    json j = json::parse(*body, nullptr, false);
    if (j.is_discarded()) return std::nullopt;
    if (j.is_array()) return j.empty() ? std::nullopt : parse(j[0]);
    return parse(j);
}

bool newer(const std::string& candidate, const std::string& current) {
    auto a = parts(candidate), b = parts(current);
    if (a.empty()) return false;
    if (b.empty()) b = {0};
    size_t n = std::max(a.size(), b.size());
    a.resize(n);
    b.resize(n);
    if (a != b) return std::lexicographical_compare(b.begin(), b.end(), a.begin(), a.end());
    auto prerelease = [](const std::string& tag) {
        auto end = tag.find('+');
        auto dash = tag.find('-');
        return dash < end ? tag.substr(dash + 1, end - dash - 1) : std::string();
    };
    auto first = prerelease(candidate), second = prerelease(current);
    if (first.empty() || second.empty()) return first.empty() && !second.empty();
    std::stringstream left(first), right(second);
    std::string x, y;
    while (std::getline(left, x, '.')) {
        if (!std::getline(right, y, '.')) return true;
        if (x == y) continue;
        bool xn = x.find_first_not_of("0123456789") == std::string::npos;
        bool yn = y.find_first_not_of("0123456789") == std::string::npos;
        if (xn != yn) return !xn;
        if (xn && x.size() != y.size()) return x.size() > y.size();
        return x > y;
    }
    return false;
}

std::string installedTag() {
    std::string t = files::read(files::installedTag());
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ')) t.pop_back();
    return t;
}

bool installDll(const Release& r, const std::function<void(float)>& progress, std::string& error) {
    std::string sums = r.sumsUrl.empty() ? std::string() : net::get(r.sumsUrl).value_or("");
    auto signature = r.signatureUrl.empty() ? std::string() : net::get(r.signatureUrl).value_or("");
    if (!verifySignature(sums, signature, r.tag)) { error = "Release signature is missing or invalid"; return false; }
    auto tmp = files::bin() / L"Monchi.dll.part";
    if (!fetch(r.dllUrl, "Monchi.dll", sums, tmp, progress, error)) return false;
    auto coreTmp = files::bin() / L"MonchiFlarial.dll.part";
    if (!r.coreUrl.empty() && !fetch(r.coreUrl, "MonchiFlarial.dll", sums, coreTmp, progress, error)) return false;
    auto tagTmp = files::bin() / L"version.txt.part";
    if (!files::write(tagTmp, r.tag)) {
        error = "Could not save the client version";
        return false;
    }
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> staged{{tmp, files::dll()}};
    if (!r.coreUrl.empty()) staged.emplace_back(coreTmp, files::core());
    staged.emplace_back(tagTmp, files::installedTag());
    bool ok = replaceFiles(staged, error);
    for (auto& [source, target] : staged) {
        std::error_code ec;
        std::filesystem::remove(source, ec);
    }
    return ok;
}

bool swapLauncher(const Release& r, const std::function<void(float)>& progress, std::string& error) {
    std::string sums = r.sumsUrl.empty() ? std::string() : net::get(r.sumsUrl).value_or("");
    auto signature = r.signatureUrl.empty() ? std::string() : net::get(r.signatureUrl).value_or("");
    if (!verifySignature(sums, signature, r.tag)) { error = "Release signature is missing or invalid"; return false; }
    auto next = files::root() / L"MonchiLauncher.new.exe";
    if (!fetch(r.launcherUrl, "MonchiLauncher.exe", sums, next, progress, error)) return false;

    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring old = std::wstring(self) + L".old";
    DeleteFileW(old.c_str());
    if (!MoveFileExW(self, old.c_str(), 0) || !MoveFileExW(next.c_str(), self, 0)) {
        MoveFileExW(old.c_str(), self, MOVEFILE_REPLACE_EXISTING);
        error = "Could not replace the launcher";
        return false;
    }
    std::wstring command = L"\"" + std::wstring(self) + L"\" --wait-for " + std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(self, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        DeleteFileW(self);
        MoveFileExW(old.c_str(), self, MOVEFILE_REPLACE_EXISTING);
        error = "Could not restart the updated launcher";
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

void cleanup() {
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    DeleteFileW((std::wstring(self) + L".old").c_str());
}

}
