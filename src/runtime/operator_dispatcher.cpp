#include "runtime/operator_dispatcher.h"
#include "memory/memory_manager.h"

namespace meow {

static return_t trap_binary(MemoryManager*, param_t lhs, param_t rhs) {
    std::cerr << "[VM Panic] Invalid binary operand types.\n";
    std::terminate(); 
    return {}; 
}

static return_t trap_unary(MemoryManager*, param_t rhs) {
    std::cerr << "[VM Panic] Invalid unary operand type.\n";
    std::terminate();
    return {};
}

#define BINARY(OP, T1, T2, ...) \
    register_binary(OpCode::OP, ValueType::T1, ValueType::T2, \
    [](MemoryManager* heap, param_t lhs, param_t rhs) -> return_t { \
        __VA_ARGS__ \
    })

#define UNARY(OP, T1, ...) \
    register_unary(OpCode::OP, ValueType::T1, \
    [](MemoryManager* heap, param_t rhs) -> return_t { \
        __VA_ARGS__ \
    })

void OperatorDispatcher::register_binary(OpCode op, ValueType lhs, ValueType rhs, binary_function_t func) noexcept {
    const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    
    if (op_idx >= (1 << OP_BITS_COMPACT)) {
        std::cerr << "[VM Error] OpCode " << (int)std::to_underlying(op) 
                  << " is outside the dispatcher range!\n";
        return; 
    }

    const size_t idx = (op_idx << (TYPE_BITS * 2)) | 
                       (std::to_underlying(lhs) << TYPE_BITS) | 
                       std::to_underlying(rhs);
                       
    if (idx < BINARY_TABLE_SIZE) binary_dispatch_table_[idx] = func;
}

void OperatorDispatcher::register_unary(OpCode op, ValueType rhs, unary_function_t func) noexcept {
    const size_t op_idx = std::to_underlying(op) - OP_OFFSET;

    if (op_idx >= (1 << OP_BITS_COMPACT)) {
        std::cerr << "[VM Error] Unary OpCode " << (int)std::to_underlying(op) << " out of range!\n";
        return;
    }

    const size_t idx = (op_idx << TYPE_BITS) | std::to_underlying(rhs);
    
    if (idx < UNARY_TABLE_SIZE) {
        unary_dispatch_table_[idx] = func;
    }
}

OperatorDispatcher::OperatorDispatcher(MemoryManager* heap) noexcept : heap_(heap) {
    binary_dispatch_table_.fill(trap_binary);
    unary_dispatch_table_.fill(trap_unary);

    using enum OpCode;
    using enum ValueType;

    BINARY(ADD, Int, Int,     return Value(lhs.as_int() + rhs.as_int()); );
    BINARY(ADD, Float, Float, return Value(lhs.as_float() + rhs.as_float()); );
    BINARY(ADD, Int, Float,   return Value(static_cast<double>(lhs.as_int()) + rhs.as_float()); );
    BINARY(ADD, Float, Int,   return Value(lhs.as_float() + static_cast<double>(rhs.as_int())); );

    BINARY(ADD, String, String, 
        auto s1 = lhs.as_string();
        auto s2 = rhs.as_string();
        std::string res;
        res.reserve(s1->size() + s2->size());
        res.append(s1->c_str(), s1->size());
        res.append(s2->c_str(), s2->size());
        return Value(heap->new_string(res));
    );

    // --- SUB (-) ---
    BINARY(SUB, Int, Int,     return Value(lhs.as_int() - rhs.as_int()); );
    BINARY(SUB, Float, Float, return Value(lhs.as_float() - rhs.as_float()); );

    // --- MUL (*) ---
    BINARY(MUL, Int, Int,     return Value(lhs.as_int() * rhs.as_int()); );
    BINARY(MUL, Float, Float, return Value(lhs.as_float() * rhs.as_float()); );

    // --- DIV (/) & MOD (%) - Cần check chia cho 0 ---
    BINARY(DIV, Int, Int,
        if (rhs.as_int() == 0) [[unlikely]] {
            // Panic("Division by zero");
            return Value{}; 
        }
        return Value(lhs.as_int() / rhs.as_int());
    );
    
    BINARY(DIV, Float, Float, return Value(lhs.as_float() / rhs.as_float()); );

    BINARY(MOD, Int, Int,
        if (rhs.as_int() == 0) [[unlikely]] return Value{};
        return Value(lhs.as_int() % rhs.as_int());
    );

    // ------------------------------------------------------------------------
    // BITWISE (Int only)
    // ------------------------------------------------------------------------
    BINARY(BIT_AND, Int, Int, return Value(lhs.as_int() & rhs.as_int()); );
    BINARY(BIT_OR,  Int, Int, return Value(lhs.as_int() | rhs.as_int()); );
    BINARY(BIT_XOR, Int, Int, return Value(lhs.as_int() ^ rhs.as_int()); );
    
    BINARY(LSHIFT, Int, Int, return Value(lhs.as_int() << rhs.as_int()); );
    BINARY(RSHIFT, Int, Int, return Value(lhs.as_int() >> rhs.as_int()); );

    // ------------------------------------------------------------------------
    // COMPARISON (EQ, GT, LT...) -> Trả về Bool
    // ------------------------------------------------------------------------
    BINARY(EQ, Int, Int,     return Value(lhs.as_int() == rhs.as_int()); );
    BINARY(EQ, Float, Float, return Value(lhs.as_float() == rhs.as_float()); );
    // So sánh string bằng con trỏ (nếu đã interning) hoặc strcmp
    BINARY(EQ, String, String, return Value(lhs.as_string() == rhs.as_string()); );

    BINARY(LT, Int, Int,     return Value(lhs.as_int() < rhs.as_int()); );
    BINARY(GT, Int, Int,     return Value(lhs.as_int() > rhs.as_int()); );

    // ------------------------------------------------------------------------
    // UNARY (NEG, NOT, BIT_NOT)
    // ------------------------------------------------------------------------
    UNARY(NEG, Int,   return Value(-rhs.as_int()); );
    UNARY(NEG, Float, return Value(-rhs.as_float()); );
    
    UNARY(NOT, Bool,  return Value(!rhs.as_bool()); );
    UNARY(BIT_NOT, Int, return Value(~rhs.as_int()); );
}

#undef BINARY
#undef UNARY

} // namespace meow