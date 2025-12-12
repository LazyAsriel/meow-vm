#pragma once

#include "diagnostics/diagnostic.h"

namespace meow {

/// ---------------------------------------------------------------------------
/// 🗺️ SimpleLocaleSource
/// A minimalistic key=value locale provider.
/// Example file format:
/// ```
/// # comment line
/// SYNTAX_ERROR = Unexpected token {token}
/// FILE_NOT_FOUND = Cannot open file: {file}
/// ```
///
/// Usage:
/// ```cpp
/// SimpleLocaleSource loc;
/// loc.load_file("lang/en.lang");
/// auto msg = loc.get_template("FILE_NOT_FOUND");
/// ```
/// ---------------------------------------------------------------------------
struct SimpleLocaleSource final : public LocaleSource {
    ~SimpleLocaleSource() noexcept override = default;
    std::unordered_map<std::string, std::string> map;

    /// Load key=value pairs from a text file.
    /// Lines starting with `#` are ignored.
    /// Whitespace is trimmed around both key and value.
    bool load_file(const std::string& path) {
        std::ifstream in(path);
        if (!in) return false;

        std::string line;
        while (std::getline(in, line)) {
            // Remove trailing spaces
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();

            // Skip leading spaces
            size_t i = 0;
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;

            if (i >= line.size() || line[i] == '#') continue;  // comment or empty line

            // Find '=' separator
            size_t pos = line.find('=', i);
            if (pos == std::string::npos) continue;

            std::string key = line.substr(i, pos - i);
            std::string val = line.substr(pos + 1);

            // Trim spaces on both sides
            auto trim = [](std::string& s) {
                size_t a = 0;
                while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
                size_t b = s.size();
                while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
                s = s.substr(a, b - a);
            };
            trim(key);
            trim(val);

            if (!key.empty()) map[key] = val;
        }

        return true;
    }

    /// Retrieve a localized message template.
    std::optional<std::string> get_template(const std::string& message_id) override {
        if (auto it = map.find(message_id); it != map.end()) return it->second;
        return std::nullopt;
    }
};

}
