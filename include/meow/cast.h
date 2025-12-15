#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <variant>
#include <memory>
#include <charconv>
#include <system_error>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iostream>

#include <meow/definitions.h>
#include <meow/core/objects.h>
#include <meow/value.h>
#include "meow/bytecode/disassemble.h"

namespace meow {

inline constexpr bool is_space(char c) noexcept {
    return c == ' ' || (c >= '\t' && c <= '\r');
}

inline constexpr char to_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

inline constexpr std::string_view trim_whitespace(std::string_view sv) noexcept {
    while (!sv.empty() && is_space(sv.front())) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && is_space(sv.back())) {
        sv.remove_suffix(1);
    }
    return sv;
}

inline int64_t to_int(param_t value) noexcept {
    using i64_limits = std::numeric_limits<int64_t>;
    
    return value.visit(
        [](null_t) -> int64_t { return 0; },
        [](int_t i) -> int64_t { return i; },
        [](float_t r) -> int64_t {
            if (std::isnan(r)) return 0;
            if (std::isinf(r)) [[unlikely]] return (r > 0) ? i64_limits::max() : i64_limits::min();
            if (r >= static_cast<double>(i64_limits::max())) [[unlikely]] return i64_limits::max();
            if (r <= static_cast<double>(i64_limits::min())) [[unlikely]] return i64_limits::min();
            return static_cast<int64_t>(r);
        },
        [](bool_t b) -> int64_t { return b ? 1 : 0; },
        
        [](object_t obj) -> int64_t {
            if (!obj) return 0;
            
            if (obj->get_type() == ObjectType::STRING) {
                string_t s = reinterpret_cast<string_t>(obj);
                const char* ptr = s->c_str();

                while (is_space(*ptr)) ptr++;
                
                if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
                    char* end;
                    return static_cast<int64_t>(std::strtoll(ptr, &end, 16));
                }

                int64_t res = 0;
                int sign = 1;
                
                if (*ptr == '-') { sign = -1; ptr++; } 
                else if (*ptr == '+') { ptr++; }
                
                bool found_digit = false;
                while (*ptr >= '0' && *ptr <= '9') {
                    res = res * 10 + (*ptr - '0');
                    ptr++;
                    found_digit = true;
                }
                
                if (found_digit) return res * sign;
                return 0;
            }
            return 0;
        },
        [](auto&&) -> int64_t { return 0; }
    );
}

inline double to_float(param_t value) noexcept {
    return value.visit(
        [](null_t) -> double { return 0.0; },
        [](int_t i) -> double { return static_cast<double>(i); },
        [](float_t f) -> double { return f; },
        [](bool_t b) -> double { return b ? 1.0 : 0.0; },
        
        [](object_t obj) -> double {
            if (!obj) return 0.0;

            if (obj->get_type() == ObjectType::STRING) {
                string_t s = reinterpret_cast<string_t>(obj);
                std::string_view sv = s->c_str();
                sv = trim_whitespace(sv);
                if (sv.empty()) return 0.0;
                
                if (sv == "NaN" || sv == "nan") return std::numeric_limits<double>::quiet_NaN();
                if (sv == "Infinity" || sv == "inf") return std::numeric_limits<double>::infinity();
                if (sv == "-Infinity" || sv == "-inf") return -std::numeric_limits<double>::infinity();

                double result = 0.0;
                auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
                
                if (ec == std::errc::result_out_of_range) {
                    return (sv.front() == '-') ? -std::numeric_limits<double>::infinity() 
                                               : std::numeric_limits<double>::infinity();
                }
                if (ec == std::errc::invalid_argument) {
                    char* end;
                    result = std::strtod(s->c_str(), &end);
                }
                return result;
            }
            return 0.0;
        },
        [](auto&&) -> double { return 0.0; }
    );
}

inline bool to_bool(param_t value) noexcept {
    return value.visit(
        [](null_t) -> bool { return false; },
        [](int_t i) -> bool { return i != 0; },
        [](float_t f) -> bool { return f != 0.0 && !std::isnan(f); },
        [](bool_t b) -> bool { return b; },
        
        [](object_t obj) -> bool {
            if (!obj) return false;

            switch(obj->get_type()) {
                case ObjectType::STRING:
                    return !reinterpret_cast<string_t>(obj)->empty();
                case ObjectType::ARRAY:
                    return !reinterpret_cast<array_t>(obj)->empty();
                case ObjectType::HASH_TABLE:
                    return !reinterpret_cast<hash_table_t>(obj)->empty();
                default:
                    return true;
            }
        },
        [](auto&&) -> bool { return true; }
    );
}

inline std::string to_string(param_t value) noexcept;

namespace detail {
inline void object_to_string(object_t obj, std::string& out) noexcept {
    if (obj == nullptr) {
        out += "<null_object_ptr>";
        return;
    }
    
    switch (obj->get_type()) {
        case ObjectType::STRING:
            out += reinterpret_cast<string_t>(obj)->c_str();
            break;

        case ObjectType::ARRAY: {
            array_t arr = reinterpret_cast<array_t>(obj);
            out.push_back('[');
            for (size_t i = 0; i < arr->size(); ++i) {
                if (i > 0) out += ", ";
                out += meow::to_string(arr->get(i));
            }
            out.push_back(']');
            break;
        }

        case ObjectType::HASH_TABLE: {
            hash_table_t hash = reinterpret_cast<hash_table_t>(obj);
            out.push_back('{');
            bool first = true;
            for (const auto& [key, val] : *hash) {
                if (!first) out += ", ";
                std::format_to(std::back_inserter(out), "{}: {}", key->c_str(), meow::to_string(val));
                first = false;
            }
            out.push_back('}');
            break;
        }

        case ObjectType::CLASS: {
            auto name = reinterpret_cast<class_t>(obj)->get_name();
            std::format_to(std::back_inserter(out), "<class '{}'>", (name ? name->c_str() : "??"));
            break;
        }

        case ObjectType::INSTANCE: {
            auto name = reinterpret_cast<instance_t>(obj)->get_class()->get_name();
            std::format_to(std::back_inserter(out), "<{} instance>", (name ? name->c_str() : "??"));
            break;
        }

        case ObjectType::BOUND_METHOD:
            out += "<bound_method>";
            break;

        case ObjectType::MODULE: {
            auto name = reinterpret_cast<module_t>(obj)->get_file_name();
            std::format_to(std::back_inserter(out), "<module '{}'>", (name ? name->c_str() : "??"));
            break;
        }

        case ObjectType::FUNCTION: {
            auto name = reinterpret_cast<function_t>(obj)->get_proto()->get_name();
            std::format_to(std::back_inserter(out), "<function '{}'>", (name ? name->c_str() : "??"));
            break;
        }

        case ObjectType::PROTO: {
            auto proto = reinterpret_cast<proto_t>(obj);
            auto name = proto->get_name();
            std::format_to(std::back_inserter(out), 
                "<proto '{}'>\n  - registers: {}\n  - upvalues:  {}\n  - constants: {}\n{}",
                (name ? name->c_str() : "??"),
                proto->get_num_registers(),
                proto->get_num_upvalues(),
                proto->get_chunk().get_pool_size(),
                disassemble_chunk(proto->get_chunk())
            );
            break;
        }

        case ObjectType::UPVALUE:
            out += "<upvalue>";
            break;

        default:
            std::unreachable();
            break;
    }
}
} // namespace detail

inline std::string to_string(param_t value) noexcept {
    return value.visit(
        [](null_t) -> std::string { return "null"; },
        [](int_t val) -> std::string { return std::to_string(val); },
        [](float_t val) -> std::string {
            if (std::isnan(val)) return "NaN";
            if (std::isinf(val)) return (val > 0) ? "Infinity" : "-Infinity";
            if (val == 0.0 && std::signbit(val)) return "-0.0";
            return std::format("{}", val); 
        },
        [](bool_t val) -> std::string { return val ? "true" : "false"; },
        [](native_t) -> std::string { return "<native_fn>"; },
        [](object_t obj) -> std::string {
            std::string out;
            out.reserve(64); 
            detail::object_to_string(obj, out);
            return out;
        }
    );
}

} // namespace meow