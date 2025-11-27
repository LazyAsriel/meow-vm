#pragma once
#include "bytecode/op_codes.h"
#include "core/value.h"
#include "common/definitions.h" // Chứa [[gnu::always_inline]]

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
    explicit OperatorDispatcher(MemoryManager* heap) noexcept;

    [[nodiscard]] 
    [[gnu::always_inline]] binary_function_t find(OpCode op, const Value& lhs, const Value& rhs) const noexcept {
        const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
        
        const size_t idx = (op_idx << (TYPE_BITS * 2)) | 
                           (lhs.index() << TYPE_BITS) | 
                           rhs.index();
        return binary_dispatch_table_[idx];
    }

    [[nodiscard]]
    [[gnu::always_inline]] unary_function_t find(OpCode op, const Value& rhs) const noexcept {
        const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
        
        const size_t idx = (op_idx << TYPE_BITS) | rhs.index();
        return unary_dispatch_table_[idx];
    }

    [[nodiscard]] [[gnu::always_inline]] binary_function_t operator[](OpCode op, const Value& lhs, const Value& rhs) const noexcept {
        return find(op, lhs, rhs);
    }
    [[nodiscard]] [[gnu::always_inline]] unary_function_t operator[](OpCode op, const Value& rhs) const noexcept {
        return find(op, rhs);
    }

    void register_binary(OpCode op, ValueType lhs, ValueType rhs, binary_function_t func) noexcept;
    void register_unary(OpCode op, ValueType rhs, unary_function_t func) noexcept;

private:
    [[maybe_unused]] MemoryManager* heap_;
    
    std::array<binary_function_t, BINARY_TABLE_SIZE> binary_dispatch_table_;
    std::array<unary_function_t, UNARY_TABLE_SIZE> unary_dispatch_table_;
};

} // namespace meow