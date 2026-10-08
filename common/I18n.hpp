#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <utility>

namespace i18n {

enum class Lang { Auto, English, German };

struct Entry {
    const char* en;
    const char* de;
};

void add(const Entry* entries, size_t count);

template <size_t N>
struct Table {
    explicit Table(const Entry (&entries)[N]) { add(entries, N); }
};

Lang system();
Lang chosen();
Lang active();
void choose(Lang lang);

const char* code(Lang lang);
Lang parse(const std::string& code);

void load();
void save();

const char* tr(const char* en);
bool known(const char* en);

template <class... Args>
std::string fmt(const char* en, Args&&... args) {
    return std::vformat(tr(en), std::make_format_args(args...));
}

}
