// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/GUI/Engine/ExpressionFormat.hpp. The rift expression library it used is no
// longer published, so {name} variables and the helper calls below are evaluated here; conditionals are not.
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ExpressionFormat {

    namespace detail {

        // ===== Custom rift functions =====

        inline std::string colorRange(double val, double greenMax, double yellowMax) {
            if (val <= greenMax) return "{green}";
            if (val <= yellowMax) return "{yellow}";
            return "{red}";
        }

        inline std::string colorRangeInverse(double val, double redMax, double yellowMax) {
            if (val <= redMax) return "{red}";
            if (val <= yellowMax) return "{yellow}";
            return "{green}";
        }

        inline std::string colorGradient(double val, double minVal, double maxVal) {
            if (maxVal <= minVal) maxVal = minVal + 1.0;
            double t = std::clamp((val - minVal) / (maxVal - minVal), 0.0, 1.0);
            int r = static_cast<int>(t * 255.0);
            int g = static_cast<int>((1.0 - t) * 255.0);
            std::ostringstream oss;
            oss << "{#" << std::hex << std::setfill('0')
                << std::setw(2) << r << std::setw(2) << g << std::setw(2) << 0 << "}";
            return oss.str();
        }

        inline std::string colorGradientInverse(double val, double minVal, double maxVal) {
            if (maxVal <= minVal) maxVal = minVal + 1.0;
            double t = std::clamp((val - minVal) / (maxVal - minVal), 0.0, 1.0);
            int r = static_cast<int>((1.0 - t) * 255.0);
            int g = static_cast<int>(t * 255.0);
            std::ostringstream oss;
            oss << "{#" << std::hex << std::setfill('0')
                << std::setw(2) << r << std::setw(2) << g << std::setw(2) << 0 << "}";
            return oss.str();
        }

        inline std::string rgbFunc(double r, double g, double b) {
            int ri = std::clamp(static_cast<int>(r), 0, 255);
            int gi = std::clamp(static_cast<int>(g), 0, 255);
            int bi = static_cast<int>(std::clamp(b, 0.0, 255.0));
            std::ostringstream oss;
            oss << "{#" << std::hex << std::setfill('0')
                << std::setw(2) << ri << std::setw(2) << gi << std::setw(2) << bi << "}";
            return oss.str();
        }

        inline std::string percentFunc(double val, double max) {
            if (max == 0.0) return "0";
            int pct = static_cast<int>(std::round((val / max) * 100.0));
            return std::to_string(std::clamp(pct, 0, 100));
        }

        inline std::string roundFunc(double val, double decimals) {
            int dec = std::clamp(static_cast<int>(decimals), 0, 6);
            if (dec == 0) return std::to_string(static_cast<int>(std::round(val)));
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(dec) << val;
            return oss.str();
        }

        inline std::string clampFunc(double val, double minVal, double maxVal) {
            double result = std::clamp(val, minVal, maxVal);
            if (result == static_cast<int>(result)) {
                return std::to_string(static_cast<int>(result));
            }
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << result;
            std::string str = oss.str();
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.') str.pop_back();
            return str;
        }


        inline std::string number(double v) {
            if (v == std::floor(v) && std::fabs(v) < 1e15) return std::to_string(static_cast<long long>(v));
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }

        inline double argument(std::string a, double value) {
            a.erase(0, a.find_first_not_of(' '));
            a.erase(a.find_last_not_of(' ') + 1);
            if (a == "value" || a == "v" || a == "val" || a == "VALUE" || a == "Value") return value;
            return std::strtod(a.c_str(), nullptr);
        }

        inline std::string call(const std::string& name, const std::vector<double>& x) {
            auto need = [&](size_t n) { return x.size() >= n; };
            if (name == "colorRange" && need(3)) return colorRange(x[0], x[1], x[2]);
            if (name == "colorRangeInverse" && need(3)) return colorRangeInverse(x[0], x[1], x[2]);
            if (name == "colorGradient" && need(3)) return colorGradient(x[0], x[1], x[2]);
            if (name == "colorGradientInverse" && need(3)) return colorGradientInverse(x[0], x[1], x[2]);
            if (name == "rgb" && need(3)) return rgbFunc(x[0], x[1], x[2]);
            if (name == "percent" && need(2)) return percentFunc(x[0], x[1]);
            if (name == "round" && need(2)) return roundFunc(x[0], x[1]);
            if (name == "clamp" && need(3)) return clampFunc(x[0], x[1], x[2]);
            return {};
        }

        inline bool expand(const std::string& body, double value, const std::string& stringValue, std::string& out) {
            static const char* colors[] = {"red", "green", "blue", "yellow", "orange", "purple", "pink", "cyan", "white", "black", "gray", "reset"};
            if (body == "value" || body == "v" || body == "val" || body == "VALUE" || body == "Value") {
                out = stringValue.empty() ? number(value) : stringValue;
                return true;
            }
            for (auto* c : colors)
                if (body == c) {
                    out = "{" + body + "}";
                    return true;
                }
            auto open = body.find('(');
            if (open == std::string::npos || body.back() != ')') return false;
            std::vector<double> args;
            std::string inner = body.substr(open + 1, body.size() - open - 2), part;
            std::stringstream ss(inner);
            while (std::getline(ss, part, ',')) args.push_back(argument(part, value));
            out = call(body.substr(0, open), args);
            return !out.empty();
        }
    }

    inline void initialize() {}

    inline std::string format(const std::string& formatString, double value, const std::string& stringValue = "") {
        if (formatString.find('{') == std::string::npos) return formatString;
        std::string out;
        for (size_t i = 0; i < formatString.size();) {
            size_t close = formatString[i] == '{' ? formatString.find('}', i) : std::string::npos;
            std::string piece;
            if (close != std::string::npos && detail::expand(formatString.substr(i + 1, close - i - 1), value, stringValue, piece)) {
                out += piece;
                i = close + 1;
            } else {
                out += formatString[i++];
            }
        }
        return out;
    }

    inline bool hasExpressions(const std::string& text) { return text.find('{') != std::string::npos; }

}
