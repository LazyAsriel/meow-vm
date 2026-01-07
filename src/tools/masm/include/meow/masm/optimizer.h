#pragma once
#include "common.h"
#include <vector>
#include <map>
#include <string>
#include <print>
#include <meow_variant.h> 
#include <variant>

namespace meow::masm {

enum class OptFlags : uint8_t {
    None        = 0,
    DCE         = 1 << 0, 
    Peephole    = 1 << 1, 
    RegAlloc    = 1 << 2, 
    ConstFold   = 1 << 3, 
    
    O1 = DCE | Peephole,
    O2 = DCE | Peephole | RegAlloc | ConstFold,
    All = 0xFF
};

inline bool has_flag(OptFlags f, OptFlags check) {
    return (static_cast<uint8_t>(f) & static_cast<uint8_t>(check)) != 0;
}

struct OptConfig {
    OptFlags flags = OptFlags::O2;
    uint8_t byte_threshold = 10; 
};

using Unknown = meow::monostate;

using ConstantValue = meow::variant<meow::monostate, int64_t, double>;

class Optimizer {
public:
    Optimizer(Prototype& proto, const std::vector<Prototype>& all_protos, OptConfig config)
        : proto_(proto), all_protos_(all_protos), config_(config) {
            std::println("sizeof(ConstantValue) = {}", sizeof(ConstantValue));
        }

    void run();

    template <typename T> uint32_t add_constant(T v);
    uint32_t add_string(const std::string& s);

private:
    // --- Passes ---
    bool pass_dce();           
    bool pass_peephole();      
    bool pass_const_folding(); 
    bool pass_reg_alloc();     

    // --- Helpers ---
    void build_liveness();
    void rewrite_proto(); 

    struct Interval {
        uint16_t reg_id;
        int start;
        int end;
        int hint_reg = -1; 
        bool is_fixed = false; 
    };

    Prototype& proto_;
    const std::vector<Prototype>& all_protos_;
    OptConfig config_;

    std::vector<IrInstruction> ir_code_;
    
    std::vector<ConstantValue> const_pool_;
    std::vector<std::string> string_pool_;

    std::map<uint16_t, Interval> intervals_;
    std::vector<uint16_t> vreg_to_phys_;
};

void optimize_prototype(Prototype& proto, const std::vector<Prototype>& all_protos, OptConfig config = {});

} // namespace meow::masm