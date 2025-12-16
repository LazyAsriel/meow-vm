#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/memory/memory_manager.h>
#include <meow/memory/gc_disable_guard.h>
#include <meow/core/module.h>
#include <meow/core/array.h>
#include <meow/cast.h>

namespace meow::natives::str {

#define CHECK_SELF() \
    if (argc < 1 || !argv[0].is_string()) [[unlikely]] { \
        vm->error("String method expects 'this' to be a String."); \
        return Value(null_t{}); \
    } \
    string_t self_obj = argv[0].as_string(); \
    std::string_view self(self_obj->c_str(), self_obj->size()); \

static Value len(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    return Value((int64_t)self.size());
}

static Value upper(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    std::string s(self);
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return Value(vm->get_heap()->new_string(s));
}

static Value lower(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    std::string s(self);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return Value(vm->get_heap()->new_string(s));
}

static Value trim(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    while (!self.empty() && std::isspace(self.front())) self.remove_prefix(1);
    while (!self.empty() && std::isspace(self.back())) self.remove_suffix(1);
    return Value(vm->get_heap()->new_string(self));
}

static Value contains(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value(false);
    string_t needle_obj = argv[1].as_string();
    std::string_view needle(needle_obj->c_str(), needle_obj->size());
    return Value(self.find(needle) != std::string::npos);
}

static Value startsWith(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value(false);
    string_t prefix_obj = argv[1].as_string();
    std::string_view prefix(prefix_obj->c_str(), prefix_obj->size());
    return Value(self.starts_with(prefix));
}

static Value endsWith(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value(false);
    string_t suffix_obj = argv[1].as_string();
    std::string_view suffix(suffix_obj->c_str(), suffix_obj->size());
    return Value(self.ends_with(suffix));
}

static Value join(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_array()) return Value(vm->get_heap()->new_string(""));

    array_t arr = argv[1].as_array();
    std::ostringstream ss;
    for (size_t i = 0; i < arr->size(); ++i) {
        if (i > 0) ss << self;
        Value item = arr->get(i);
        if (item.is_string()) ss << item.as_string()->c_str();
        else ss << to_string(item);
    }
    return Value(vm->get_heap()->new_string(ss.str()));
}

static Value split(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    std::string_view delim = " ";
    if (argc >= 2 && argv[1].is_string()) {
        string_t d = argv[1].as_string();
        delim = std::string_view(d->c_str(), d->size());
    }

    auto arr = vm->get_heap()->new_array();
    
    if (delim.empty()) {
        for (char c : self) {
            arr->push(Value(vm->get_heap()->new_string(&c, 1)));
        }
    } else {
        size_t start = 0;
        size_t end = self.find(delim);
        while (end != std::string::npos) {
            std::string_view token = self.substr(start, end - start);
            arr->push(Value(vm->get_heap()->new_string(token)));
            start = end + delim.length();
            end = self.find(delim, start);
        }
        std::string_view last = self.substr(start);
        arr->push(Value(vm->get_heap()->new_string(last)));
    }
    return Value(arr);
}

static Value replace(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 3 || !argv[1].is_string() || !argv[2].is_string()) {
        return argv[0];
    }
    string_t from_obj = argv[1].as_string();
    string_t to_obj = argv[2].as_string();
    
    std::string_view from(from_obj->c_str(), from_obj->size());
    std::string_view to(to_obj->c_str(), to_obj->size());
    
    std::string s(self);
    size_t pos = s.find(from);
    if (pos != std::string::npos) {
        s.replace(pos, from.length(), to);
    }
    return Value(vm->get_heap()->new_string(s));
}

static Value indexOf(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value((int64_t)-1);
    
    string_t sub_obj = argv[1].as_string();
    size_t start_pos = 0;
    if (argc > 2 && argv[2].is_int()) {
        int64_t p = argv[2].as_int();
        if (p > 0) start_pos = static_cast<size_t>(p);
    }

    size_t pos = self.find(sub_obj->c_str(), start_pos);
    if (pos == std::string::npos) return Value((int64_t)-1);
    return Value((int64_t)pos);
}

static Value lastIndexOf(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value((int64_t)-1);
    
    string_t sub_obj = argv[1].as_string();
    size_t pos = self.rfind(sub_obj->c_str());
    if (pos == std::string::npos) return Value((int64_t)-1);
    return Value((int64_t)pos);
}

static Value substring(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) return argv[0];
    
    int64_t start = argv[1].as_int();
    int64_t length = (int64_t)self.size();
    
    if (argc > 2 && argv[2].is_int()) {
        length = argv[2].as_int();
    }
    
    if (start < 0) start = 0;
    if (start >= (int64_t)self.size()) return Value(vm->get_heap()->new_string(""));
    
    return Value(vm->get_heap()->new_string(self.substr(start, length)));
}

static Value slice_str(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    int64_t len = (int64_t)self.size();
    int64_t start = 0;
    int64_t end = len;

    if (argc >= 2 && argv[1].is_int()) {
        start = argv[1].as_int();
        if (start < 0) start += len;
        if (start < 0) start = 0;
    }
    if (argc >= 3 && argv[2].is_int()) {
        end = argv[2].as_int();
        if (end < 0) end += len;
    }
    if (start >= end || start >= len) return Value(vm->get_heap()->new_string(""));
    if (end > len) end = len;

    return Value(vm->get_heap()->new_string(self.substr(start, end - start)));
}

static Value repeat(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) return Value(vm->get_heap()->new_string(""));
    int64_t count = argv[1].as_int();
    if (count <= 0) return Value(vm->get_heap()->new_string(""));
    
    std::string res;
    res.reserve(self.size() * count);
    for(int i=0; i<count; ++i) res.append(self);
    
    return Value(vm->get_heap()->new_string(res));
}

static Value padLeft(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) return argv[0];
    int64_t target_len_i64 = argv[1].as_int();
    if (target_len_i64 < 0) return argv[0];
    size_t target_len = static_cast<size_t>(target_len_i64);

    if (target_len <= self.size()) return argv[0];
    
    std::string_view pad_char = " ";
    if (argc > 2 && argv[2].is_string()) {
        string_t p = argv[2].as_string();
        if (!p->empty()) pad_char = std::string_view(p->c_str(), p->size());
    }

    std::string res;
    size_t needed_len = target_len - self.size();
    while (res.size() < needed_len) res.append(pad_char);
    res.resize(needed_len); 
    res.append(self);
    
    return Value(vm->get_heap()->new_string(res));
}

static Value padRight(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) return argv[0];
    int64_t target_len_i64 = argv[1].as_int();
    if (target_len_i64 < 0) return argv[0];
    size_t target_len = static_cast<size_t>(target_len_i64);

    if (target_len <= self.size()) return argv[0];
    
    std::string_view pad_char = " ";
    if (argc > 2 && argv[2].is_string()) {
        string_t p = argv[2].as_string();
        if (!p->empty()) pad_char = std::string_view(p->c_str(), p->size());
    }

    std::string res(self);
    while (res.size() < target_len) res.append(pad_char);
    res.resize(target_len);
    
    return Value(vm->get_heap()->new_string(res));
}

static Value equalsIgnoreCase(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value(false);
    string_t other = argv[1].as_string();
    if (self.size() != other->size()) return Value(false);
    
    return Value(std::equal(self.begin(), self.end(), other->c_str(), 
        [](char a, char b) { return tolower(a) == tolower(b); }));
}

static Value charAt(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) return Value(vm->get_heap()->new_string(""));
    int64_t idx = argv[1].as_int();
    if (idx < 0 || idx >= (int64_t)self.size()) return Value(vm->get_heap()->new_string(""));
    
    char c = self[idx];
    return Value(vm->get_heap()->new_string(&c, 1));
}

static Value charCodeAt(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) return Value((int64_t)-1);
    int64_t idx = argv[1].as_int();
    if (idx < 0 || idx >= (int64_t)self.size()) return Value((int64_t)-1);
    
    return Value((int64_t)(unsigned char)self[idx]);
}

} // namespace meow::natives::str

namespace meow::stdlib {
module_t create_string_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("string");
    auto mod = heap->new_module(name, name);
    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };

    using namespace meow::natives::str;
    reg("len", len);
    reg("size", len);
    reg("length", len);
    
    reg("upper", upper);
    reg("lower", lower);
    reg("trim", trim);
    
    reg("contains", contains);
    reg("startsWith", startsWith);
    reg("endsWith", endsWith);
    reg("join", join);
    reg("split", split);
    reg("replace", replace);
    reg("indexOf", indexOf);
    reg("lastIndexOf", lastIndexOf);
    reg("substring", substring);
    reg("slice", slice_str);
    reg("repeat", repeat);
    reg("padLeft", padLeft);
    reg("padRight", padRight);
    reg("equalsIgnoreCase", equalsIgnoreCase);
    reg("charAt", charAt);
    reg("charCodeAt", charCodeAt);
    
    return mod;
}
}