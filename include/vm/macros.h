#pragma once

#define READ_BYTE() (*ip++)
#define READ_U16() (ip += 2, (uint16_t)((ip[-2] | (ip[-1] << 8))))
#define READ_U64()                                                                                                                                                                 \
    (ip += 8, (uint64_t)(ip[-8]) | ((uint64_t)(ip[-7]) << 8) | ((uint64_t)(ip[-6]) << 16) | ((uint64_t)(ip[-5]) << 24) | ((uint64_t)(ip[-4]) << 32) | ((uint64_t)(ip[-3]) << 40) | \
                  ((uint64_t)(ip[-2]) << 48) | ((uint64_t)(ip[-1]) << 56))

#define READ_I64() (std::bit_cast<int64_t>(READ_U64()))
#define READ_F64() (std::bit_cast<double>(READ_U64()))
#define READ_ADDRESS() READ_U16()

#define CURRENT_CHUNK() (context_->current_frame_->function_->get_proto()->get_chunk())
#define READ_CONSTANT() (CURRENT_CHUNK().get_constant(READ_U16()))

#define REGISTER(idx) (context_->registers_[context_->current_base_ + (idx)])
#define CONSTANT(idx) (CURRENT_CHUNK().get_constant(idx))

#define UNARY_OP_HANDLER(OPCODE, OPNAME) \
    op_##OPCODE: { \
        uint16_t dst = READ_U16(); \
        uint16_t src = READ_U16(); \
        auto& val = REGISTER(src); \
        if (auto func = op_dispatcher_->find(OpCode::OPCODE, val)) { \
            REGISTER(dst) = func(heap_.get(), val); \
        } else { \
            throw_vm_error("Unsupported unary operator " OPNAME); \
        } \
        DISPATCH(); \
    }

#define BINARY_OP_HANDLER(OPCODE, OPNAME) \
    op_##OPCODE: { \
        uint16_t dst = READ_U16(); \
        uint16_t r1 = READ_U16(); \
        uint16_t r2 = READ_U16(); \
        auto& left = REGISTER(r1); \
        auto& right = REGISTER(r2); \
        if (auto func = op_dispatcher_->find(OpCode::OPCODE, left, right)) { \
            REGISTER(dst) = func(heap_.get(), left, right); \
        } else { \
            throw_vm_error("Unsupported binary operator " OPNAME); \
        } \
        DISPATCH(); \
    }

#define DISPATCH()                                                \
    do {                                                          \
        context_->current_frame_->ip_ = ip;                       \
        uint8_t instruction = READ_BYTE();                        \
        [[assume(instruction < static_cast<size_t>(OpCode::TOTAL_OPCODES))]]; \
        goto *dispatch_table[instruction];                        \
    } while (0)