#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

struct NameHash {
    using is_transparent = void;
    static unsigned char fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
    size_t operator()(std::string_view name) const {
        size_t hash = 14695981039346656037ull;
        for (unsigned char c : name) { hash ^= fold(c); hash *= 1099511628211ull; }
        return hash;
    }
};

struct NameEqual {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (NameHash::fold(a[i]) != NameHash::fold(b[i])) return false;
        return true;
    }
};

template<class User>
class UserCache {
public:
    using Map = std::unordered_map<std::string, User, NameHash, NameEqual>;
    template<class Source> void replace(const Source& source) {
        auto next = std::make_shared<Map>();
        next->reserve(source.size());
        for (const auto& [key, user] : source) next->emplace(key, user);
        users_.store(std::move(next));
    }
    std::shared_ptr<const Map> snapshot() const { return users_.load(); }
    bool find(std::string_view name, User& out) const {
        auto users = snapshot();
        auto it = users->find(name);
        if (it == users->end()) return false;
        out = it->second;
        return true;
    }
private:
    std::atomic<std::shared_ptr<const Map>> users_{std::make_shared<const Map>()};
};
