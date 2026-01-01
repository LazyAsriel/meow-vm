#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/memory/gc_disable_guard.h>
#include <meow/core/array.h>
#include <meow/core/hash_table.h>
#include <meow/core/string.h>
#include <meow/core/module.h>
#include <meow/cast.h>

namespace meow::natives::json {

class JsonParser {
private:
    std::string_view json_;
    size_t pos_ = 0;
    Machine* vm_;
    bool has_error_ = false;

    Value report_error() {
        has_error_ = true;
        return Value(null_t{});
    }

    char peek() const {
        return pos_ < json_.length() ? json_[pos_] : '\0';
    }

    void advance() {
        if (pos_ < json_.length()) pos_++;
    }

    void skip_whitespace() {
        while (pos_ < json_.length() && std::isspace(static_cast<unsigned char>(json_[pos_]))) {
            pos_++;
        }
    }

    Value parse_value();
    Value parse_object();
    Value parse_array();
    Value parse_string();
    Value parse_number();
    Value parse_true();
    Value parse_false();
    Value parse_null();

public:
    explicit JsonParser(Machine* vm) : vm_(vm) {}

    Value parse(std::string_view str) {
        json_ = str;
        pos_ = 0;
        has_error_ = false;
        
        skip_whitespace();
        if (json_.empty()) return Value(null_t{});

        Value result = parse_value();
        
        if (has_error_) return Value(null_t{});

        skip_whitespace();
        if (pos_ < json_.length()) {
            return report_error();
        }
        return result;
    }
};

Value JsonParser::parse_value() {
    skip_whitespace();
    if (pos_ >= json_.length()) return report_error();

    char c = peek();
    switch (c) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': return parse_string();
        case 't': return parse_true();
        case 'f': return parse_false();
        case 'n': return parse_null();
        default:
            if (std::isdigit(c) || c == '-') {
                return parse_number();
            }
            return report_error();
    }
}

Value JsonParser::parse_object() {
    advance();
    
    auto hash = vm_->get_heap()->new_hash();

    skip_whitespace();
    if (peek() == '}') {
        advance();
        return Value(hash);
    }

    while (true) {
        skip_whitespace();
        if (peek() != '"') return report_error();

        Value key_val = parse_string();
        if (has_error_) return key_val;

        string_t key_str = key_val.as_string();

        skip_whitespace();
        if (peek() != ':') return report_error();
        advance();

        Value val = parse_value();
        if (has_error_) return val;

        hash->set(key_str, val);

        skip_whitespace();
        char next = peek();
        if (next == '}') {
            advance();
            break;
        }
        if (next != ',') return report_error();
        advance();
    }
    return Value(hash);
}

Value JsonParser::parse_array() {
    advance();
    
    auto arr = vm_->get_heap()->new_array();

    skip_whitespace();
    if (peek() == ']') {
        advance();
        return Value(arr);
    }

    while (true) {
        Value elem = parse_value();
        if (has_error_) return elem;
        
        arr->push(elem);

        skip_whitespace();
        char next = peek();
        if (next == ']') {
            advance();
            break;
        }
        if (next != ',') return report_error();
        advance();
    }
    return Value(arr);
}

Value JsonParser::parse_string() {
    advance();
    std::string s;
    s.reserve(32);

    while (pos_ < json_.length() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (pos_ >= json_.length()) return report_error();
            
            char escaped = peek();
            switch (escaped) {
                case '"':  s += '"'; break;
                case '\\': s += '\\'; break;
                case '/':  s += '/'; break;
                case 'b':  s += '\b'; break;
                case 'f':  s += '\f'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                case 'u': {
                    advance(); // Skip 'u'
                    if (pos_ + 4 > json_.length()) return report_error();

                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = peek();
                        advance();
                        codepoint <<= 4;
                        if (h >= '0' && h <= '9') codepoint |= (h - '0');
                        else if (h >= 'a' && h <= 'f') codepoint |= (10 + h - 'a');
                        else if (h >= 'A' && h <= 'F') codepoint |= (10 + h - 'A');
                        else return report_error();
                    }

                    if (codepoint <= 0x7F) {
                        s += static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        s += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else {
                        s += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                        s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    continue;
                }
                default:
                    s += escaped; break;
            }
        } else {
            s += peek();
        }
        advance();
    }

    if (pos_ >= json_.length() || peek() != '"') {
        return report_error();
    }
    advance();

    return Value(vm_->get_heap()->new_string(s));
}

Value JsonParser::parse_number() {
    size_t start = pos_;
    if (peek() == '-') advance();

    if (peek() == '0') {
        advance();
    } else if (std::isdigit(peek())) {
        while (pos_ < json_.length() && std::isdigit(peek())) advance();
    } else {
        return report_error();
    }

    bool is_float = false;
    if (pos_ < json_.length() && peek() == '.') {
        is_float = true;
        advance();
        if (!std::isdigit(peek())) return report_error();
        while (pos_ < json_.length() && std::isdigit(peek())) advance();
    }

    if (pos_ < json_.length() && (peek() == 'e' || peek() == 'E')) {
        is_float = true;
        advance();
        if (pos_ < json_.length() && (peek() == '+' || peek() == '-')) advance();
        if (!std::isdigit(peek())) return report_error();
        while (pos_ < json_.length() && std::isdigit(peek())) advance();
    }

    std::string num_str(json_.substr(start, pos_ - start));
    
    if (is_float) {
        return Value(std::stod(num_str));
    } else {
        return Value(static_cast<int64_t>(std::stoll(num_str)));
    }
}

Value JsonParser::parse_true() {
    if (json_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return Value(true);
    }
    return report_error();
}

Value JsonParser::parse_false() {
    if (json_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return Value(false);
    }
    return report_error();
}

Value JsonParser::parse_null() {
    if (json_.substr(pos_, 4) == "null") {
        pos_ += 4;
        return Value(null_t{});
    }
    return report_error();
}

// ============================================================================
// 🖨️ JSON STRINGIFIER
// ============================================================================

static std::string escape_string(std::string_view s) {
    std::ostringstream o;
    o << '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c <= 0x1F) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    o << '"';
    return o.str();
}

static std::string to_json_recursive(const Value& val, int indent_level, int tab_size) {
    std::ostringstream ss;
    std::string indent(indent_level * tab_size, ' ');
    std::string inner_indent((indent_level + 1) * tab_size, ' ');
    bool pretty = (tab_size > 0);
    const char* newline = pretty ? "\n" : "";
    const char* sep = pretty ? ": " : ":";

    if (val.is_null()) {
        ss << "null";
    } 
    else if (val.is_bool()) {
        ss << (val.as_bool() ? "true" : "false");
    } 
    else if (val.is_int()) {
        ss << val.as_int();
    } 
    else if (val.is_float()) {
        ss << val.as_float();
    } 
    else if (val.is_string()) {
        ss << escape_string(val.as_string()->c_str());
    } 
    else if (val.is_array()) {
        array_t arr = val.as_array();
        if (arr->empty()) {
            ss << "[]";
        } else {
            ss << "[" << newline;
            for (size_t i = 0; i < arr->size(); ++i) {
                if (pretty) ss << inner_indent;
                ss << to_json_recursive(arr->get(i), indent_level + 1, tab_size);
                if (i + 1 < arr->size()) ss << ",";
                ss << newline;
            }
            if (pretty) ss << indent;
            ss << "]";
        }
    } 
    else if (val.is_hash_table()) {
        hash_table_t obj = val.as_hash_table();
        if (obj->empty()) {
            ss << "{}";
        } else {
            ss << "{" << newline;
            size_t i = 0;
            size_t size = obj->size();
            for (auto it = obj->begin(); it != obj->end(); ++it) {
                if (pretty) ss << inner_indent;
                ss << escape_string(it->first->c_str()) << sep;
                ss << to_json_recursive(it->second, indent_level + 1, tab_size);
                if (i + 1 < size) ss << ",";
                ss << newline;
                i++;
            }
            if (pretty) ss << indent;
            ss << "}";
        }
    } 
    else {
        ss << "\"<unsupported_type>\"";
    }
    
    return ss.str();
}

static Value json_parse(Machine* vm, int argc, Value* argv) {
    if (argc < 1 || !argv[0].is_string()) {
        return Value(null_t{});
    }

    std::string_view json_str = argv[0].as_string()->c_str();
    JsonParser parser(vm);
    return parser.parse(json_str);
}

static Value json_stringify(Machine* vm, int argc, Value* argv) {
    if (argc < 1) return Value(null_t{});
    
    int tab_size = 2;
    if (argc > 1 && argv[1].is_int()) {
        tab_size = static_cast<int>(argv[1].as_int());
        if (tab_size < 0) tab_size = 0;
    }
    
    std::string res = to_json_recursive(argv[0], 0, tab_size);
    return Value(vm->get_heap()->new_string(res));
}

} // namespace meow::natives::json

namespace meow::stdlib {

module_t create_json_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("json");
    auto mod = heap->new_module(name, name);

    auto reg = [&](const char* n, native_t fn) {
        mod->set_export(heap->new_string(n), Value(fn));
    };

    using namespace meow::natives::json;
    reg("parse", json_parse);
    reg("stringify", json_stringify);

    return mod;
}

} // namespace meow::stdlib