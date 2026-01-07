#include "meow/bytecode/op_codes.h"
#include <type_traits>

namespace meow {

using enum OpCode;
using enum ArgType;

namespace {

    constexpr auto reg8   = REG8;
    constexpr auto reg16  = REG16;
    
    constexpr auto u16    = U16;
    constexpr auto u32    = U32;
    constexpr auto i64    = I64;
    constexpr auto f64    = F64;
    
    constexpr auto off16  = OFFSET16;
    constexpr auto idx    = CONST_IDX;

    struct Entry {
        OpCode op;
        OpSchema schema;
    };

    template <typename... Args>
    constexpr Entry def(OpCode op, Args... args) {
        return {op, OpSchema(meow::enum_name(op), args...)};
    }

    template <typename... Entries>
    constexpr auto make_table(Entries... entries) {
        std::array<OpSchema, 256> table{};
        ((table[static_cast<size_t>(entries.op)] = entries.schema), ...);
        return table;
    }

} // namespace

static constexpr auto OP_TABLE = make_table(
    // 1. CORE
    def(NOP),
    def(HALT),
    def(MOVE,           reg16, reg16),
    
    // 2. CONSTANTS
    def(LOAD_CONST,     reg16, idx),
    def(LOAD_NULL,      reg16),
    def(LOAD_TRUE,      reg16),
    def(LOAD_FALSE,     reg16),
    def(LOAD_INT,       reg16, i64),
    def(LOAD_FLOAT,     reg16, f64),

    // 3. MATH (16-bit)
    def(ADD,            reg16, reg16, reg16),
    def(SUB,            reg16, reg16, reg16),
    def(MUL,            reg16, reg16, reg16),
    def(DIV,            reg16, reg16, reg16),
    def(MOD,            reg16, reg16, reg16),
    def(POW,            reg16, reg16, reg16),
    def(NEG,            reg16, reg16),
    def(INC,            reg16),
    def(DEC,            reg16),

    // 4. LOGIC & BITWISE (16-bit)
    def(NOT,            reg16, reg16),
    def(BIT_NOT,        reg16, reg16),
    def(BIT_AND,        reg16, reg16, reg16),
    def(BIT_OR,         reg16, reg16, reg16),
    def(BIT_XOR,        reg16, reg16, reg16),
    def(LSHIFT,         reg16, reg16, reg16),
    def(RSHIFT,         reg16, reg16, reg16),
    def(EQ,             reg16, reg16, reg16),
    def(NEQ,            reg16, reg16, reg16),
    def(GT,             reg16, reg16, reg16),
    def(GE,             reg16, reg16, reg16),
    def(LT,             reg16, reg16, reg16),
    def(LE,             reg16, reg16, reg16),

    // 5. FLOW CONTROL
    def(JUMP,           off16),
    def(JUMP_IF_TRUE,   reg16, off16),
    def(JUMP_IF_FALSE,  reg16, off16),
    def(CALL,           reg16, reg16, u16, u16), 
    def(CALL_VOID,      reg16, u16, u16),        
    def(RETURN,         reg16),
    def(TAIL_CALL,      reg16, reg16, u16, u16),

    // 6. EXCEPTION & MODULES
    def(THROW,          reg16),
    def(SETUP_TRY,      off16, reg16), 
    def(POP_TRY),
    def(IMPORT_MODULE,  reg16, idx),
    def(EXPORT,         idx, reg16),
    def(GET_EXPORT,     reg16, reg16, idx),
    def(IMPORT_ALL,     reg16),

    // 7. DATA & OOP
    def(NEW_ARRAY,      reg16, u16, u16),
    def(NEW_HASH,       reg16, u16, u16),
    def(GET_INDEX,      reg16, reg16, reg16),
    def(SET_INDEX,      reg16, reg16, reg16),
    def(GET_KEYS,       reg16, reg16),
    def(GET_VALUES,     reg16, reg16),
    
    def(NEW_CLASS,      reg16, idx),
    def(NEW_INSTANCE,   reg16, reg16),
    def(GET_PROP,       reg16, reg16, idx),
    def(SET_PROP,       reg16, idx, reg16),
    def(SET_METHOD,     reg16, idx, reg16),
    def(INHERIT,        reg16, reg16),
    def(GET_SUPER,      reg16, idx),
    def(INVOKE,         reg16, reg16, idx, u16, u16),

    // 8. CLOSURES
    def(GET_GLOBAL,     reg16, idx),
    def(SET_GLOBAL,     idx, reg16),
    def(GET_UPVALUE,    reg16, u16),
    def(SET_UPVALUE,    u16, reg16),
    def(CLOSURE,        reg16, idx),
    def(CLOSE_UPVALUES, reg16),

    // ==========================================
    //            OPTIMIZED OPS (_B)
    // ==========================================
    def(ADD_B,          reg8, reg8, reg8),
    def(SUB_B,          reg8, reg8, reg8),
    def(MUL_B,          reg8, reg8, reg8),
    def(DIV_B,          reg8, reg8, reg8),
    def(MOD_B,          reg8, reg8, reg8),
    def(INC_B,          reg8),
    def(DEC_B,          reg8),
    def(NEG_B,          reg8, reg8),
    def(NOT_B,          reg8, reg8),

    def(BIT_AND_B,      reg8, reg8, reg8),
    def(BIT_OR_B,       reg8, reg8, reg8),
    def(BIT_XOR_B,      reg8, reg8, reg8),
    def(BIT_NOT_B,      reg8, reg8),
    def(LSHIFT_B,       reg8, reg8, reg8),
    def(RSHIFT_B,       reg8, reg8, reg8),

    def(EQ_B,           reg8, reg8, reg8),
    def(NEQ_B,          reg8, reg8, reg8),
    def(GT_B,           reg8, reg8, reg8),
    def(GE_B,           reg8, reg8, reg8),
    def(LT_B,           reg8, reg8, reg8),
    def(LE_B,           reg8, reg8, reg8),

    def(MOVE_B,         reg8, reg8),
    // SỬA: LOAD_CONST_B dùng idx (2 byte) để truy cập full constant pool
    def(LOAD_CONST_B,   reg8, idx), 
    def(LOAD_INT_B,     reg8, i64),
    
    // THÊM: Các lệnh Load _B còn thiếu
    def(LOAD_FLOAT_B,   reg8, f64),
    def(LOAD_NULL_B,    reg8),
    def(LOAD_TRUE_B,    reg8),
    def(LOAD_FALSE_B,   reg8),

    // Fused Jumps
    def(JUMP_IF_EQ,     reg16, reg16, off16),
    def(JUMP_IF_NEQ,    reg16, reg16, off16),
    def(JUMP_IF_GT,     reg16, reg16, off16),
    def(JUMP_IF_GE,     reg16, reg16, off16),
    def(JUMP_IF_LT,     reg16, reg16, off16),
    def(JUMP_IF_LE,     reg16, reg16, off16),

    def(JUMP_IF_TRUE_B, reg8, off16),
    def(JUMP_IF_FALSE_B,reg8, off16),
    def(JUMP_IF_EQ_B,   reg8, reg8, off16),
    def(JUMP_IF_NEQ_B,  reg8, reg8, off16),
    def(JUMP_IF_GT_B,   reg8, reg8, off16),
    def(JUMP_IF_GE_B,   reg8, reg8, off16),
    def(JUMP_IF_LT_B,   reg8, reg8, off16),
    def(JUMP_IF_LE_B,   reg8, reg8, off16)
);

const OpSchema& get_op_schema(OpCode op) {
    if (static_cast<size_t>(op) >= OP_TABLE.size()) {
        static const OpSchema EMPTY;
        return EMPTY;
    }
    return OP_TABLE[static_cast<size_t>(op)];
}

OpInfo get_op_info(OpCode op) {
    const auto& s = get_op_schema(op);
    uint8_t size = s.get_operand_bytes();

    constexpr uint8_t IC_SIZE = 80;

    switch (op) {
        case OpCode::CALL: 
        case OpCode::CALL_VOID:
        case OpCode::TAIL_CALL: 
            size += 16; // CallIC
            break;
            
        case OpCode::GET_PROP: 
        case OpCode::SET_PROP:
        case OpCode::INVOKE:
            size += IC_SIZE; 
            break;
            
        default: break;
    }
    
    return { s.count, size };
}

} // namespace meow