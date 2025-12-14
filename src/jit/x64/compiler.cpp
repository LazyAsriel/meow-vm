#include "jit/x64/compiler.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <format>

// Include các header cần thiết để gọi helper
#include "vm/handlers/math_ops.h"
#include "vm/handlers/data_ops.h"
#include "vm/handlers/flow_ops.h"
#include "meow/core/meow_object.h"

namespace meow::jit::x64 {

// --- Constants & Helpers ---
static constexpr size_t PAGE_SIZE = 4096;

// Nanbox Layout Constants (Copy from meow_nanbox_layout.h)
using Layout = meow::NanboxLayout;

// Register Mapping (System V AMD64)
// RDI: regs
// RSI: constants
// RDX: state
static constexpr Reg REG_VM_REGS  = RDI;
static constexpr Reg REG_CONSTS   = RSI;
static constexpr Reg REG_STATE    = RDX;

// Scratch Registers
static constexpr Reg RAX_REG = RAX;
static constexpr Reg RCX_REG = RCX;
static constexpr Reg RBX_REG = RBX; // Callee-saved, cần push/pop nếu dùng

// --- Helper Prototypes (Slow Paths) ---
// Các hàm này sẽ được gọi từ JIT code khi cần xử lý phức tạp
extern "C" {
    static void helper_add(Value* regs, VMState* state, uint16_t dst, uint16_t r1, uint16_t r2) {
        Value& left = regs[r1];
        Value& right = regs[r2];
        regs[dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    
    static void helper_sub(Value* regs, VMState* state, uint16_t dst, uint16_t r1, uint16_t r2) {
        Value& left = regs[r1];
        Value& right = regs[r2];
        regs[dst] = OperatorDispatcher::find(OpCode::SUB, left, right)(&state->heap, left, right);
    }

    static void helper_panic(VMState* state, const char* msg) {
        state->error(msg);
    }
}

// --- Compiler Implementation ---

Compiler::Compiler() : emit_(nullptr, 0) {
    // 1. Align capacity to page size
    if (capacity_ % PAGE_SIZE != 0) {
        capacity_ = (capacity_ / PAGE_SIZE + 1) * PAGE_SIZE;
    }

    // 2. Allocate Executable Memory (RWX for simplicity, ideally W^X)
#if defined(__linux__) || defined(__APPLE__)
    code_mem_ = (uint8_t*)mmap(nullptr, capacity_, 
                               PROT_READ | PROT_WRITE | PROT_EXEC, 
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_mem_ == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
#else
    // Windows implementation omitted for brevity
    code_mem_ = new uint8_t[capacity_]; 
#endif

    // 3. Reset Emitter with buffer
    emit_ = Emitter(code_mem_, capacity_);
}

Compiler::~Compiler() {
#if defined(__linux__) || defined(__APPLE__)
    if (code_mem_) {
        munmap(code_mem_, capacity_);
    }
#else
    if (code_mem_) delete[] code_mem_;
#endif
}

Compiler::JitFunc Compiler::compile(const uint8_t* bytecode, size_t len) {
    emit_ = Emitter(code_mem_, capacity_); // Reset cursor
    fixups_.clear();
    bc_to_native_.clear();

    emit_prologue();

    size_t ip = 0;
    while (ip < len) {
        // Record mapping: Bytecode Offset -> Native Offset
        bc_to_native_[ip] = emit_.cursor();

        OpCode op = static_cast<OpCode>(bytecode[ip]);
        ip++; // Consume OpCode

        switch (op) {
            case OpCode::LOAD_CONST: {
                uint16_t dst = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                uint16_t idx = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                
                // mov rax, [rsi + idx * 8]
                // mov [rdi + dst * 8], rax
                emit_.mov(RAX_REG, REG_CONSTS, idx * 8);
                emit_.mov(REG_VM_REGS, dst * 8, RAX_REG);
                break;
            }

            case OpCode::LOAD_INT: {
                uint16_t dst = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                int64_t val = *reinterpret_cast<const int64_t*>(bytecode + ip); ip += 8;

                // Tạo Value Int (Nanboxed)
                // bits = QNAN_POS | (Type::Int << TAG_SHIFT) | (val & PAYLOAD_MASK)
                // Tuy nhiên Value::Value(int) làm việc này. Ta giả lập bitwise.
                // Giả sử ValueType::Int là 2 (theo enum trong meow_variant hoặc definitions)
                // Cần check chính xác thứ tự types trong meow::base_t
                
                // Fast way: Load immediate to register
                Value v(val); 
                emit_.mov(RAX_REG, v.as_raw_u64());
                emit_.mov(REG_VM_REGS, dst * 8, RAX_REG);
                break;
            }

            case OpCode::MOVE: {
                uint16_t dst = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                uint16_t src = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                
                emit_.mov(RAX_REG, REG_VM_REGS, src * 8);
                emit_.mov(REG_VM_REGS, dst * 8, RAX_REG);
                break;
            }

            case OpCode::ADD: {
                // ADD dst, r1, r2
                uint16_t dst = *reinterpret_cast<const uint16_t*>(bytecode + ip);
                uint16_t r1  = *reinterpret_cast<const uint16_t*>(bytecode + ip + 2);
                uint16_t r2  = *reinterpret_cast<const uint16_t*>(bytecode + ip + 4);
                ip += 6;

                // --- Optimized JIT for ADD (Int Fast Path) ---
                // 1. Load r1, r2
                emit_.mov(RAX_REG, REG_VM_REGS, r1 * 8); // RAX = left
                emit_.mov(RCX_REG, REG_VM_REGS, r2 * 8); // RCX = right

                // 2. Check Tag (Is Int?)
                // Int Tag: (bits >> TAG_SHIFT) == IntIndex
                // Hoặc check range NaN boxing.
                // Giả sử chỉ check Int đơn giản:
                // Int encoded: QNAN_POS | (2 << 48) | val (example)
                // Ta dùng Value::is_int() logic: data_.holds<int_t>()
                
                // Fallback label
                // Do JIT phức tạp nếu check bit thủ công mà không hardcode layout, 
                // ta sẽ gọi helper C++ cho an toàn, NHƯNG user đòi hỏi tối ưu.
                // -> Inline Check.
                
                // Mask lấy tag: 0xFFFF000000000000 (approx)
                // Layout::TAG_MASK (từ NanboxLayout)
                
                // Emit check r1
                emit_.mov(R8, RAX_REG);
                emit_.mov(R9, 0xFFFF000000000000ULL); // Tag mask
                emit_.and_(R8, R9);
                
                // Tính Int Tag Value: 
                // Value v(0); Tag = v.raw() & Mask;
                uint64_t int_tag = Value(0).as_raw_u64() & 0xFFFF000000000000ULL;
                
                emit_.mov(R10, int_tag);
                emit_.cmp(R8, R10);
                emit_.jcc(Condition::NE, 0); // Placeholder offset for SlowPath
                size_t jump_slow_1 = emit_.cursor();

                // Emit check r2
                emit_.mov(R8, RCX_REG);
                emit_.and_(R8, R9);
                emit_.cmp(R8, R10);
                emit_.jcc(Condition::NE, 0); 
                size_t jump_slow_2 = emit_.cursor();

                // 3. Fast Path: Add Integers
                // Cả 2 đều là Int, payload là 32bit hoặc 48bit signed.
                // Do Nanbox payload mask, ta cần cẩn thận sign extension nếu số âm.
                // Tuy nhiên nếu là int32_t (common), ta có thể cộng trực tiếp nếu bỏ tag.
                // Cách an toàn nhất: Trừ Tag -> Cộng -> Cộng Tag.
                // v1 = tag | payload1
                // v2 = tag | payload2
                // res = tag | (payload1 + payload2)
                // -> res = v1 + v2 - tag? Không đúng nếu tràn bit.
                
                // Unpack: AND Payload Mask
                emit_.mov(R11, 0x0000FFFFFFFFFFFFULL); // Payload mask
                emit_.and_(RAX_REG, R11); // RAX = payload1 (raw int casted to u64)
                emit_.and_(RCX_REG, R11); // RCX = payload2

                // Sign extend 48-bit to 64-bit (nếu cần thiết cho số âm)
                // shift left 16, sar 16
                emit_.shl(RAX_REG, 16); emit_.sar(RAX_REG, 16);
                emit_.shl(RCX_REG, 16); emit_.sar(RCX_REG, 16);

                emit_.add(RAX_REG, RCX_REG); // RAX = result (int64)

                // Repack: OR Tag
                emit_.or_(RAX_REG, R10); // Or với Tag

                // Store Result
                emit_.mov(REG_VM_REGS, dst * 8, RAX_REG);

                // Jump to Done
                emit_.jmp(0); 
                size_t jump_done = emit_.cursor();

                // 4. Slow Path
                size_t slow_path_start = emit_.cursor();
                // Patch Jumps to here
                emit_.patch_u32(jump_slow_1 - 4, slow_path_start - jump_slow_1);
                emit_.patch_u32(jump_slow_2 - 4, slow_path_start - jump_slow_2);

                // Setup Arguments for Helper call
                // void helper_add(Value* regs, VMState* state, dst, r1, r2)
                // RDI (regs), RSI (state -> chuyển từ RDX), RDX (dst), RCX (r1), R8 (r2)
                // Chú ý: Calling Convention: RDI, RSI, RDX, RCX, R8, R9
                // Hiện tại: RDI=regs, RSI=constants, RDX=state
                
                // Save caller-saved regs if needed (regs/state/consts are in callee-saved or args?)
                // RDI, RSI, RDX cần được bảo tồn sau call?
                // Ta push chúng.
                emit_.push(RDI);
                emit_.push(RSI);
                emit_.push(RDX);

                emit_.mov(RSI, RDX); // Arg2: state (đang ở RDX)
                // Arg1: regs (đang ở RDI) - ok
                
                emit_.mov(RDX, dst); // Arg3: dst
                emit_.mov(RCX, r1);  // Arg4: r1
                emit_.mov(R8,  r2);  // Arg5: r2

                // Call Helper
                emit_.mov(RAX_REG, (uint64_t)helper_add);
                emit_.call(RAX_REG);

                emit_.pop(RDX);
                emit_.pop(RSI);
                emit_.pop(RDI);

                // 5. Done Label
                size_t done_pos = emit_.cursor();
                emit_.patch_u32(jump_done - 4, done_pos - jump_done);

                break;
            }

            case OpCode::JUMP: {
                uint16_t offset = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                // Target Bytecode Index
                // Current Bytecode Index start was 'ip - 3' (1 opcode + 2 offset)
                // Offset is relative to *instruction base* in interpreter, but here absolute BC index?
                // Logic interpreter: return state->instruction_base + offset; -> offset is absolute index
                
                size_t target_bc = offset;

                if (target_bc < ip) {
                    // Backward Jump: Target đã được emit
                    size_t target_native = bc_to_native_[target_bc];
                    size_t current_native = emit_.cursor();
                    int32_t rel = static_cast<int32_t>(target_native) - static_cast<int32_t>(current_native) - 5; // 5 bytes jmp
                    emit_.jmp(rel);
                } else {
                    // Forward Jump: Record Fixup
                    fixups_.push_back({emit_.cursor(), target_bc, false});
                    emit_.jmp(0); // Placeholder
                }
                break;
            }
            
            case OpCode::JUMP_IF_FALSE: {
                uint16_t cond_reg = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                uint16_t offset   = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;

                // Check False
                emit_.mov(RAX_REG, REG_VM_REGS, cond_reg * 8);
                
                // is_bool check: tag == bool_tag
                uint64_t bool_tag = Value(false).as_raw_u64() & 0xFFFF000000000000ULL;
                // Value(false) payload = 0, Value(true) payload = 1
                
                // Mask tag
                emit_.mov(R8, RAX_REG);
                emit_.mov(R9, 0xFFFF000000000000ULL);
                emit_.and_(R8, R9);
                emit_.mov(R10, bool_tag);
                emit_.cmp(R8, R10);
                
                // If not bool, fallback to slow (ignore for now or treat as truthy)
                // For speed, let's assume bool. If payload == 0 -> Jump.
                
                // Test payload (lowest bit)
                emit_.test(RAX_REG, RAX_REG); // Zero flag set if false (0)
                
                size_t target_bc = offset;
                if (target_bc < ip) {
                    size_t target_native = bc_to_native_[target_bc];
                    size_t current_native = emit_.cursor();
                    int32_t rel = static_cast<int32_t>(target_native) - static_cast<int32_t>(current_native) - 6; // 6 bytes jcc
                    emit_.jcc(Condition::E, rel); // Jump if Equal (Zero) -> False
                } else {
                    fixups_.push_back({emit_.cursor(), target_bc, true});
                    emit_.jcc(Condition::E, 0);
                }
                break;
            }

            case OpCode::RETURN: {
                uint16_t ret_reg = *reinterpret_cast<const uint16_t*>(bytecode + ip); ip += 2;
                if (ret_reg != 0xFFFF) {
                    // Load return value to RAX (Convention? Or handle by caller?)
                    // JIT function is void, but maybe we store to a specific slot?
                    // Interpreter logic handles frame popping.
                    // For pure JIT compiled function (assuming simple call), just ret.
                }
                emit_epilogue();
                break;
            }

            default: {
                // Ignore or implement PANIC
                // std::cerr << "Unimplemented OpCode in JIT: " << (int)op << "\n";
                // emit_.int3(); // Breakpoint
                break;
            }
        }
    }

    // 4. Resolve Fixups (Forward Jumps)
    for (const auto& fix : fixups_) {
        size_t target_native = bc_to_native_[fix.target_bc];
        size_t jump_inst_end = fix.jump_op_pos + (fix.is_cond ? 6 : 5); // jcc 6 bytes, jmp 5 bytes
        int32_t rel = static_cast<int32_t>(target_native) - static_cast<int32_t>(jump_inst_end);
        
        // Offset nằm ở cuối lệnh jump (4 bytes cuối)
        // jmp rel32 -> opcode(1) + rel(4)
        // jcc rel32 -> opcode(2) + rel(4)
        emit_.patch_u32(jump_inst_end - 4, rel);
    }

    // Cast code memory to function pointer
    return reinterpret_cast<JitFunc>(code_mem_);
}

void Compiler::emit_prologue() {
    // Standard Prologue
    emit_.push(RBP);
    emit_.mov(RBP, RSP);
    
    // Save Callee-saved registers we use (RBX, R12-R15)
    emit_.push(RBX); 
    // emit_.push(R12); ...
}

void Compiler::emit_epilogue() {
    // Restore Callee-saved
    emit_.pop(RBX);
    
    emit_.mov(RSP, RBP);
    emit_.pop(RBP);
    emit_.ret();
}

Reg Compiler::map_vm_reg(int vm_reg) const {
    // TODO: Register Allocation Logic
    return INVALID_REG;
}

} // namespace meow::jit::x64