#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "vm/vm_state.h"

namespace meow {
class Interpreter {
public:
    static void run(VMState state) noexcept;
private:
    using OpHandler = void (*)(const uint8_t* ip, VMState state);

    static const std::array<OpHandler, 256> dispatch_table;

    template <typename T>
    [[gnu::always_inline]] 
    static T read(const uint8_t*& ip) noexcept {
        T val;
        std::memcpy(&val, ip, sizeof(T));
        ip += sizeof(T);
        return val;
    }
    
    [[gnu::always_inline]]
    static uint16_t read_u16(const uint8_t*& ip) noexcept {
        uint16_t val = static_cast<uint16_t>(ip[0]) | (static_cast<uint16_t>(ip[1]) << 8);
        ip += 2;
        return val;
    }
    
    [[gnu::always_inline]]
    static void dispatch(const uint8_t* ip, VMState state) noexcept {
        uint8_t opcode = *ip++;
        [[clang::musttail]] return dispatch_table[opcode](ip, state);
    }
    
    static void op_LOAD_CONST(const uint8_t* ip, VMState state) noexcept;
    static void op_ADD(const uint8_t* ip, VMState state) noexcept;
    static void op_RETURN(const uint8_t* ip, VMState state) noexcept;
    static void op_HALT(const uint8_t* ip, VMState state) noexcept;
    
    static void op_PANIC(const uint8_t* ip, VMState state) noexcept;
};

}