#include "runtime/operator_dispatcher.h"
#include "memory/memory_manager.h"
#include <iostream>

namespace meow {

// --- Trap Handlers (Xử lý lỗi) ---
static return_t trap_binary(MemoryManager*, param_t, param_t) {
    std::cerr << "[VM Panic] Invalid binary operand types.\n";
    std::terminate(); 
    return {}; 
}

static return_t trap_unary(MemoryManager*, param_t) {
    std::cerr << "[VM Panic] Invalid unary operand type.\n";
    std::terminate();
    return {};
}

// --- Compile-time Helpers ---

consteval size_t calc_bin_idx(OpCode op, ValueType lhs, ValueType rhs) {
    const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    return (op_idx << (TYPE_BITS * 2)) | 
           (std::to_underlying(lhs) << TYPE_BITS) | 
           std::to_underlying(rhs);
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
    reg(ADD, Int, Int,     [](auto*, param_t a, param_t b) { return Value(a.as_int() + b.as_int()); });
    reg(ADD, Float, Float, [](auto*, param_t a, param_t b) { return Value(a.as_float() + b.as_float()); });
    reg(ADD, Int, Float,   [](auto*, param_t a, param_t b) { return Value(static_cast<double>(a.as_int()) + b.as_float()); });
    reg(ADD, Float, Int,   [](auto*, param_t a, param_t b) { return Value(a.as_float() + static_cast<double>(b.as_int())); });
    
    reg(ADD, String, String, [](MemoryManager* heap, param_t a, param_t b) -> return_t {
        auto s1 = a.as_string();
        auto s2 = b.as_string();
        std::string res;
        res.reserve(s1->size() + s2->size());
        res.append(s1->c_str(), s1->size());
        res.append(s2->c_str(), s2->size());
        return Value(heap->new_string(res));
    });

    // SUB
    reg(SUB, Int, Int,     [](auto*, param_t a, param_t b) { return Value(a.as_int() - b.as_int()); });
    reg(SUB, Float, Float, [](auto*, param_t a, param_t b) { return Value(a.as_float() - b.as_float()); });

    // MUL
    reg(MUL, Int, Int,     [](auto*, param_t a, param_t b) { return Value(a.as_int() * b.as_int()); });
    reg(MUL, Float, Float, [](auto*, param_t a, param_t b) { return Value(a.as_float() * b.as_float()); });

    // DIV
    reg(DIV, Int, Int, [](auto*, param_t a, param_t b) {
        if (b.as_int() == 0) [[unlikely]] return Value{}; // TODO: Error handling
        return Value(a.as_int() / b.as_int());
    });
    reg(DIV, Float, Float, [](auto*, param_t a, param_t b) { return Value(a.as_float() / b.as_float()); });

    // MOD
    reg(MOD, Int, Int, [](auto*, param_t a, param_t b) {
        if (b.as_int() == 0) [[unlikely]] return Value{};
        return Value(a.as_int() % b.as_int());
    });

    // BITWISE
    reg(BIT_AND, Int, Int, [](auto*, param_t a, param_t b) { return Value(a.as_int() & b.as_int()); });
    reg(BIT_OR,  Int, Int, [](auto*, param_t a, param_t b) { return Value(a.as_int() | b.as_int()); });
    reg(BIT_XOR, Int, Int, [](auto*, param_t a, param_t b) { return Value(a.as_int() ^ b.as_int()); });
    reg(LSHIFT,  Int, Int, [](auto*, param_t a, param_t b) { return Value(a.as_int() << b.as_int()); });
    reg(RSHIFT,  Int, Int, [](auto*, param_t a, param_t b) { return Value(a.as_int() >> b.as_int()); });

    // COMPARISON
    reg(EQ, Int, Int,       [](auto*, param_t a, param_t b) { return Value(a.as_int() == b.as_int()); });
    reg(EQ, Float, Float,   [](auto*, param_t a, param_t b) { return Value(a.as_float() == b.as_float()); });
    reg(EQ, String, String, [](auto*, param_t a, param_t b) { return Value(a.as_string() == b.as_string()); });

    reg(LT, Int, Int,       [](auto*, param_t a, param_t b) { return Value(a.as_int() < b.as_int()); });
    reg(GT, Int, Int,       [](auto*, param_t a, param_t b) { return Value(a.as_int() > b.as_int()); });

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

    reg(NEG, Int,   [](auto*, param_t v) { return Value(-v.as_int()); });
    reg(NEG, Float, [](auto*, param_t v) { return Value(-v.as_float()); });
    reg(NOT, Bool,  [](auto*, param_t v) { return Value(!v.as_bool()); });
    reg(BIT_NOT, Int, [](auto*, param_t v) { return Value(~v.as_int()); });

    return table;
}

constinit const std::array<binary_function_t, BINARY_TABLE_SIZE> OperatorDispatcher::binary_dispatch_table_ = make_binary_table();
constinit const std::array<unary_function_t, UNARY_TABLE_SIZE> OperatorDispatcher::unary_dispatch_table_  = make_unary_table();

} // namespace meow