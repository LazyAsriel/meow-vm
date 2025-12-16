#pragma once

#include "diagnostics/diagnostic.h"

namespace meow {

struct SimpleLocaleSource final : public LocaleSource {
    ~SimpleLocaleSource() noexcept override = default;
    std::unordered_map<std::string, std::string> map;

    bool load_file(const std::string& path) {
        std::ifstream in(path);
        if (!in) return false;

        std::string line;
        while (std::getline(in, line)) {
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();

            size_t i = 0;
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;

            if (i >= line.size() || line[i] == '#') continue;
            size_t pos = line.find('=', i);
            if (pos == std::string::npos) continue;

            std::string key = line.substr(i, pos - i);
            std::string val = line.substr(pos + 1);

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

    std::optional<std::string> get_template(const std::string& message_id) override {
        if (auto it = map.find(message_id); it != map.end()) return it->second;
        return std::nullopt;
    }
};

}
