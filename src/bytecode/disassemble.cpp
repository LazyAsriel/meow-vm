#include <meow/bytecode/disassemble.h>
#include <meow/common.h>
#include <meow/core/objects.h>
#include <meow/bytecode/op_codes.h>
#include <meow/value.h>
#include <meow/bytecode/chunk.h>
#include <cstring> 
#include <format>
#include <iterator>
#include <vector>
#include <algorithm>
#include <bit>

namespace meow {

static constexpr size_t CALL_IC_SIZE = 16;

template <typename T>
[[gnu::always_inline]] 
static inline T read_as(const uint8_t* code, size_t& ip) {
    T val;
    std::memcpy(&val, code + ip, sizeof(T));
    ip += sizeof(T);
    return val;
}

static inline uint8_t read_u8(const uint8_t* code, size_t& ip) { return code[ip++]; }
static inline uint16_t read_u16(const uint8_t* code, size_t& ip) { return read_as<uint16_t>(code, ip); }
static inline uint64_t read_u64(const uint8_t* code, size_t& ip) { return read_as<uint64_t>(code, ip); }
static inline double read_f64(const uint8_t* code, size_t& ip) { return read_as<double>(code, ip); }

static std::string value_to_string(const Value& value) {
    if (value.is_null()) return "null";
    if (value.is_bool()) return value.as_bool() ? "true" : "false";
    if (value.is_int()) return std::to_string(value.as_int());
    if (value.is_float()) return std::format("{:.6g}", value.as_float());
    if (value.is_string()) return std::format("\"{}\"", value.as_string()->c_str());
    if (value.is_function()) {
        auto name = value.as_function()->get_proto()->get_name();
        return std::format("<fn {}>", name ? name->c_str() : "script");
    }
    return "<obj>";
}

std::pair<std::string, size_t> disassemble_instruction(const Chunk& chunk, size_t offset) noexcept {
    const uint8_t* code = chunk.get_code();
    size_t code_size = chunk.get_code_size();
    
    if (offset >= code_size) return {"<end>", offset};

    size_t ip = offset;
    uint8_t raw_op = read_u8(code, ip);
    OpCode op = static_cast<OpCode>(raw_op);
    
    std::string line;
    line.reserve(64); 

    std::string_view op_name = meow::enum_name(op);
    if (op_name.empty()) op_name = "UNKNOWN_OP";

    std::format_to(std::back_inserter(line), "{:<16}", op_name);

    switch (op) {
        // --- CONSTANTS ---
        case OpCode::LOAD_CONST: {
            uint16_t dst = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, [{}]", dst, idx);
            if (idx < chunk.get_pool_size()) {
                std::format_to(std::back_inserter(line), " ({})", value_to_string(chunk.get_constant(idx)));
            }
            break;
        }
        case OpCode::LOAD_CONST_B: {
            uint8_t dst = read_u8(code, ip);
            uint16_t idx = read_u16(code, ip); 
            std::format_to(std::back_inserter(line), "r{}, [{}]", dst, idx);
            if (idx < chunk.get_pool_size()) {
                std::format_to(std::back_inserter(line), " ({})", value_to_string(chunk.get_constant(idx)));
            }
            break;
        }
        case OpCode::LOAD_INT: {
            uint16_t dst = read_u16(code, ip);
            int64_t val = std::bit_cast<int64_t>(read_u64(code, ip));
            std::format_to(std::back_inserter(line), "r{}, {}", dst, val);
            break;
        }
        case OpCode::LOAD_INT_B: {
            uint8_t dst = read_u8(code, ip);
            int64_t val = std::bit_cast<int64_t>(read_u64(code, ip));
            std::format_to(std::back_inserter(line), "r{}, {}", dst, val);
            break;
        }
        case OpCode::LOAD_FLOAT: {
            uint16_t dst = read_u16(code, ip);
            double val = read_f64(code, ip);
            std::format_to(std::back_inserter(line), "r{}, {:.6g}", dst, val);
            break;
        }
        case OpCode::LOAD_FLOAT_B: {
            uint8_t dst = read_u8(code, ip);
            double val = read_f64(code, ip);
            std::format_to(std::back_inserter(line), "r{}, {:.6g}", dst, val);
            break;
        }
        case OpCode::LOAD_NULL: case OpCode::LOAD_TRUE: case OpCode::LOAD_FALSE: {
            uint16_t dst = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}", dst);
            break;
        }
        case OpCode::LOAD_NULL_B: case OpCode::LOAD_TRUE_B: case OpCode::LOAD_FALSE_B: {
            uint8_t dst = read_u8(code, ip);
            std::format_to(std::back_inserter(line), "r{}", dst);
            break;
        }

        // --- MOVES ---
        case OpCode::MOVE: {
            uint16_t dst = read_u16(code, ip);
            uint16_t src = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{}", dst, src);
            break;
        }
        case OpCode::MOVE_B: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src = read_u8(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{}", dst, src);
            break;
        }

        // --- MATH / BINARY (STANDARD 16-bit) ---
        case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::DIV:
        case OpCode::MOD: case OpCode::POW:
        case OpCode::EQ: case OpCode::NEQ: case OpCode::GT: case OpCode::GE:
        case OpCode::LT: case OpCode::LE:
        case OpCode::BIT_AND: case OpCode::BIT_OR: case OpCode::BIT_XOR:
        case OpCode::LSHIFT: case OpCode::RSHIFT: {
            uint16_t dst = read_u16(code, ip);
            uint16_t r1 = read_u16(code, ip);
            uint16_t r2 = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{}, r{}", dst, r1, r2);
            break;
        }

        // --- MATH / BINARY (BYTE OPTIMIZED 8-bit) ---
        case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: case OpCode::DIV_B:
        case OpCode::MOD_B:
        case OpCode::EQ_B: case OpCode::NEQ_B: case OpCode::GT_B: case OpCode::GE_B:
        case OpCode::LT_B: case OpCode::LE_B: 
        case OpCode::BIT_AND_B: case OpCode::BIT_OR_B: case OpCode::BIT_XOR_B:
        case OpCode::LSHIFT_B: case OpCode::RSHIFT_B: {
            uint8_t dst = read_u8(code, ip);
            uint8_t r1 = read_u8(code, ip);
            uint8_t r2 = read_u8(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{}, r{}", dst, r1, r2);
            break;
        }

        // --- UNARY (STANDARD) ---
        case OpCode::NEG: case OpCode::NOT: case OpCode::BIT_NOT: {
            uint16_t dst = read_u16(code, ip);
            uint16_t src = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{}", dst, src);
            break;
        }
        case OpCode::INC: case OpCode::DEC: case OpCode::THROW: {
            uint16_t reg = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}", reg);
            break;
        }

        // --- UNARY (BYTE) ---
        case OpCode::NEG_B: case OpCode::NOT_B: case OpCode::BIT_NOT_B: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src = read_u8(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{}", dst, src);
            break;
        }
        case OpCode::INC_B: case OpCode::DEC_B: {
            uint8_t reg = read_u8(code, ip);
            std::format_to(std::back_inserter(line), "r{}", reg);
            break;
        }

        // --- GLOBALS / UPVALUES ---
        case OpCode::GET_GLOBAL: {
            uint16_t dst = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, g[{}]", dst, idx);
            if (idx < chunk.get_pool_size()) std::format_to(std::back_inserter(line), " ({})", value_to_string(chunk.get_constant(idx)));
            break;
        }
        case OpCode::SET_GLOBAL: {
            uint16_t idx = read_u16(code, ip);
            uint16_t src = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "g[{}], r{}", idx, src);
            if (idx < chunk.get_pool_size()) std::format_to(std::back_inserter(line), " ({})", value_to_string(chunk.get_constant(idx)));
            break;
        }
        case OpCode::GET_UPVALUE: {
            uint16_t dst = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, uv[{}]", dst, idx);
            break;
        }
        case OpCode::SET_UPVALUE: {
            uint16_t idx = read_u16(code, ip);
            uint16_t src = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "uv[{}], r{}", idx, src);
            break;
        }
        case OpCode::CLOSURE: {
            uint16_t dst = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, <proto {}>", dst, idx);
            break;
        }
        case OpCode::CLOSE_UPVALUES: {
            uint16_t slot = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "stack>={}", slot);
            break;
        }

        // --- JUMPS (SIMPLE) ---
        case OpCode::JUMP: {
            uint16_t offset = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "-> {:04d}", offset);
            break;
        }
        case OpCode::SETUP_TRY: {
            uint16_t offset = read_u16(code, ip);
            uint16_t err_reg = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "try -> {:04d}, catch=r{}", offset, err_reg);
            break;
        }
        case OpCode::JUMP_IF_FALSE:
        case OpCode::JUMP_IF_TRUE: {
            uint16_t cond = read_u16(code, ip);
            uint16_t offset = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{} ? -> {:04d}", cond, offset);
            break;
        }
        case OpCode::JUMP_IF_FALSE_B:
        case OpCode::JUMP_IF_TRUE_B: {
            uint8_t cond = read_u8(code, ip);
            uint16_t offset = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{} ? -> {:04d}", cond, offset);
            break;
        }

        // --- FUSED JUMPS (16-bit) ---
        case OpCode::JUMP_IF_EQ: case OpCode::JUMP_IF_NEQ:
        case OpCode::JUMP_IF_GT: case OpCode::JUMP_IF_GE:
        case OpCode::JUMP_IF_LT: case OpCode::JUMP_IF_LE: {
            uint16_t r1 = read_u16(code, ip);
            uint16_t r2 = read_u16(code, ip);
            uint16_t off = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{} ? -> {:04d}", r1, r2, off);
            break;
        }

        // --- FUSED JUMPS (8-bit) ---
        case OpCode::JUMP_IF_EQ_B: case OpCode::JUMP_IF_NEQ_B:
        case OpCode::JUMP_IF_GT_B: case OpCode::JUMP_IF_GE_B:
        case OpCode::JUMP_IF_LT_B: case OpCode::JUMP_IF_LE_B: {
            uint8_t r1 = read_u8(code, ip);
            uint8_t r2 = read_u8(code, ip);
            uint16_t off = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, r{} ? -> {:04d}", r1, r2, off);
            break;
        }

        // --- CALLS  ---
        case OpCode::CALL: {
            uint16_t dst = read_u16(code, ip);
            uint16_t fn = read_u16(code, ip);
            uint16_t arg = read_u16(code, ip);
            uint16_t argc = read_u16(code, ip);
            
            std::format_to(std::back_inserter(line), "r{} = r{}(argc={}, args=r{})", dst, fn, argc, arg);
            
            ip += CALL_IC_SIZE; 
            break;
        }
        case OpCode::CALL_VOID: {
            uint16_t fn = read_u16(code, ip);
            uint16_t arg = read_u16(code, ip);
            uint16_t argc = read_u16(code, ip);

            std::format_to(std::back_inserter(line), "void r{}(argc={}, args=r{})", fn, argc, arg);
            
            ip += CALL_IC_SIZE;
            break;
        }
        case OpCode::TAIL_CALL: {
            uint16_t dst = read_u16(code, ip); (void)dst;
            uint16_t fn = read_u16(code, ip);
            uint16_t arg = read_u16(code, ip);
            uint16_t argc = read_u16(code, ip);

            std::format_to(std::back_inserter(line), "tail r{}(argc={}, args=r{})", fn, argc, arg);
            
            ip += CALL_IC_SIZE;
            break;
        }
        case OpCode::RETURN: {
            uint16_t reg = read_u16(code, ip);
            if (reg == 0xFFFF) line += "void";
            else std::format_to(std::back_inserter(line), "r{}", reg);
            break;
        }

        // --- STRUCTURES & OOP ---
        case OpCode::NEW_ARRAY:
        case OpCode::NEW_HASH: {
            uint16_t dst = read_u16(code, ip);
            uint16_t start = read_u16(code, ip);
            uint16_t count = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, start=r{}, count={}", dst, start, count);
            break;
        }
        case OpCode::GET_INDEX: {
            uint16_t dst = read_u16(code, ip);
            uint16_t src = read_u16(code, ip);
            uint16_t key = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{} = r{}[r{}]", dst, src, key);
            break;
        }
        case OpCode::SET_INDEX: {
            uint16_t src = read_u16(code, ip);
            uint16_t key = read_u16(code, ip);
            uint16_t val = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}[r{}] = r{}", src, key, val);
            break;
        }
        case OpCode::NEW_CLASS: {
            uint16_t dst = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, name=[{}]", dst, idx);
            break;
        }
        case OpCode::NEW_INSTANCE: {
            uint16_t dst = read_u16(code, ip);
            uint16_t cls = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, class=r{}", dst, cls);
            break;
        }
        case OpCode::GET_PROP: {
            uint16_t dst = read_u16(code, ip);
            uint16_t obj = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{} = r{}.[{}]", dst, obj, idx);
            
            ip += 48; 
            break;
        }
        case OpCode::SET_PROP: {
            uint16_t obj = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            uint16_t val = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}.[{}] = r{}", obj, idx, val);
            
            ip += 48;
            break;
        }
        case OpCode::SET_METHOD: {
            uint16_t cls = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            uint16_t mth = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}.methods[{}] = r{}", cls, idx, mth);
            break;
        }
        case OpCode::INHERIT: {
            uint16_t sub = read_u16(code, ip);
            uint16_t sup = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "sub=r{}, super=r{}", sub, sup);
            break;
        }
        case OpCode::GET_SUPER: {
            uint16_t dst = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, name=[{}]", dst, idx);
            break;
        }
        case OpCode::INVOKE: {
             uint16_t dst = read_u16(code, ip);
             uint16_t obj = read_u16(code, ip);
             uint16_t idx = read_u16(code, ip);
             uint16_t arg = read_u16(code, ip);
             uint16_t argc = read_u16(code, ip);
             std::format_to(std::back_inserter(line), "r{} = r{}.[{}](argc={}, args=r{})", dst, obj, idx, argc, arg);
             ip += 80; // IC_SIZE
             break;
        }

        // --- MODULES ---
        case OpCode::IMPORT_MODULE: {
            uint16_t dst = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, path=[{}]", dst, idx);
            break;
        }
        case OpCode::EXPORT: {
            uint16_t idx = read_u16(code, ip);
            uint16_t src = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "exports[{}] = r{}", idx, src);
            break;
        }
        case OpCode::GET_EXPORT: {
            uint16_t dst = read_u16(code, ip);
            uint16_t mod = read_u16(code, ip);
            uint16_t idx = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{} = r{}::[{}]", dst, mod, idx);
            break;
        }
        case OpCode::IMPORT_ALL: {
            uint16_t mod = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "module=r{}", mod);
            break;
        }

        // --- OTHERS ---
        case OpCode::GET_KEYS:
        case OpCode::GET_VALUES: {
            uint16_t dst = read_u16(code, ip);
            uint16_t src = read_u16(code, ip);
            std::format_to(std::back_inserter(line), "r{}, from=r{}", dst, src);
            break;
        }
        case OpCode::HALT:
        case OpCode::POP_TRY:
        case OpCode::NOP:
            break; 

        default:
            line += "<unknown_operands>";
            break;
    }

    return {line, ip};
}

std::string disassemble_chunk(const Chunk& chunk, const char* name) noexcept {
    std::string out;
    out.reserve(chunk.get_code_size() * 16); 

    std::format_to(std::back_inserter(out), "== {} ==\n", name ? name : "Chunk");
    
    size_t ip = 0;
    while (ip < chunk.get_code_size()) {
        auto [str, next_ip] = disassemble_instruction(chunk, ip);
        std::format_to(std::back_inserter(out), "{:04d}: {}\n", ip, str);
        ip = next_ip;
    }
    return out;
}

std::string disassemble_around(const Chunk& chunk, size_t target_ip, int context_lines) noexcept {
    std::vector<size_t> lines;
    lines.reserve(chunk.get_code_size() / 2); 

    size_t scan_ip = 0;
    while (scan_ip < chunk.get_code_size()) {
        lines.push_back(scan_ip);
        
        auto [_, next] = disassemble_instruction(chunk, scan_ip);
        if (next <= scan_ip) scan_ip++; 
        else scan_ip = next;
    }

    std::string out;
    out.reserve(1024);

    auto it = std::upper_bound(lines.begin(), lines.end(), target_ip);
    
    size_t found_idx = 0;
    bool is_aligned = false;

    if (it == lines.begin()) {
        std::format_to(std::back_inserter(out), "CRITICAL ERROR: Target IP {} is before start of chunk!\n", target_ip);
        found_idx = 0;
    } else {
        --it;
        size_t start_ip = *it;
        found_idx = std::distance(lines.begin(), it);
        
        if (start_ip == target_ip) {
            is_aligned = true;
        } else {
            std::format_to(std::back_inserter(out), 
                "WARNING: IP Misalignment detected!\n"
                "Runtime IP: {}\n"
                "Scanner thinks instruction starts at: {}\n"
                "Diff: {} bytes (Runtime jumped into middle of instruction?)\n"
                "------------------------------------------------\n",
                target_ip, start_ip, target_ip - start_ip);
        }
    }

    size_t start_idx = (found_idx > (size_t)context_lines) ? found_idx - context_lines : 0;
    size_t end_idx = std::min(lines.size(), found_idx + context_lines + 1);

    for (size_t i = start_idx; i < end_idx; ++i) {
        size_t ip = lines[i];
        auto [str, next_ip] = disassemble_instruction(chunk, ip);
        
        if (ip == lines[found_idx]) {
            if (is_aligned) {
                std::format_to(std::back_inserter(out), " -> {:04d}: {}   <--- HERE (Exact)\n", ip, str);
            } else {
                std::format_to(std::back_inserter(out), " -> {:04d}: {}   <--- Scanner sees this\n", ip, str);
                
                long long diff = static_cast<long long>(target_ip) - static_cast<long long>(ip);
                std::format_to(std::back_inserter(out), "    ....: (Runtime is at offset {:+} inside here)\n", diff);
            }
        } else {
            std::format_to(std::back_inserter(out), "    {:04d}: {}\n", ip, str);
        }
    }

    return out;
}

} // namespace meow