#include "runtime/operator_dispatcher.h"
#include <meow/memory/memory_manager.h>
#include <meow/cast.h>
#include <iostream>
#include <cmath>
#include <limits>

namespace meow {

static return_t trap_binary(MemoryManager*, param_t, param_t) {
    return Value(null_t{});
}

static return_t trap_unary(MemoryManager*, param_t) {
    return Value(null_t{});
}

static constexpr int64_t bool_to_int(bool b) { return b ? 1 : 0; }
static constexpr double bool_to_double(bool b) { return b ? 1.0 : 0.0; }

static Value string_concat(MemoryManager* heap, std::string_view s1, std::string_view s2) {
    std::string res;
    res.reserve(s1.size() + s2.size());
    res.append(s1);
    res.append(s2);
    return Value(heap->new_string(res));
}

static Value string_repeat(MemoryManager* heap, std::string_view s, int64_t times) {
    if (times <= 0) return Value(heap->new_string(""));
    std::string res;
    res.reserve(s.size() * static_cast<size_t>(times));
    for (int64_t i = 0; i < times; ++i) res.append(s);
    return Value(heap->new_string(res));
}

static Value safe_div(double a, double b) {
    if (b == 0.0) {
        if (a > 0.0) return Value(std::numeric_limits<double>::infinity());
        if (a < 0.0) return Value(-std::numeric_limits<double>::infinity());
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    return Value(a / b);
}

static Value pow_op(double a, double b) {
    return Value(std::pow(a, b));
}

static bool loose_eq(param_t a, param_t b) noexcept {
    if (a.is_int() && b.is_int()) return a.as_int() == b.as_int();
    if (a.is_float() && b.is_float()) return std::abs(a.as_float() - b.as_float()) < std::numeric_limits<double>::epsilon();
    if (a.is_int() && b.is_float()) return std::abs(static_cast<double>(a.as_int()) - b.as_float()) < std::numeric_limits<double>::epsilon();
    if (a.is_float() && b.is_int()) return std::abs(a.as_float() - static_cast<double>(b.as_int())) < std::numeric_limits<double>::epsilon();
    if (a.is_bool() && b.is_bool()) return a.as_bool() == b.as_bool();
    if (a.is_string() && b.is_string()) return a.as_string() == b.as_string(); 
    if (a.is_null() && b.is_null()) return true;
    if (a.is_bool() && b.is_int()) return bool_to_int(a.as_bool()) == b.as_int();
    if (a.is_int() && b.is_bool()) return a.as_int() == bool_to_int(b.as_bool());
    if (a.is_object() && b.is_object()) {
        return a.as_object() == b.as_object();
    }

    return false;
}

// --- Index Calculation ---
consteval size_t calc_bin_idx(OpCode op, ValueType lhs, ValueType rhs) {
    const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    return (op_idx << (TYPE_BITS * 2)) | (std::to_underlying(lhs) << TYPE_BITS) | std::to_underlying(rhs);
}

consteval size_t calc_un_idx(OpCode op, ValueType rhs) {
    const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    return (op_idx << TYPE_BITS) | std::to_underlying(rhs);
}

consteval auto make_binary_table() {
    std::array<binary_function_t, BINARY_TABLE_SIZE> table;
    table.fill(trap_binary);

    auto reg = [&](OpCode op, ValueType t1, ValueType t2, binary_function_t f) {
        table[calc_bin_idx(op, t1, t2)] = f;
    };

    using enum OpCode;
    using enum ValueType;

    // ADD
    reg(ADD, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() + b.as_int()); });
    reg(ADD, Float, Float, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() + b.as_float()); });
    reg(ADD, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<double>(a.as_int()) + b.as_float()); });
    reg(ADD, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() + static_cast<double>(b.as_int())); });
    
    reg(ADD, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() + bool_to_int(b.as_bool())); });
    reg(ADD, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) + b.as_int()); });
    reg(ADD, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() + bool_to_double(b.as_bool())); });
    reg(ADD, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_double(a.as_bool()) + b.as_float()); });
    reg(ADD, Bool, Bool,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<int64_t>(bool_to_int(a.as_bool()) + bool_to_int(b.as_bool()))); });

    reg(ADD, String, String, [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), b.as_string()->c_str()); });
    
    // Stateless wrappers for string concat with any
    reg(ADD, String, Int,    [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });
    reg(ADD, String, Float,  [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });
    reg(ADD, String, Bool,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });
    reg(ADD, String, Null,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });

    reg(ADD, Int, String,    [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });
    reg(ADD, Float, String,  [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });
    reg(ADD, Bool, String,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });
    reg(ADD, Null, String,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });

    // SUB
    reg(SUB, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() - b.as_int()); });
    reg(SUB, Float, Float, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() - b.as_float()); });
    reg(SUB, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<double>(a.as_int()) - b.as_float()); });
    reg(SUB, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() - static_cast<double>(b.as_int())); });
    reg(SUB, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() - bool_to_int(b.as_bool())); });
    reg(SUB, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) - b.as_int()); });
    reg(SUB, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() - bool_to_double(b.as_bool())); });
    reg(SUB, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_double(a.as_bool()) - b.as_float()); });
    reg(SUB, Bool, Bool,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<int64_t>(bool_to_int(a.as_bool()) - bool_to_int(b.as_bool()))); });

    // MUL
    reg(MUL, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() * b.as_int()); });
    reg(MUL, Float, Float, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() * b.as_float()); });
    reg(MUL, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<double>(a.as_int()) * b.as_float()); });
    reg(MUL, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() * static_cast<double>(b.as_int())); });
    reg(MUL, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() * bool_to_int(b.as_bool())); });
    reg(MUL, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) * b.as_int()); });
    reg(MUL, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() * bool_to_double(b.as_bool())); });
    reg(MUL, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_double(a.as_bool()) * b.as_float()); });
    
    reg(MUL, String, Int,  [](MemoryManager* h, param_t a, param_t b) { return string_repeat(h, a.as_string()->c_str(), b.as_int()); });
    reg(MUL, String, Bool, [](MemoryManager* h, param_t a, param_t b) { return string_repeat(h, a.as_string()->c_str(), bool_to_int(b.as_bool())); });

    // DIV
    reg(DIV, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return safe_div(static_cast<double>(a.as_int()), static_cast<double>(b.as_int())); });
    reg(DIV, Float, Float, [](MemoryManager*, param_t a, param_t b) { return safe_div(a.as_float(), b.as_float()); });
    reg(DIV, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return safe_div(static_cast<double>(a.as_int()), b.as_float()); });
    reg(DIV, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return safe_div(a.as_float(), static_cast<double>(b.as_int())); });
    reg(DIV, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return safe_div(static_cast<double>(a.as_int()), bool_to_double(b.as_bool())); });
    reg(DIV, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return safe_div(bool_to_double(a.as_bool()), static_cast<double>(b.as_int())); });
    reg(DIV, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return safe_div(bool_to_double(a.as_bool()), b.as_float()); });
    reg(DIV, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return safe_div(a.as_float(), bool_to_double(b.as_bool())); });

    // MOD
    reg(MOD, Int, Int, [](MemoryManager*, param_t a, param_t b) {
        if (b.as_int() == 0) return Value(std::numeric_limits<double>::quiet_NaN());
        return Value(a.as_int() % b.as_int());
    });
    reg(MOD, Int, Bool, [](MemoryManager*, param_t a, param_t b) {
        int64_t div = bool_to_int(b.as_bool());
        if (div == 0) return Value(std::numeric_limits<double>::quiet_NaN());
        return Value(a.as_int() % div);
    });

    // POW
    reg(POW, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return pow_op(static_cast<double>(a.as_int()), static_cast<double>(b.as_int())); });
    reg(POW, Float, Float, [](MemoryManager*, param_t a, param_t b) { return pow_op(a.as_float(), b.as_float()); });
    reg(POW, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return pow_op(static_cast<double>(a.as_int()), b.as_float()); });
    reg(POW, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return pow_op(a.as_float(), static_cast<double>(b.as_int())); });

    // BITWISE
    reg(BIT_AND, Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() & b.as_int()); });
    reg(BIT_OR,  Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() | b.as_int()); });
    reg(BIT_XOR, Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() ^ b.as_int()); });
    reg(LSHIFT,  Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() << b.as_int()); });
    reg(RSHIFT,  Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() >> b.as_int()); });
    
    // Explicit casts to bool or int to resolve ambiguity
    reg(BIT_AND, Bool, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<bool>(static_cast<int>(a.as_bool()) & static_cast<int>(b.as_bool()))); });
    reg(BIT_OR,  Bool, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<bool>(static_cast<int>(a.as_bool()) | static_cast<int>(b.as_bool()))); });
    reg(BIT_XOR, Bool, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<bool>(static_cast<int>(a.as_bool()) ^ static_cast<int>(b.as_bool()))); });

    reg(BIT_AND, Int, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() & bool_to_int(b.as_bool())); });
    reg(BIT_AND, Bool, Int, [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) & b.as_int()); });
    reg(BIT_OR, Int, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() | bool_to_int(b.as_bool())); });
    reg(BIT_OR, Bool, Int,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) | b.as_int()); });

    // EQUALITY
    for (size_t i = 0; i < (1 << TYPE_BITS); ++i) {
        for (size_t j = 0; j < (1 << TYPE_BITS); ++j) {
            auto t1 = static_cast<ValueType>(i);
            auto t2 = static_cast<ValueType>(j);
            
            // loose_eq đã tự xử lý việc check type bên trong, nên an toàn tuyệt đối
            reg(EQ, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(loose_eq(a, b)); });
            reg(NEQ, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(!loose_eq(a, b)); });
        }
    }

    // COMPARISON
    reg(LT, Int, Int,       [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() < b.as_int()); });
    reg(GT, Int, Int,       [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() > b.as_int()); });
    
    auto reg_cmp = [&](ValueType t1, ValueType t2) {
        reg(LT, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) < to_float(b)); });
        reg(GT, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) > to_float(b)); });
        reg(LE, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) <= to_float(b)); });
        reg(GE, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) >= to_float(b)); });
    };

    reg_cmp(Float, Float); reg_cmp(Int, Float); reg_cmp(Float, Int);
    reg_cmp(Int, Bool); reg_cmp(Bool, Int); reg_cmp(Bool, Bool);

    reg(LT, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) < std::string_view(b.as_string()->c_str())); });
    reg(GT, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) > std::string_view(b.as_string()->c_str())); });
    reg(LE, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) <= std::string_view(b.as_string()->c_str())); });
    reg(GE, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) >= std::string_view(b.as_string()->c_str())); });

    return table;
}

consteval auto make_unary_table() {
    std::array<unary_function_t, UNARY_TABLE_SIZE> table;
    table.fill(trap_unary);

    auto reg = [&](OpCode op, ValueType t, unary_function_t f) {
        table[calc_un_idx(op, t)] = f;
    };

    using enum OpCode;
    using enum ValueType;

    reg(NEG, Int,   [](MemoryManager*, param_t v) { return Value(-v.as_int()); });
    reg(NEG, Float, [](MemoryManager*, param_t v) { return Value(-v.as_float()); });
    reg(NEG, Bool,  [](MemoryManager*, param_t v) { return Value(-bool_to_int(v.as_bool())); });

    reg(BIT_NOT, Int,  [](MemoryManager*, param_t v) { return Value(~v.as_int()); });
    reg(BIT_NOT, Bool, [](MemoryManager*, param_t v) { return Value(~bool_to_int(v.as_bool())); });

    auto logic_not = [](MemoryManager*, param_t v) { return Value(!to_bool(v)); };
    
    reg(NOT, Null, logic_not);
    reg(NOT, Bool, logic_not);
    reg(NOT, Int, logic_not);
    reg(NOT, Float, logic_not);
    reg(NOT, String, logic_not);
    reg(NOT, Array, logic_not);
    reg(NOT, HashTable, logic_not);
    reg(NOT, Instance, logic_not);
    reg(NOT, Class, logic_not);
    reg(NOT, Function, logic_not);
    reg(NOT, Module, logic_not);

    return table;
}

constinit const std::array<binary_function_t, BINARY_TABLE_SIZE> OperatorDispatcher::binary_dispatch_table_ = make_binary_table();
constinit const std::array<unary_function_t, UNARY_TABLE_SIZE> OperatorDispatcher::unary_dispatch_table_  = make_unary_table();

} // namespace meow