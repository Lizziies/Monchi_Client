#pragma once

#include <cmath>

template<class Json>
bool compatibleValue(const Json& current, const Json& value) {
    if (current.is_boolean()) return value.is_boolean();
    if (current.is_string()) return value.is_string();
    if (current.is_number()) return value.is_number() && std::isfinite(value.template get<double>());
    return false;
}
