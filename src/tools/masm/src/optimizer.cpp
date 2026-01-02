/*
type: uploaded file
fileName: masm/src/optimizer.cpp
*/
#include <meow/masm/optimizer.h>
#include <meow/bytecode/op_codes.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <print>
#include <bit>

namespace meow::masm {

void optimize_prototype(Prototype& proto, const std::vector<Prototype>& all_protos, OptConfig config) {
    if (proto.ir_code.empty()) return;
    Optimizer opt(proto, all_protos, config);
    opt.run();
}

// --- HELPER: Constant Pool Deduplication ---
template <typename T>
uint32_t Optimizer::add_constant(T v) {
    for(size_t i=0; i<const_pool_.size(); ++i) {
        bool found = const_pool_[i].visit(
            [&](T existing) { return existing == v; },
            [](auto) { return false; }
        );
        if (found) return (uint32_t)i;
    }
    const_pool_.push_back(v);
    return (uint32_t)(const_pool_.size() - 1);
}

template uint32_t Optimizer::add_constant<int64_t>(int64_t);
template uint32_t Optimizer::add_constant<double>(double);

uint32_t Optimizer::add_string(const std::string& s) {
    auto it = std::find(string_pool_.begin(), string_pool_.end(), s);
    if (it != string_pool_.end()) return (uint32_t)std::distance(string_pool_.begin(), it);
    string_pool_.push_back(s);
    return (uint32_t)(string_pool_.size() - 1);
}

// --- MAIN PIPELINE ---

void Optimizer::run() {
    ir_code_ = std::move(proto_.ir_code);
    string_pool_ = std::move(proto_.string_pool);

    struct PassDesc {
        OptFlags required_flag;
        bool (Optimizer::*func)();
        bool loopable;
    };

    static constexpr std::array pipeline = {
        PassDesc{OptFlags::DCE,      &Optimizer::pass_dce,      true},
        PassDesc{OptFlags::Peephole, &Optimizer::pass_peephole, true},
        PassDesc{OptFlags::RegAlloc, &Optimizer::pass_reg_alloc, false}
    };

    bool changed = true;
    int loop_count = 0;
    const int MAX_LOOPS = 5;

    // Phase 1: Iterative Passes
    while (changed && loop_count < MAX_LOOPS) {
        changed = false;
        for (const auto& pass : pipeline) {
            if (pass.loopable && has_flag(config_.flags, pass.required_flag)) {
                if ((this->*pass.func)()) changed = true;
            }
        }
        loop_count++;
    }

    // Phase 2: Final Passes
    for (const auto& pass : pipeline) {
        if (!pass.loopable && has_flag(config_.flags, pass.required_flag)) {
            (this->*pass.func)();
        }
    }

    rewrite_proto();
}

// --- PASS: Dead Code Elimination ---
bool Optimizer::pass_dce() {
    bool changed = false;
    auto it = std::remove_if(ir_code_.begin(), ir_code_.end(), [&](const IrInstruction& inst) {
        if (inst.op == meow::OpCode::MOVE && inst.arg_count >= 2) {
            if (inst.args[0].is<Reg>() && inst.args[1].is<Reg>()) {
                if (inst.args[0].unsafe_get<Reg>().id == inst.args[1].unsafe_get<Reg>().id) {
                    changed = true;
                    return true; 
                }
            }
        }
        return false;
    });
    if (changed) ir_code_.erase(it, ir_code_.end());
    return changed;
}

// --- PASS: Peephole Optimization ---
bool Optimizer::pass_peephole() {
    bool changed = false;
    std::vector<IrInstruction> next_ir;
    next_ir.reserve(ir_code_.size());

    for (size_t i = 0; i < ir_code_.size(); ++i) {
        auto& inst = ir_code_[i];
        bool fused = false;

        // Fusion: CMP + JUMP_IF_TRUE -> JUMP_IF_CMP
        if (i + 1 < ir_code_.size()) {
            auto& next = ir_code_[i+1];
            if (next.op == meow::OpCode::JUMP_IF_TRUE && inst.arg_count >= 3 && next.arg_count >= 2) {
                 if (inst.args[0].is<Reg>() && next.args[0].is<Reg>() &&
                     inst.args[0].unsafe_get<Reg>().id == next.args[0].unsafe_get<Reg>().id) {

                    meow::OpCode new_op = meow::OpCode::NOP;
                    switch(inst.op) {
                        case meow::OpCode::EQ: new_op = meow::OpCode::JUMP_IF_EQ; break;
                        case meow::OpCode::NEQ: new_op = meow::OpCode::JUMP_IF_NEQ; break;
                        case meow::OpCode::LT: new_op = meow::OpCode::JUMP_IF_LT; break;
                        case meow::OpCode::LE: new_op = meow::OpCode::JUMP_IF_LE; break;
                        case meow::OpCode::GT: new_op = meow::OpCode::JUMP_IF_GT; break;
                        case meow::OpCode::GE: new_op = meow::OpCode::JUMP_IF_GE; break;
                        default: break;
                    }

                    if (new_op != meow::OpCode::NOP) {
                        IrInstruction f = inst;
                        f.op = new_op;
                        f.arg_count = 3; 
                        f.args[0] = inst.args[1]; 
                        f.args[1] = inst.args[2]; 
                        f.args[2] = next.args[1]; // Label
                        next_ir.push_back(f);
                        i++; 
                        fused = true;
                        changed = true;
                    }
                 }
            }
        }
        if (!fused) next_ir.push_back(inst);
    }
    if (changed) ir_code_ = std::move(next_ir);
    return changed;
}

// --- PASS: Register Allocation ---
bool Optimizer::pass_reg_alloc() {
    build_liveness();
    
    std::vector<uint16_t> sorted_vregs;
    uint16_t max_vreg = 0;
    for (auto& [r, iv] : intervals_) {
        sorted_vregs.push_back(r);
        if (r > max_vreg) max_vreg = r;
    }
    vreg_to_phys_.assign(max_vreg + 1, 0xFFFF); 

    std::sort(sorted_vregs.begin(), sorted_vregs.end(), [&](uint16_t a, uint16_t b) {
        return intervals_[a].start < intervals_[b].start;
    });

    std::vector<int> free_until(256, -1); 

    for (uint16_t vreg : sorted_vregs) {
        auto& iv = intervals_[vreg];
        if (iv.is_fixed) {
            vreg_to_phys_[vreg] = vreg; 
            if (vreg < 256) free_until[vreg] = std::max(free_until[vreg], iv.end);
            continue;
        }

        int phys = -1;
        // Hint -> Fast -> Spill
        if (iv.hint_reg != -1 && iv.hint_reg <= max_vreg && vreg_to_phys_[iv.hint_reg] != 0xFFFF) {
            int h = vreg_to_phys_[iv.hint_reg];
            if (h < config_.max_fast_regs && free_until[h] <= iv.start) phys = h;
        }
        if (phys == -1) {
            for (int i = 0; i < config_.max_fast_regs; ++i) {
                if (free_until[i] <= iv.start) { phys = i; break; }
            }
        }
        // [NOTE] Spill Logic: Nếu hết fast regs (ví dụ 10), ta sẽ tràn (spill) vào các register tiếp theo.
        // Tuy nhiên ở đây logic đơn giản hóa: spill tất cả vào slot cuối cùng của fast regs.
        // Cần lưu ý nếu muốn mở rộng ra > 10 regs nhưng vẫn < 256.
        if (phys == -1) {
             // Nếu cho phép spill ra ngoài fast area:
             // for (int i = config_.max_fast_regs; i < 255; ++i) ...
             // Nhưng ở đây ta giữ logic spill vào slot cuối để tiết kiệm register count.
             phys = config_.max_fast_regs; 
        }

        vreg_to_phys_[vreg] = (uint16_t)phys;
        free_until[phys] = iv.end;
    }
    return false;
}

void Optimizer::build_liveness() {
    intervals_.clear();
    int pc = 0;
    for (const auto& inst : ir_code_) {
        for (int k = 0; k < inst.arg_count; ++k) {
            inst.args[k].visit(
                [&](Reg r) {
                    if (intervals_.find(r.id) == intervals_.end()) intervals_[r.id] = {r.id, pc, pc};
                    else intervals_[r.id].end = pc;
                },
                [](auto) {}
            );
        }
        if (inst.op == meow::OpCode::MOVE && inst.arg_count >= 2 && 
            inst.args[0].is<Reg>() && inst.args[1].is<Reg>()) {
            intervals_[inst.args[0].unsafe_get<Reg>().id].hint_reg = inst.args[1].unsafe_get<Reg>().id;
        }
        if (inst.op == meow::OpCode::CLOSURE && inst.arg_count >= 2 && inst.args[1].is<int64_t>()) {
             size_t p_idx = (size_t)inst.args[1].unsafe_get<int64_t>();
             if (p_idx < all_protos_.size()) {
                 for(const auto& uv : all_protos_[p_idx].upvalues) {
                     if (uv.is_local && intervals_.count(uv.index)) {
                         intervals_[uv.index].is_fixed = true;
                     }
                 }
             }
        }
        pc++;
    }
}

// --- HELPER: Bytecode Optimization Logic ---
static bool is_byte_op(meow::OpCode op) {
    using enum meow::OpCode;
    return (op >= ADD_B && op <= RSHIFT_B) || 
           (op == JUMP_IF_TRUE_B || op == JUMP_IF_FALSE_B) ||
           (op == MOVE_B);
}

static meow::OpCode to_byte_op(meow::OpCode op) {
    using enum meow::OpCode;
    switch(op) {
        case ADD: return ADD_B;
        case SUB: return SUB_B;
        case MUL: return MUL_B;
        case DIV: return DIV_B;
        case MOD: return MOD_B;
        
        case EQ: return EQ_B;
        case NEQ: return NEQ_B;
        case GT: return GT_B;
        case GE: return GE_B;
        case LT: return LT_B;
        case LE: return LE_B;

        case BIT_AND: return BIT_AND_B;
        case BIT_OR: return BIT_OR_B;
        case BIT_XOR: return BIT_XOR_B;
        case LSHIFT: return LSHIFT_B;
        case RSHIFT: return RSHIFT_B;

        case MOVE: return MOVE_B;
        case JUMP_IF_TRUE: return JUMP_IF_TRUE_B;
        case JUMP_IF_FALSE: return JUMP_IF_FALSE_B;
        // LOAD_INT_B bỏ qua vì OpInfo chưa support
        default: return op;
    }
}

// --- CODEGEN: IR -> Bytecode ---
void Optimizer::rewrite_proto() {
    uint16_t max_phys = 0;
    
    // 1. Remap Registers
    for (auto& inst : ir_code_) {
        for (int k = 0; k < inst.arg_count; ++k) {
            inst.args[k].visit(
                [&](Reg& r) { 
                    if (r.id < vreg_to_phys_.size() && vreg_to_phys_[r.id] != 0xFFFF) {
                        r.id = vreg_to_phys_[r.id];
                        max_phys = std::max(max_phys, r.id);
                    }
                },
                [](auto&) {}
            );
        }
    }
    proto_.num_regs = max_phys + 1;

    // [OPTIMIZATION] Chỉ dùng Byte OpCode nếu số lượng thanh ghi nằm trong vùng "Fast"
    // (ví dụ <= 10 regs như bạn yêu cầu). Nếu spill ra ngoài thì dùng bản chuẩn (u16).
    bool use_byte_ops = (proto_.num_regs <= config_.max_fast_regs); 

    // 2. Emit Bytecode
    proto_.bytecode.clear();
    
    auto emit_byte = [&](uint8_t b) { proto_.bytecode.push_back(b); };
    auto emit_u16  = [&](uint16_t v) { emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); };
    auto emit_u64  = [&](uint64_t v) { for(int i=0; i<8; ++i) emit_byte((v>>(i*8))&0xFF); };

    struct Patch { size_t pos; uint32_t label_id; };
    std::vector<Patch> patches;
    std::map<uint32_t, size_t> label_locs;

    for (auto& inst : ir_code_) {
        if (inst.op == meow::OpCode::NOP && inst.arg_count > 0 && inst.args[0].is<LabelIdx>()) {
            label_locs[inst.args[0].unsafe_get<LabelIdx>().id] = proto_.bytecode.size();
            continue;
        }
        if (inst.op == meow::OpCode::NOP) continue;

        // Cleanup Redundant Move
        if (inst.op == meow::OpCode::MOVE && inst.arg_count >= 2) {
             if (inst.args[0].is<Reg>() && inst.args[1].is<Reg>()) {
                 if (inst.args[0].unsafe_get<Reg>().id == inst.args[1].unsafe_get<Reg>().id) {
                     continue; 
                 }
             }
        }

        // [OPTIMIZATION] Switch to Byte OpCode
        if (use_byte_ops) {
            inst.op = to_byte_op(inst.op);
        }

        emit_byte(static_cast<uint8_t>(inst.op));
        
        int written_bytes = 0;
        // Kiểm tra xem OpCode hiện tại có phải loại Byte không để emit register 1 byte
        bool is_byte_mode = is_byte_op(inst.op);

        for(int k=0; k < inst.arg_count; ++k) {
            inst.args[k].visit(
                [&](Reg r) { 
                    if (is_byte_mode) {
                        emit_byte((uint8_t)r.id); 
                        written_bytes += 1;
                    } else {
                        emit_u16(r.id); 
                        written_bytes += 2;
                    }
                },
                [&](int64_t v) { emit_u64((uint64_t)v); written_bytes += 8; },
                [&](double v)  { emit_u64(std::bit_cast<uint64_t>(v)); written_bytes += 8; },
                [&](ConstIdx c) {
                    const_pool_[c.id].visit(
                        [&](int64_t i) { emit_u64((uint64_t)i); written_bytes += 8; },
                        [&](double d)  { emit_u64(std::bit_cast<uint64_t>(d)); written_bytes += 8; }
                    );
                },
                [&](StrIdx s)   { emit_u16((uint16_t)s.id); written_bytes += 2; },
                [&](LabelIdx l) { 
                    patches.push_back({proto_.bytecode.size(), l.id});
                    emit_u16(0xFFFF); 
                    written_bytes += 2;
                },
                [&](JumpOffset o){ emit_u16((uint16_t)o.val); written_bytes += 2; },
                [](auto){}
            );
        }

        // Emit Padding (Inline Cache support)
        auto info = meow::get_op_info(inst.op);
        int padding = info.operand_bytes - written_bytes;
        if (padding > 0) {
            for(int i = 0; i < padding; ++i) emit_byte(0);
        }
    }

    // 3. Patch Jumps
    for(auto& p : patches) {
        if (label_locs.count(p.label_id)) {
            size_t target = label_locs[p.label_id];
            proto_.bytecode[p.pos] = target & 0xFF;
            proto_.bytecode[p.pos+1] = (target >> 8) & 0xFF;
        }
    }

    proto_.string_pool = std::move(string_pool_);
    proto_.ir_code.clear();
}

} // namespace meow::masm