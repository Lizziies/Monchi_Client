#pragma once

#include <format>
#include <string>
#include <string_view>

namespace logger {

void open();
void close();
void write(std::string_view level, std::string_view msg);

template <class... A>
void info(std::format_string<A...> fmt, A&&... args) {
    write("info", std::format(fmt, std::forward<A>(args)...));
}

template <class... A>
void warn(std::format_string<A...> fmt, A&&... args) {
    write("warn", std::format(fmt, std::forward<A>(args)...));
}

template <class... A>
void error(std::format_string<A...> fmt, A&&... args) {
    write("error", std::format(fmt, std::forward<A>(args)...));
}

std::string narrow(std::wstring_view w);
std::wstring widen(std::string_view s);

}
