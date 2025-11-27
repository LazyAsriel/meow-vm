#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/objects.h"
#include "core/value.h"
#include "bytecode/disassemble.h"

namespace meow {

constexpr std::string_view trim_whitespace(std::string_view sv) noexcept {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
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
            if (std::isinf(r)) return (r > 0) ? i64_limits::max() : i64_limits::min();
            if (r >= static_cast<double>(i64_limits::max())) return i64_limits::max();
            if (r <= static_cast<double>(i64_limits::min())) return i64_limits::min();
            return static_cast<int64_t>(r);
        },
        [](bool_t b) -> int64_t { return b ? 1 : 0; },
        [](string_t s) -> int64_t {
            std::string_view sv = s->c_str();
            sv = trim_whitespace(sv);
            if (sv.empty()) return 0;

            int base = 10;
            
            bool negative = false;
            if (sv.front() == '-') {
                negative = true;
                sv.remove_prefix(1);
            } else if (sv.front() == '+') {
                sv.remove_prefix(1);
            }

            if (sv.size() >= 2 && sv.front() == '0') {
                char indicator = std::tolower(static_cast<unsigned char>(sv[1]));
                if (indicator == 'x') { base = 16; sv.remove_prefix(2); }
                else if (indicator == 'b') { base = 2; sv.remove_prefix(2); }
                else if (indicator == 'o') { base = 8; sv.remove_prefix(2); }
            }

            if (sv.empty()) return 0;

            int64_t result = 0;
            auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result, base);

            if (ec == std::errc::result_out_of_range) {
                return negative ? i64_limits::min() : i64_limits::max();
            }
            
            return negative ? -result : result;
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
        [](string_t s) -> double {
            std::string_view sv = s->c_str();
            sv = trim_whitespace(sv);
            if (sv.empty()) return 0.0;
            auto is_case_insensitive = [](std::string_view a, std::string_view b) {
                return std::ranges::equal(a, b, [](char c1, char c2) {
                    return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
                });
            };

            if (is_case_insensitive(sv, "nan")) return std::numeric_limits<double>::quiet_NaN();
            
            bool negative = (sv.front() == '-');
            std::string_view temp_sv = sv;
            if (negative || sv.front() == '+') temp_sv.remove_prefix(1);

            if (is_case_insensitive(temp_sv, "infinity") || is_case_insensitive(temp_sv, "inf")) {
                return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
            }

            double result = 0.0;
            auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result, std::chars_format::general);

            if (ec == std::errc::result_out_of_range) {
                return (sv.front() == '-') ? -std::numeric_limits<double>::infinity() 
                                           : std::numeric_limits<double>::infinity();
            }
            
            return result;
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
        [](string_t s) -> bool { return !s->empty(); },
        [](array_t a) -> bool { return !a->empty(); },
        [](hash_table_t o) -> bool { return !o->empty(); },
        [](auto&&) -> bool { return true; }
    );
}

inline std::string to_string(param_t value) noexcept;

namespace detail {
inline void object_to_string_impl(object_t obj, std::string& out) noexcept {
    if (obj == nullptr) {
        out += "<null_object_ptr>";
        return;
    }
    
    switch (obj->get_type()) {
        case ObjectType::STRING:
            out.push_back('"');
            out += reinterpret_cast<string_t>(obj)->c_str();
            out.push_back('"');
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
                std::format_to(std::back_inserter(out), "\"{}\": {}", key->c_str(), meow::to_string(val));
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
            out += "<unknown_object_type>";
            break;
    }
}
}

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
            detail::object_to_string_impl(obj, out);
            return out;
        }
    );
}

}