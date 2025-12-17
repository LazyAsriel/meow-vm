#pragma once

#include "jit_compiler.h"
#include "x64/assembler.h"
#include "x64/common.h"
#include <vector>
#include <unordered_map>

namespace meow::jit::x64 {

struct Fixup {
    size_t jump_op_pos; // Vị trí ghi opcode nhảy
    size_t target_bc;   // Bytecode đích đến
    bool is_cond;       // Là nhảy có điều kiện? (JCC) hay không (JMP)
};

struct SlowPath {
    size_t label_start;           // Địa chỉ bắt đầu sinh mã slow path
    std::vector<size_t> jumps_to_here; // Các chỗ cần patch để nhảy vào đây
    size_t patch_jump_over;       // Chỗ cần nhảy về sau khi xong (Fast path end)
    
    int op;           // Opcode gốc
    int dst_reg_idx;  // VM Register index
    int src1_reg_idx;
    int src2_reg_idx;
};

class CodeGenerator {
public:
    CodeGenerator(uint8_t* buffer, size_t capacity);
    
    // Compile bytecode -> trả về con trỏ hàm JIT
    JitFunc compile(const uint8_t* bytecode, size_t len);

private:
    Assembler asm_;
    
    // Map từ Bytecode Offset -> Native Code Offset
    std::unordered_map<size_t, size_t> bc_to_native_;
    
    // Danh sách cần fix nhảy (Forward Jumps)
    std::vector<Fixup> fixups_;
    
    // Danh sách Slow Paths (sinh mã ở cuối buffer)
    std::vector<SlowPath> slow_paths_;

    // --- Helpers ---
    void emit_prologue();
    void emit_epilogue();

    // Register Allocator đơn giản (Map VM Reg -> CPU Reg)
    Reg map_vm_reg(int vm_reg) const;
    void load_vm_reg(Reg cpu_dst, int vm_src);
    void store_vm_reg(int vm_dst, Reg cpu_src);

    // [MỚI] Đồng bộ trạng thái VM State (QUAN TRỌNG)
    void flush_cached_regs();
    void reload_cached_regs();
};

} // namespace meow::jit::x64