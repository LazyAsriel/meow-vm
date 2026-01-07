#include <meow/masm/optimizer.h>
#include <meow/bytecode/op_codes.h>
#include <iostream>
#include <algorithm>
#include <print>
#include <bit>
#include <set>
#include <cmath> 

namespace meow::masm {

void optimize_prototype(Prototype& proto, const std::vector<Prototype>& all_protos, OptConfig config) {
    if (proto.ir_code.empty()) return;
    Optimizer opt(proto, all_protos, config);
    opt.run();
}

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

// --- HELPER: Fast Constant Evaluation ---

static ConstantValue get_val(
    const IrArg& arg, 
    const std::map<uint16_t, ConstantValue>& known_regs,
    const std::vector<ConstantValue>& const_pool
) {
    if (arg.is<Reg>()) {
        uint16_t rid = arg.unsafe_get<Reg>().id;
        auto it = known_regs.find(rid);
        if (it != known_regs.end()) return it->second;
        return Unknown{};
    }
    if (arg.is<int64_t>()) return arg.unsafe_get<int64_t>();
    if (arg.is<double>()) return arg.unsafe_get<double>();
    if (arg.is<ConstIdx>()) {
        size_t idx = arg.unsafe_get<ConstIdx>().id;
        if (idx < const_pool.size()) return const_pool[idx];
    }
    return Unknown{};
}

static ConstantValue evaluate_binary(OpCode op, const ConstantValue& lhs, const ConstantValue& rhs) {
    if (lhs.is<Unknown>() || rhs.is<Unknown>()) return Unknown{};

    return lhs.visit(
        [&](int64_t l) -> ConstantValue {
            return rhs.visit(
                [&](int64_t r) -> ConstantValue { 
                    switch(op) {
                        case OpCode::ADD: return l + r;
                        case OpCode::SUB: return l - r;
                        case OpCode::MUL: return l * r;
                        case OpCode::DIV: return (r == 0) ? ConstantValue(Unknown{}) : (l / r);
                        case OpCode::MOD: return (r == 0) ? ConstantValue(Unknown{}) : (l % r);
                        case OpCode::BIT_AND: return l & r;
                        case OpCode::BIT_OR:  return l | r;
                        case OpCode::BIT_XOR: return l ^ r;
                        case OpCode::LSHIFT:  return l << r;
                        case OpCode::RSHIFT:  return l >> r;
                        case OpCode::EQ: return (int64_t)(l == r);
                        case OpCode::NEQ: return (int64_t)(l != r);
                        case OpCode::GT: return (int64_t)(l > r);
                        case OpCode::GE: return (int64_t)(l >= r);
                        case OpCode::LT: return (int64_t)(l < r);
                        case OpCode::LE: return (int64_t)(l <= r);
                        default: return Unknown{};
                    }
                },
                [&](double r) -> ConstantValue { 
                    double ld = static_cast<double>(l);
                    switch(op) {
                        case OpCode::ADD: return ld + r;
                        case OpCode::SUB: return ld - r;
                        case OpCode::MUL: return ld * r;
                        case OpCode::DIV: return ld / r;
                        case OpCode::EQ: return (int64_t)(ld == r);
                        case OpCode::NEQ: return (int64_t)(ld != r);
                        case OpCode::LT: return (int64_t)(ld < r);
                        case OpCode::LE: return (int64_t)(ld <= r);
                        case OpCode::GT: return (int64_t)(ld > r);
                        case OpCode::GE: return (int64_t)(ld >= r);
                        default: return Unknown{};
                    }
                },
                [](auto) -> ConstantValue { return Unknown{}; }
            );
        },
        [&](double l) -> ConstantValue {
             return rhs.visit(
                [&](int64_t r) -> ConstantValue { 
                    double rd = static_cast<double>(r);
                    switch(op) {
                        case OpCode::ADD: return l + rd;
                        case OpCode::SUB: return l - rd;
                        case OpCode::MUL: return l * rd;
                        case OpCode::DIV: return l / rd;
                        case OpCode::EQ: return (int64_t)(l == rd);
                        case OpCode::NEQ: return (int64_t)(l != rd);
                        case OpCode::LT: return (int64_t)(l < rd);
                        case OpCode::LE: return (int64_t)(l <= rd);
                        case OpCode::GT: return (int64_t)(l > rd);
                        case OpCode::GE: return (int64_t)(l >= rd);
                        default: return Unknown{};
                    }
                },
                [&](double r) -> ConstantValue { 
                     switch(op) {
                        case OpCode::ADD: return l + r;
                        case OpCode::SUB: return l - r;
                        case OpCode::MUL: return l * r;
                        case OpCode::DIV: return l / r;
                        case OpCode::EQ: return (int64_t)(l == r);
                        case OpCode::NEQ: return (int64_t)(l != r);
                        case OpCode::LT: return (int64_t)(l < r);
                        case OpCode::LE: return (int64_t)(l <= r);
                        case OpCode::GT: return (int64_t)(l > r);
                        case OpCode::GE: return (int64_t)(l >= r);
                        default: return Unknown{};
                    }
                },
                [](auto) -> ConstantValue { return Unknown{}; }
            );
        },
        [](auto) -> ConstantValue { return Unknown{}; }
    );
}

// --- MAIN PIPELINE ---

void Optimizer::run() {
    ir_code_ = std::move(proto_.ir_code);
    string_pool_ = std::move(proto_.string_pool);

    for(const auto& c : proto_.constants) {
        if(c.type == ConstType::INT_T) add_constant(c.val_i64);
        else if(c.type == ConstType::FLOAT_T) add_constant(c.val_f64);
    }

    struct PassDesc {
        OptFlags required_flag;
        bool (Optimizer::*func)();
        bool loopable;
    };

    static constexpr std::array pipeline = {
        PassDesc{OptFlags::DCE,       &Optimizer::pass_dce,       true},
        PassDesc{OptFlags::ConstFold, &Optimizer::pass_const_folding, true}, 
        PassDesc{OptFlags::Peephole,  &Optimizer::pass_peephole,  true},
        PassDesc{OptFlags::RegAlloc,  &Optimizer::pass_reg_alloc, false} 
    };

    bool changed = true;
    int loop_count = 0;
    const int MAX_LOOPS = 5; 

    while (changed && loop_count < MAX_LOOPS) {
        changed = false;
        for (const auto& pass : pipeline) {
            if (pass.loopable && has_flag(config_.flags, pass.required_flag)) {
                if ((this->*pass.func)()) changed = true;
            }
        }
        loop_count++;
    }

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

// --- PASS: Constant Folding (Optimized) ---
bool Optimizer::pass_const_folding() {
    bool changed = false;
    std::map<uint16_t, ConstantValue> known_values;

    for (auto& inst : ir_code_) {
        // 1. INVALIDATION
        if (inst.op == OpCode::NOP && inst.arg_count > 0 && inst.args[0].is<LabelIdx>()) {
            known_values.clear();
            continue;
        }

        // 2. CAPTURE DEFINITIONS
        if (inst.op == OpCode::LOAD_INT && inst.arg_count == 2) {
            uint16_t dst = inst.args[0].unsafe_get<Reg>().id;
            known_values[dst] = inst.args[1].unsafe_get<int64_t>();
            continue;
        }
        if (inst.op == OpCode::LOAD_FLOAT && inst.arg_count == 2) {
            uint16_t dst = inst.args[0].unsafe_get<Reg>().id;
            known_values[dst] = inst.args[1].unsafe_get<double>();
            continue;
        }
        if (inst.op == OpCode::LOAD_CONST && inst.arg_count == 2) {
             uint16_t dst = inst.args[0].unsafe_get<Reg>().id;
             size_t idx = inst.args[1].unsafe_get<ConstIdx>().id;
             if (idx < const_pool_.size()) {
                 known_values[dst] = const_pool_[idx];
             }
             continue;
        }

        // 3. FOLDING
        if (inst.arg_count == 3 && inst.args[0].is<Reg>()) {
            ConstantValue lhs = get_val(inst.args[1], known_values, const_pool_);
            if (lhs.is<Unknown>()) {
                uint16_t dst = inst.args[0].unsafe_get<Reg>().id;
                known_values.erase(dst);
                continue;
            }

            ConstantValue rhs = get_val(inst.args[2], known_values, const_pool_);
            if (rhs.is<Unknown>()) {
                uint16_t dst = inst.args[0].unsafe_get<Reg>().id;
                known_values.erase(dst);
                continue;
            }

            // Tính toán trực tiếp
            ConstantValue result = evaluate_binary(inst.op, lhs, rhs);
            
            if (!result.is<Unknown>()) {
                uint16_t dst_reg = inst.args[0].unsafe_get<Reg>().id;
                
                result.visit(
                    [&](int64_t i) {
                        inst.op = OpCode::LOAD_INT;
                        inst.arg_count = 2;
                        inst.args[0] = Reg{dst_reg};
                        inst.args[1] = i; 
                        known_values[dst_reg] = i; 
                    },
                    [&](double d) {
                        inst.op = OpCode::LOAD_FLOAT;
                        inst.arg_count = 2;
                        inst.args[0] = Reg{dst_reg};
                        inst.args[1] = d; 
                        known_values[dst_reg] = d;
                    },
                    [](auto){}
                );
                
                changed = true;
                continue; 
            }
        }

        // 4. CLEANUP
        if (inst.arg_count > 0 && inst.args[0].is<Reg>()) {
            uint16_t dst = inst.args[0].unsafe_get<Reg>().id;
            known_values.erase(dst);
        }
    }
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

        if (i + 1 < ir_code_.size()) {
            auto& next = ir_code_[i+1];
            if (next.op == meow::OpCode::JUMP_IF_TRUE && inst.arg_count >= 3 && next.arg_count >= 2) {
                 if (inst.args[0].is<Reg>() && next.args[0].is<Reg>() &&
                     inst.args[0].unsafe_get<Reg>().id == next.args[0].unsafe_get<Reg>().id) {

                    meow::OpCode new_op = meow::OpCode::NOP;
                    using enum meow::OpCode;
                    switch(inst.op) {
                        case EQ: new_op = JUMP_IF_EQ; break;
                        case NEQ: new_op = JUMP_IF_NEQ; break;
                        case LT: new_op = JUMP_IF_LT; break;
                        case LE: new_op = JUMP_IF_LE; break;
                        case GT: new_op = JUMP_IF_GT; break;
                        case GE: new_op = JUMP_IF_GE; break;
                        default: break;
                    }

                    if (new_op != NOP) {
                        IrInstruction f = inst;
                        f.op = new_op;
                        f.arg_count = 3; 
                        f.args[0] = inst.args[1]; 
                        f.args[1] = inst.args[2]; 
                        f.args[2] = next.args[1]; 
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
    uint16_t max_vreg_id = 0;
    for (auto& [r, iv] : intervals_) {
        sorted_vregs.push_back(r);
        if (r > max_vreg_id) max_vreg_id = r;
    }
    
    vreg_to_phys_.assign(max_vreg_id + 1, 0xFFFF); 

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

        // Ưu tiên 1: Hint Reg (byte_threshold)
        if (iv.hint_reg != -1 && iv.hint_reg <= max_vreg_id && vreg_to_phys_[iv.hint_reg] != 0xFFFF) {
            int h = vreg_to_phys_[iv.hint_reg];
            if (h < config_.byte_threshold && free_until[h] <= iv.start) {
                phys = h;
            }
        }

        // Ưu tiên 2: Byte Regs
        if (phys == -1) {
            for (int i = 0; i < config_.byte_threshold; ++i) {
                if (free_until[i] <= iv.start) { phys = i; break; }
            }
        }
        
        // Ưu tiên 3: Spill Area
        if (phys == -1) {
             for (int i = config_.byte_threshold; i < 256; ++i) {
                if (free_until[i] <= iv.start) { phys = i; break; }
             }
        }

        if (phys == -1) phys = 0; // Fallback unsafe

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

static bool is_byte_op(meow::OpCode op) {
    using enum meow::OpCode;
    return (op >= ADD_B && op <= RSHIFT_B) || 
           (op == JUMP_IF_TRUE_B || op == JUMP_IF_FALSE_B) ||
           (op == MOVE_B);
}

static meow::OpCode to_byte_op(meow::OpCode op) {
    using enum meow::OpCode;
    switch(op) {
        case ADD: return ADD_B; case SUB: return SUB_B;
        case MUL: return MUL_B; case DIV: return DIV_B; case MOD: return MOD_B;
        case EQ: return EQ_B; case NEQ: return NEQ_B;
        case GT: return GT_B; case GE: return GE_B;
        case LT: return LT_B; case LE: return LE_B;
        case BIT_AND: return BIT_AND_B; case BIT_OR: return BIT_OR_B;
        case BIT_XOR: return BIT_XOR_B; 
        case LSHIFT: return LSHIFT_B; case RSHIFT: return RSHIFT_B;
        case MOVE: return MOVE_B;
        case JUMP_IF_TRUE: return JUMP_IF_TRUE_B;
        case JUMP_IF_FALSE: return JUMP_IF_FALSE_B;
        case LOAD_CONST: return LOAD_CONST_B;
        default: return op;
    }
}

void Optimizer::rewrite_proto() {
    uint16_t max_phys = 0;
    
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

    bool use_byte_ops = (proto_.num_regs <= config_.byte_threshold); 

    proto_.constants.clear();
    for (const auto& val : const_pool_) {
        Constant c;
        val.visit(
            [&](int64_t i) { c.type = ConstType::INT_T; c.val_i64 = i; },
            [&](double d)  { c.type = ConstType::FLOAT_T; c.val_f64 = d; },
            [](auto) {} // Bỏ qua Unknown (thực tế const_pool ko nên chứa Unknown)
        );
        proto_.constants.push_back(c);
    }
    proto_.string_pool = std::move(string_pool_);

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

        if (inst.op == meow::OpCode::MOVE && inst.arg_count >= 2) {
             if (inst.args[0].is<Reg>() && inst.args[1].is<Reg>()) {
                 if (inst.args[0].unsafe_get<Reg>().id == inst.args[1].unsafe_get<Reg>().id) {
                     continue; 
                 }
             }
        }

        if (use_byte_ops) inst.op = to_byte_op(inst.op);

        emit_byte(static_cast<uint8_t>(inst.op));
        
        int written_bytes = 0;
        bool is_byte_mode = is_byte_op(inst.op);

        for(int k=0; k < inst.arg_count; ++k) {
            inst.args[k].visit(
                [&](Reg r) { 
                    if (is_byte_mode) { emit_byte((uint8_t)r.id); written_bytes += 1; } 
                    else { emit_u16(r.id); written_bytes += 2; }
                },
                [&](int64_t v) { emit_u64((uint64_t)v); written_bytes += 8; },
                [&](double v)  { emit_u64(std::bit_cast<uint64_t>(v)); written_bytes += 8; },
                [&](ConstIdx c) { emit_u16((uint16_t)c.id); written_bytes += 2; },
                [&](StrIdx s)   { emit_u16((uint16_t)s.id); written_bytes += 2; },
                [&](LabelIdx l) { 
                    patches.push_back({proto_.bytecode.size(), l.id});
                    emit_u16(0xFFFF); written_bytes += 2;
                },
                [&](JumpOffset o){ emit_u16((uint16_t)o.val); written_bytes += 2; },
                [](auto){}
            );
        }

        auto info = meow::get_op_info(inst.op);
        int padding = info.operand_bytes - written_bytes;
        if (padding > 0) {
            for(int i = 0; i < padding; ++i) emit_byte(0);
        }
    }

    for(auto& p : patches) {
        if (label_locs.count(p.label_id)) {
            size_t target = label_locs[p.label_id];
            int32_t offset = static_cast<int32_t>(target) - static_cast<int32_t>(p.pos + 2);
            proto_.bytecode[p.pos] = offset & 0xFF;
            proto_.bytecode[p.pos+1] = (offset >> 8) & 0xFF;
        }
    }

    proto_.ir_code.clear();
}

} // namespace meow::masm