#pragma once

#include "x64/emitter.h"
#include <meow/value.h>
#include <meow/compiler/op_codes.h>
#include <unordered_map>
#include <vector>

namespace meow::jit {

class Compiler {
public:
    using JitFunc = void (*)(Value* regs);

    Compiler();
    ~Compiler();

    JitFunc compile(const uint8_t* bytecode, size_t len);

private:
    uint8_t* code_mem_ = nullptr;
    size_t capacity_ = 1024 * 256;
    x64::Emitter emit_;
    
    struct Fixup {
        size_t jump_op_pos; // Vị trí bắt đầu lệnh jump
        size_t target_bc;   // Bytecode target
        bool is_cond;       // True nếu là Jcc (để tính offset lệnh)
    };
    std::vector<Fixup> fixups_;
    std::unordered_map<size_t, size_t> bc_to_native_;

    // Helpers
    x64::Reg map_vm_reg(int vm_reg) const;
    void load_vm_reg(x64::Reg cpu_dst, int vm_src);
    void store_vm_reg(int vm_dst, x64::Reg cpu_src);
    
    void emit_prologue();
    void emit_epilogue();
};

} // namespace meow::jit
