#include "modules/common/FoldLess.hpp"
#include <cassert>
#include <string>
#include <random>

std::string lower(std::string value) {
    for (auto& c : value) c = char(std::tolower((unsigned char)c));
    return value;
}
int main() {
    std::mt19937 random(64);
    for (int i = 0; i < 20000; i++) {
        std::string a(random() % 80, ' '), b(random() % 80, ' ');
        for (auto& c : a) c = char(random() % 256);
        for (auto& c : b) c = char(random() % 256);
        assert(text::foldLess(a, b) == (lower(a) < lower(b)));
        assert(!text::foldLess(a, a));
    }
    assert(!text::foldLess("Player", "pLaYeR"));
    assert(text::foldLess("abc", "abcd"));
}
