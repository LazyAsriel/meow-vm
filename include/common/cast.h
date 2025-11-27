#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/objects.h"
#include "core/value.h"
#include "bytecode/disassemble.h"

namespace meow {

inline int64_t to_int(param_t value) noexcept {
    using i64_limits = std::numeric_limits<int64_t>;
    return value.visit(
        [](null_t) -> int64_t { return 0; },
        [](int_t i) -> int64_t { return i; },
        [](float_t r) -> int64_t {
            if (std::isinf(r)) return (r > 0) ? i64_limits::max() : i64_limits::min();
            if (std::isnan(r)) return 0;
            return static_cast<int64_t>(r);
        },
        [](bool_t b) -> int64_t { return b ? 1 : 0; },
        [](string_t s) -> int64_t {
            std::string_view sv = s->c_str();

            size_t left = 0;
            while (left < sv.size() && std::isspace(static_cast<unsigned char>(sv[left]))) ++left;
            size_t right = sv.size();
            while (right > left && std::isspace(static_cast<unsigned char>(sv[right - 1]))) --right;
            if (left >= right) return 0;
            sv = sv.substr(left, right - left);

            bool negative = false;
            if (!sv.empty() && sv[0] == '-') {
                negative = true;
                sv.remove_prefix(1);
            } else if (!sv.empty() && sv[0] == '+') {
                sv.remove_prefix(1);
            }

            if (sv.size() >= 2) {
                auto prefix = sv.substr(0, 2);
                if (prefix == "0b" || prefix == "0B") {
                    using ull = unsigned long long;
                    ull acc = 0;
                    const ull limit = static_cast<ull>(i64_limits::max());
                    sv.remove_prefix(2);
                    for (char c : sv) {
                        if (c == '0' || c == '1') {
                            int d = c - '0';
                            if (acc > (limit - d) / 2) {
                                return negative ? i64_limits::min() : i64_limits::max();
                            }
                            acc = (acc << 1) | static_cast<ull>(d);
                        } else {
                            break;
                        }
                    }
                    int64_t result = static_cast<int64_t>(acc);
                    return negative ? -result : result;
                }
            }

            int base = 10;
            std::string token(sv.begin(), sv.end());
            if (token.size() >= 2) {
                std::string p = token.substr(0, 2);
                if (p == "0x" || p == "0X") {
                    base = 16;
                    token = token.substr(2);
                } else if (p == "0o" || p == "0O") {
                    base = 8;
                    token = token.substr(2);
                }
            }

            if (negative) token.insert(token.begin(), '-');

            errno = 0;
            char* endptr = nullptr;
            long long parsed = std::strtoll(token.c_str(), &endptr, base);
            if (endptr == token.c_str()) return 0;
            if (errno == ERANGE) return (parsed > 0) ? i64_limits::max() : i64_limits::min();
            if (parsed > i64_limits::max()) return i64_limits::max();
            if (parsed < i64_limits::min()) return i64_limits::min();
            return static_cast<int64_t>(parsed);
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
            std::string str = s->c_str();
            for (auto& c : str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (str == "nan") return std::numeric_limits<double>::quiet_NaN();
            if (str == "infinity" || str == "+infinity" || str == "inf" || str == "+inf")
                return std::numeric_limits<double>::infinity();
            if (str == "-infinity" || str == "-inf")
                return -std::numeric_limits<double>::infinity();
            const char* cs = str.c_str();
            errno = 0;
            char* endptr = nullptr;
            double val = std::strtod(cs, &endptr);
            if (cs == endptr) return 0.0;
            if (errno == ERANGE) return (val > 0) ? std::numeric_limits<double>::infinity()
                                                 : -std::numeric_limits<double>::infinity();
            return val;
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
inline std::string object_to_string(object_t obj) noexcept {
    if (obj == nullptr) {
        return "<null_object_ptr>";
    }
    
    switch (obj->get_type()) {
        case ObjectType::STRING:
            return "\"" + std::string(reinterpret_cast<string_t>(obj)->c_str()) + "\"";

        case ObjectType::ARRAY: {
            array_t arr = reinterpret_cast<array_t>(obj);
            std::string out = "[";
            for (size_t i = 0; i < arr->size(); ++i) {
                if (i > 0) out += ", ";
                out += to_string(arr->get(i)); 
            }
            out += "]";
            return out;
        }

        case ObjectType::HASH_TABLE: {
            hash_table_t hash = reinterpret_cast<hash_table_t>(obj);
            std::string out = "{";
            bool first = true;
            for (auto it = hash->begin(); it != hash->end(); ++it) {
                if (!first) out += ", ";
                out += "\"" + std::string(it->first->c_str()) + "\": ";
                out += to_string(it->second);
                first = false;
            }
            out += "}";
            return out;
        }

        case ObjectType::CLASS: {
            auto name = reinterpret_cast<class_t>(obj)->get_name();
            return "<class '" + (name ? std::string(name->c_str()) : "??") + "'>";
        }

        case ObjectType::INSTANCE: {
            auto name = reinterpret_cast<instance_t>(obj)->get_class()->get_name();
            return "<" + (name ? std::string(name->c_str()) : "??") + " instance>";
        }

        case ObjectType::BOUND_METHOD:
            return "<bound_method>";

        case ObjectType::MODULE: {
            auto name = reinterpret_cast<module_t>(obj)->get_file_name();
            return "<module '" + (name ? std::string(name->c_str()) : "??") + "'>";
        }

        case ObjectType::FUNCTION: {
            auto name = reinterpret_cast<function_t>(obj)->get_proto()->get_name();
            return "<function '" + (name ? std::string(name->c_str()) : "??") + "'>";
        }

        case ObjectType::PROTO: {
            auto proto = reinterpret_cast<proto_t>(obj);
            auto name = proto->get_name();

            return std::format("<proto '{}'>\n  - registers: {}\n  - upvalues:  {}\n  - constants: {}\n{}",
                (name ? name->c_str() : "??"),
                proto->get_num_registers(),
                proto->get_num_upvalues(),
                proto->get_chunk().get_pool_size(),
                disassemble_chunk(proto->get_chunk())
            );
        }

        case ObjectType::UPVALUE:
            return "<upvalue>";

        default:
            return "<unknown_object_type>";
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
            
            std::string str = std::format("{}", val);
            
            auto pos = str.find('.');
            if (pos != std::string::npos) {
                size_t end = str.size();
                while (end > pos + 1 && str[end - 1] == '0') {
                    --end;
                }
                if (end == pos + 1) {
                    end++; 
                }
                return str.substr(0, end);
            }
            return str;
        },
        [](bool_t val) -> std::string { return val ? "true" : "false"; },
        [](native_t) -> std::string { return "<native_fn>"; },
        [](object_t obj) -> std::string {
            return detail::object_to_string(obj);
        }
    );
}

}