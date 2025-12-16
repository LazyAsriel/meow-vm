#pragma once
#include "emitter.h"
#include <meow/value.h>
#include <meow/bytecode/op_codes.h>
#include <vector>
#include <unordered_map>

namespace meow { struct VMState; }

namespace meow::jit::x64 {

class Compiler {
public:
    using JitFunc = void (*)(Value* regs, const Value* consts, VMState* state);

    Compiler();
    ~Compiler();

    JitFunc compile(const uint8_t* bytecode, size_t len);

private:
    uint8_t* code_mem_ = nullptr;
    size_t capacity_ = 1024 * 256;
    Emitter emit_;
    
    struct Fixup {
        size_t jump_op_pos;
        size_t target_bc;
        bool is_cond;
    };
    std::vector<Fixup> fixups_;
    std::unordered_map<size_t, size_t> bc_to_native_;

    Reg map_vm_reg(int vm_reg) const;
    void load_vm_reg(Reg cpu_dst, int vm_src);
    void store_vm_reg(int vm_dst, Reg cpu_src);
    void emit_prologue();
    void emit_epilogue();
};

} // namespace meow::jit::x64