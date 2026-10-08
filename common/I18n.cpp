#include "I18n.hpp"
#include "DataDir.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace i18n {

namespace {

std::unordered_map<std::string_view, const char*>& german() {
    static std::unordered_map<std::string_view, const char*> map;
    return map;
}

Lang pick = Lang::Auto;

std::filesystem::path file() {
#ifdef _WIN32
    return dataDir() / L"lang.txt";
#else
    return std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".monchi-lang";
#endif
}
}

void add(const Entry* entries, size_t count) {
    for (size_t i = 0; i < count; i++) german()[entries[i].en] = entries[i].de;
}

Lang system() {
    static const Lang lang = [] {
#ifdef _WIN32
        return (GetUserDefaultUILanguage() & 0x3FF) == LANG_GERMAN ? Lang::German : Lang::English;
#else
        const char* env = std::getenv("MONCHI_LANG");
        return env && parse(env) == Lang::German ? Lang::German : Lang::English;
#endif
    }();
    return lang;
}

Lang chosen() { return pick; }

Lang active() { return pick == Lang::Auto ? system() : pick; }

void choose(Lang lang) {
    pick = lang;
    save();
}

const char* code(Lang lang) {
    switch (lang) {
    case Lang::English: return "en";
    case Lang::German: return "de";
    default: return "auto";
    }
}

Lang parse(const std::string& s) {
    if (s == "en") return Lang::English;
    if (s == "de") return Lang::German;
    return Lang::Auto;
}

void load() {
    std::ifstream in(file());
    std::string s;
    in >> s;
    pick = parse(s);
}

void save() {
    std::error_code ec;
    std::filesystem::create_directories(file().parent_path(), ec);
    std::ofstream(file(), std::ios::trunc) << code(pick);
}

bool known(const char* en) { return german().count(en) > 0; }

const char* tr(const char* en) {
    if (active() != Lang::German) return en;
    auto& map = german();
    auto it = map.find(en);
    return it == map.end() ? en : it->second;
}

}
