#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "bytecode/op_codes.h"
#include "core/value.h"

namespace meow { class MemoryManager; }

namespace meow {

constexpr size_t TYPE_BITS = 4;
constexpr size_t MAX_TYPES = 1 << TYPE_BITS; // 16

constexpr size_t OP_BITS = 8;
constexpr size_t MAX_OPS = 1 << OP_BITS; // 256

constexpr size_t BINARY_TABLE_SIZE = MAX_OPS * MAX_TYPES * MAX_TYPES;
constexpr size_t UNARY_TABLE_SIZE  = MAX_OPS * MAX_TYPES;

using binary_function_t = return_t (*)(MemoryManager*, param_t, param_t);
using unary_function_t  = return_t (*)(MemoryManager*, param_t);

class OperatorDispatcher {
public:
    explicit OperatorDispatcher(MemoryManager* heap) noexcept;

    [[nodiscard]] [[gnu::always_inline]] 
    inline binary_function_t find(OpCode op, const Value& lhs, const Value& rhs) const noexcept {
        const size_t idx = (std::to_underlying(op) << (TYPE_BITS * 2)) | 
                           (lhs.index() << TYPE_BITS) | rhs.index();
        
        return binary_dispatch_table_[idx];
    }

    [[nodiscard]] [[gnu::always_inline]]
    inline unary_function_t find(OpCode op, const Value& rhs) const noexcept {
        const size_t idx = (std::to_underlying(op) << TYPE_BITS) | rhs.index();
        return unary_dispatch_table_[idx];
    }

    [[nodiscard]] [[gnu::always_inline]]
    inline binary_function_t operator[](OpCode op, const Value& lhs, const Value& rhs) const noexcept {
        return find(op, lhs, rhs);
    }

    [[nodiscard]] [[gnu::always_inline]]
    inline unary_function_t operator[](OpCode op, const Value& rhs) const noexcept {
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