/*
type: uploaded file
fileName: masm/tests/test_optimizer.cpp
*/
#include <iostream>
#include <vector>
#include <iomanip>
#include <print>
#include <string>
#include <string_view>

#include "meow/masm/common.h"
#include "meow/masm/optimizer.h"
#include <meow_enum.h> 

using namespace meow;
using namespace meow::masm;

// --- Helper đọc số từ bytecode ---
uint16_t read_u16(const std::vector<uint8_t>& code, size_t& ip) {
    uint16_t v = code[ip] | (code[ip+1] << 8);
    ip += 2;
    return v;
}

uint8_t read_u8(const std::vector<uint8_t>& code, size_t& ip) {
    return code[ip++];
}

// --- Helper tạo lệnh nhanh ---
IrInstruction make_inst(OpCode op, std::vector<IrArg> args) {
    IrInstruction inst;
    inst.op = op;
    inst.arg_count = (uint8_t)args.size();
    for(size_t i=0; i<args.size(); ++i) inst.args[i] = args[i];
    return inst;
}

// --- Dump IR ---
void dump_ir_code(const std::vector<IrInstruction>& ir, std::string_view title) {
    std::println("\n┌────────────────────────────────────────────────────────┐");
    std::println("│ {:<54} │", title);
    std::println("├──────┬─────────────────┬───────────────────────────────┤");
    std::println("│ {:<4} │ {:<15} │ {:<29} │", "ID", "OPCODE", "ARGUMENTS");
    std::println("├──────┼─────────────────┼───────────────────────────────┤");

    for (size_t i = 0; i < ir.size(); ++i) {
        const auto& inst = ir[i];
        std::string_view op_name = meow::enum_name(inst.op);
        
        std::print("│ {:<4} │ {:<15} │ ", i, op_name);
        
        for (int k = 0; k < inst.arg_count; ++k) {
            if (k > 0) std::print(", ");
            inst.args[k].visit(
                [](Reg r)       { std::print("r{}", r.id); },
                [](LabelIdx l)  { std::print("Label[{}]", l.id); },
                [](auto)        { std::print("?"); }
            );
        }
        std::println(""); 
    }
    std::println("└──────┴─────────────────┴───────────────────────────────┘");
}

// --- Dump Bytecode (Disassembler thông minh) ---
void dump_bytecode(const std::vector<uint8_t>& code, std::string_view title) {
    std::println("\n┌────────────────────────────────────────────────────────┐");
    std::println("│ {:<54} │", title);
    std::println("├──────┬─────────────────┬───────────────────────────────┤");
    std::println("│ {:<4} │ {:<15} │ {:<29} │", "ADDR", "OPCODE", "OPERANDS");
    std::println("├──────┼─────────────────┼───────────────────────────────┤");
    
    size_t ip = 0;
    while (ip < code.size()) {
        size_t start_ip = ip;
        OpCode op = static_cast<OpCode>(code[ip++]);
        std::string_view op_name = meow::enum_name(op);

        std::print("│ {:04X} │ {:<15} │ ", start_ip, op_name);

        // Logic check loại OpCode để decode cho đúng
        bool is_byte_op = std::string(op_name).ends_with("_B");

        switch(op) {
            // --- STANDARD OPS (16-bit Registers) ---
            case OpCode::MOVE: {
                uint16_t dst = read_u16(code, ip); uint16_t src = read_u16(code, ip);
                std::println("r{}, r{}", dst, src); break;
            }
            case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::DIV: 
            case OpCode::EQ: case OpCode::NEQ: {
                uint16_t dst = read_u16(code, ip); uint16_t lhs = read_u16(code, ip); uint16_t rhs = read_u16(code, ip);
                std::println("r{}, r{}, r{}", dst, lhs, rhs); break;
            }
            case OpCode::JUMP_IF_EQ: case OpCode::JUMP_IF_NEQ: // JUMP_IF_EQ không có bản _B
            case OpCode::JUMP_IF_LT: case OpCode::JUMP_IF_LE: 
            case OpCode::JUMP_IF_GT: case OpCode::JUMP_IF_GE: {
                uint16_t lhs = read_u16(code, ip); uint16_t rhs = read_u16(code, ip);
                uint16_t off = read_u16(code, ip);
                std::println("r{}, r{}, @{:04X}", lhs, rhs, off); break;
            }
            
            // --- BYTE OPS (8-bit Registers) ---
            case OpCode::MOVE_B: {
                uint8_t dst = read_u8(code, ip); uint8_t src = read_u8(code, ip);
                std::println("r{}b, r{}b", dst, src); break;
            }
            case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: case OpCode::DIV_B:
            case OpCode::EQ_B: case OpCode::NEQ_B: {
                uint8_t dst = read_u8(code, ip); uint8_t lhs = read_u8(code, ip); uint8_t rhs = read_u8(code, ip);
                std::println("r{}b, r{}b, r{}b", dst, lhs, rhs); break;
            }

            // --- JUMPS ---
            case OpCode::JUMP_IF_TRUE: case OpCode::JUMP_IF_FALSE: {
                uint16_t r = read_u16(code, ip); uint16_t off = read_u16(code, ip);
                std::println("r{}, @{:04X}", r, off); break;
            }
            case OpCode::JUMP_IF_TRUE_B: case OpCode::JUMP_IF_FALSE_B: {
                uint8_t r = read_u8(code, ip); uint16_t off = read_u16(code, ip);
                std::println("r{}b, @{:04X}", r, off); break;
            }
            case OpCode::JUMP: {
                uint16_t off = read_u16(code, ip);
                std::println("@{:04X}", off); break;
            }
            
            case OpCode::NOP: { std::println(""); break; }
            default: { std::println("???"); break; }
        }
    }
    std::println("└──────┴─────────────────┴───────────────────────────────┘");
}

int main() {
    Prototype proto;
    std::vector<Prototype> all_protos;

    // --- KỊCH BẢN TEST ---
    
    // 1. Dead Code (MOVE r100, r100) -> Xóa
    proto.ir_code.push_back(make_inst(OpCode::MOVE, {Arg::R(100), Arg::R(100)}));

    // 2. Peephole (EQ + JUMP) -> JUMP_IF_EQ (Chuẩn u16, vì VM không có JUMP_IF_EQ_B)
    proto.ir_code.push_back(make_inst(OpCode::EQ, {Arg::R(50), Arg::R(1), Arg::R(2)}));
    proto.ir_code.push_back(make_inst(OpCode::JUMP_IF_TRUE, {Arg::R(50), Arg::Label(0)}));

    // 3. Bytecode Optimization (ADD r10, r1, r2) -> ADD_B (Vì số regs ít)
    //    Đây là phần kiểm tra xem Optimizer có tự chuyển sang _B không
    proto.ir_code.push_back(make_inst(OpCode::ADD, {Arg::R(10), Arg::R(1), Arg::R(2)}));

    // 4. Target Label
    proto.ir_code.push_back(make_inst(OpCode::NOP, {Arg::Label(0)}));

    dump_ir_code(proto.ir_code, "INPUT IR");

    // --- CHẠY OPTIMIZER ---
    OptConfig config;
    config.flags = OptFlags::O2; 
    Optimizer opt(proto, all_protos, config);
    
    std::println("\n       ⬇️  RUNNING OPTIMIZER (O2)... ⬇️");
    opt.run();

    dump_bytecode(proto.bytecode, "OUTPUT BYTECODE");

    return 0;
}