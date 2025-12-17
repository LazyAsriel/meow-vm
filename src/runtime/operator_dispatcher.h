// File: src/runtime/operator_dispatcher.h
#pragma once
#include <meow/bytecode/op_codes.h>
#include <meow/value.h>
#include <meow/definitions.h>

namespace meow {

class MemoryManager;
constexpr size_t TYPE_BITS = 4;

constexpr size_t OP_BITS_COMPACT = 5; 
constexpr size_t OP_OFFSET = std::to_underlying(OpCode::__BEGIN_OPERATOR__) + 1;

constexpr size_t BINARY_TABLE_SIZE = (1 << OP_BITS_COMPACT) * (1 << TYPE_BITS) * (1 << TYPE_BITS);
constexpr size_t UNARY_TABLE_SIZE  = (1 << OP_BITS_COMPACT) * (1 << TYPE_BITS);

using binary_function_t = return_t (*)(MemoryManager*, param_t, param_t);
using unary_function_t  = return_t (*)(MemoryManager*, param_t);

class OperatorDispatcher {
public:
    [[gnu::always_inline]] 
    static inline ValueType get_detailed_type(param_t v) noexcept {
        if (v.is_object()) {
            switch (v.as_object()->get_type()) {
                case ObjectType::STRING: return ValueType::String;
                case ObjectType::ARRAY:  return ValueType::Array;
                case ObjectType::HASH_TABLE: return ValueType::HashTable;
                default: return ValueType::Object;
            }
        }
        return static_cast<ValueType>(v.index());
    }

    [[nodiscard]] 
    [[gnu::always_inline]] static binary_function_t find(OpCode op, param_t lhs, param_t rhs) noexcept {
        const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
        
        const size_t t1 = std::to_underlying(get_detailed_type(lhs));
        const size_t t2 = std::to_underlying(get_detailed_type(rhs));

        const size_t idx = (op_idx << (TYPE_BITS * 2)) | (t1 << TYPE_BITS) | t2;
                           
        return binary_dispatch_table_[idx];
    }

    [[nodiscard]]
    [[gnu::always_inline]] static unary_function_t find(OpCode op, param_t rhs) noexcept {
        const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
        
        const size_t t1 = std::to_underlying(get_detailed_type(rhs));
        
        const size_t idx = (op_idx << TYPE_BITS) | t1;
        return unary_dispatch_table_[idx];
    }

private:
    static const std::array<binary_function_t, BINARY_TABLE_SIZE> binary_dispatch_table_;
    static const std::array<unary_function_t, UNARY_TABLE_SIZE> unary_dispatch_table_;
};

} // namespace meow