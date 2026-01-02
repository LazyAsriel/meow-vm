#pragma once
#include "common.h"
#include <vector>
#include <string>
#include <map>
#include <array>
#include <bit>

namespace meow::masm {

// --- 1. Bitwise Configuration ---
enum class OptFlags : uint32_t {
    None         = 0,
    DCE          = 1 << 0, // Dead Code Elimination
    Peephole     = 1 << 1, // Instruction Fusion
    RegAlloc     = 1 << 2, // Register Allocation
    BlockReorder = 1 << 3, // Future: Basic Block Reordering
    
    // Presets
    O0           = None,
    O1           = DCE | Peephole,
    O2           = DCE | Peephole | RegAlloc,
    All          = 0xFFFFFFFF
};

[[nodiscard]] constexpr OptFlags operator|(OptFlags a, OptFlags b) { return (OptFlags)((uint32_t)a | (uint32_t)b); }
[[nodiscard]] constexpr OptFlags operator&(OptFlags a, OptFlags b) { return (OptFlags)((uint32_t)a & (uint32_t)b); }
[[nodiscard]] constexpr bool has_flag(OptFlags mask, OptFlags flag) { return (mask & flag) == flag; }

struct OptConfig {
    OptFlags flags = OptFlags::O2;
    uint8_t max_fast_regs = 10;
};

// --- 2. Helper Structs for Logic ---

struct LiveInterval {
    uint16_t v_reg;
    int start = -1, end = -1, hint_reg = -1;
    bool is_fixed = false;
};

// Internal Constant Pool của Optimizer (dùng Variant)
using ConstantValue = meow::variant<int64_t, double>;

// --- 3. Optimizer Class ---

class Optimizer {
public:
    Optimizer(Prototype& proto, const std::vector<Prototype>& all_protos, OptConfig config)
        : proto_(proto), all_protos_(all_protos), config_(config) {}

    // Hàm chạy chính
    void run();

    // API để Assembler đẩy dữ liệu vào Pool (Deduplication)
    uint32_t add_string(const std::string& s);
    
    // Template để add số (int/double) vào const_pool
    template <typename T> uint32_t add_constant(T v);

    // Wrapper để push lệnh vào Prototype IR
    void push_inst(const IrInstruction& inst) { proto_.ir_code.push_back(inst); }

private:
    Prototype& proto_;
    const std::vector<Prototype>& all_protos_;
    OptConfig config_;

    // Local context (Copy/Move từ Prototype sang để xử lý cho nhanh)
    std::vector<IrInstruction> ir_code_;
    std::vector<std::string> string_pool_;
    std::vector<ConstantValue> const_pool_; 

    // RegAlloc State
    std::map<uint16_t, LiveInterval> intervals_;
    std::vector<uint16_t> vreg_to_phys_;

    // --- Optimization Passes ---
    // Trả về true nếu có thay đổi IR (để chạy lại loop)
    bool pass_dce();
    bool pass_peephole();
    bool pass_reg_alloc(); // RegAlloc thường chạy 1 lần cuối, return false

    // Helpers
    void build_liveness();
    void rewrite_proto(); // CodeGen: IR -> Bytecode
};

// Helper function global
void optimize_prototype(Prototype& proto, const std::vector<Prototype>& all_protos, OptConfig config = {});

} // namespace meow::masm