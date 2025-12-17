/**
 * @file code_generator.h
 * @brief Translates Meow Bytecode into x64 Assembly
 */

#pragma once

#include "jit/x64/assembler.h"
#include "jit/jit_compiler.h" // For JitFunc
#include "meow/bytecode/op_codes.h"
#include <vector>
#include <map>

namespace meow::jit::x64 {

    class CodeGenerator {
    public:
        CodeGenerator(uint8_t* buffer, size_t capacity);

        // Hàm chính: Dịch bytecode
        JitFunc compile(const uint8_t* bytecode, size_t len);

    private:
        // --- Helpers ---
        void emit_prologue();
        void emit_epilogue();
        
        // Quản lý thanh ghi (Register Allocation đơn giản)
        Reg map_vm_reg(int vm_reg) const;
        void load_vm_reg(Reg cpu_dst, int vm_src);
        void store_vm_reg(int vm_dst, Reg cpu_src);

        // Sinh mã cho lệnh so sánh (EQ, LT, GT...)
        Condition emit_cmp_logic(OpCode op_cmp, uint16_t r1, uint16_t r2);

        // --- Data Structures ---
        
        // Fixup: Các vị trí Jump cần điền địa chỉ sau
        struct Fixup {
            size_t jump_op_pos; // Vị trí lệnh nhảy
            size_t target_bc;   // Bytecode đích cần nhảy tới
            bool is_cond;       // Là nhảy có điều kiện (JCC) hay không (JMP)
        };

        // SlowPath: Đoạn code xử lý trường hợp ngoại lệ/chậm (đặt cuối hàm)
        struct SlowPath {
            size_t jump_from; // Nơi nhảy tới đây
            size_t label_start; // Địa chỉ bắt đầu của slow path
            // Có thể thêm info để quay về
        };

        Assembler asm_;
        
        // Mapping: Bytecode Offset -> Native Offset (để tính toán Jump)
        std::map<size_t, size_t> bc_to_native_;
        std::vector<Fixup> fixups_;
    };

} // namespace meow::jit::x64