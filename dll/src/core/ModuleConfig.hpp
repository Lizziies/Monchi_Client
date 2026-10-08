#pragma once

#include <json.hpp>
#include <string>

class ModuleConfig {
public:
    void load(const nlohmann::json& modules) {
        values_ = modules.is_object() ? modules : nlohmann::json::object();
    }
    nlohmann::json get(const std::string& name) const {
        auto it = values_.find(name);
        return it == values_.end() ? nlohmann::json::object() : *it;
    }
    nlohmann::json snapshot() const { return values_; }
private:
    nlohmann::json values_ = nlohmann::json::object();
};
