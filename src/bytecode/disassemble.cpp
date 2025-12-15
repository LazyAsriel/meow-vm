#include <meow/bytecode/disassemble.h>
#include <meow/definitions.h>
#include <meow/core/objects.h>
#include <meow/bytecode/op_codes.h>
#include <meow/value.h>
#include <meow/bytecode/chunk.h>

#include <format> // C++20/23: Nhanh hơn rất nhiều so với iostream
#include <vector>
#include <bit>
#include <cstring>

namespace meow {

// --- Constants ---
static constexpr size_t CALL_IC_SIZE = 16; // 2 pointers (tag + dest)

// --- Opcode Name Map (Compile-time) ---
constexpr std::string_view get_opcode_name(OpCode op) {
    switch (op) {
#define OP(name) case OpCode::name: return #name;
        OP(LOAD_CONST) OP(LOAD_NULL) OP(LOAD_TRUE) OP(LOAD_FALSE)
        OP(LOAD_INT) OP(LOAD_FLOAT) OP(MOVE)
        OP(INC) OP(DEC)
        OP(ADD) OP(SUB) OP(MUL) OP(DIV) OP(MOD) OP(POW)
        OP(EQ) OP(NEQ) OP(GT) OP(GE) OP(LT) OP(LE)
        OP(NEG) OP(NOT)
        OP(BIT_AND) OP(BIT_OR) OP(BIT_XOR) OP(BIT_NOT)
        OP(LSHIFT) OP(RSHIFT)
        OP(GET_GLOBAL) OP(SET_GLOBAL)
        OP(GET_UPVALUE) OP(SET_UPVALUE)
        OP(CLOSURE) OP(CLOSE_UPVALUES)
        OP(JUMP) OP(JUMP_IF_FALSE) OP(JUMP_IF_TRUE)
        OP(CALL) OP(CALL_VOID) OP(RETURN) OP(HALT)
        OP(NEW_ARRAY) OP(NEW_HASH)
        OP(GET_INDEX) OP(SET_INDEX)
        OP(GET_KEYS) OP(GET_VALUES)
        OP(NEW_CLASS) OP(NEW_INSTANCE)
        OP(GET_PROP) OP(SET_PROP) OP(SET_METHOD)
        OP(INHERIT) OP(GET_SUPER)
        OP(THROW) OP(SETUP_TRY) OP(POP_TRY)
        OP(IMPORT_MODULE) OP(EXPORT) OP(GET_EXPORT) OP(IMPORT_ALL)
        OP(TAIL_CALL)
        
        OP(ADD_B) OP(SUB_B) OP(MUL_B) OP(DIV_B) OP(MOD_B)
        OP(EQ_B) OP(NEQ_B) OP(GT_B) OP(GE_B) OP(LT_B) OP(LE_B)
        OP(JUMP_IF_TRUE_B) OP(JUMP_IF_FALSE_B)
        OP(MOVE_B) OP(LOAD_INT_B)
#undef OP
        default: return "UNKNOWN_OP";
    }
}

// --- High Performance Readers ---
// Dùng memcpy để compiler tối ưu thành lệnh MOV đơn lẻ, an toàn bộ nhớ.

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

// --- Formatters ---

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

// --- Main Logic ---

std::pair<std::string, size_t> disassemble_instruction(const Chunk& chunk, size_t offset) noexcept {
    const uint8_t* code = chunk.get_code();
    size_t code_size = chunk.get_code_size();
    
    if (offset >= code_size) return {"<end>", offset};

    size_t ip = offset;
    uint8_t raw_op = read_u8(code, ip);
    OpCode op = static_cast<OpCode>(raw_op);
    
    // Sử dụng std::string buffer để tránh allocation liên tục nếu có thể
    std::string line;
    line.reserve(64); 

    // 1. In tên OpCode (Căn lề trái 16 ký tự)
    std::format_to(std::back_inserter(line), "{:<16}", get_opcode_name(op));

    try {
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
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16(code, ip);
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
            case OpCode::LOAD_NULL:
            case OpCode::LOAD_TRUE:
            case OpCode::LOAD_FALSE: {
                uint16_t dst = read_u16(code, ip);
                std::format_to(std::back_inserter(line), "r{}", dst);
                break;
            }
            case OpCode::LOAD_INT_B: {
                uint8_t dst = read_u8(code, ip);
                int8_t val = static_cast<int8_t>(read_u8(code, ip));
                std::format_to(std::back_inserter(line), "r{}, {}", dst, val);
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

            // --- MATH / BINARY (STANDARD) ---
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

            // --- MATH / BINARY (BYTE OPTIMIZED) ---
            case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: case OpCode::DIV_B:
            case OpCode::MOD_B:
            case OpCode::EQ_B: case OpCode::NEQ_B: case OpCode::GT_B: case OpCode::GE_B:
            case OpCode::LT_B: case OpCode::LE_B: {
                uint8_t dst = read_u8(code, ip);
                uint8_t r1 = read_u8(code, ip);
                uint8_t r2 = read_u8(code, ip);
                std::format_to(std::back_inserter(line), "r{}, r{}, r{}", dst, r1, r2);
                break;
            }

            // --- UNARY ---
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

            // --- JUMPS ---
            case OpCode::JUMP: 
            case OpCode::SETUP_TRY: {
                uint16_t offset = read_u16(code, ip);
                std::format_to(std::back_inserter(line), "-> {:04d}", offset);
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

            // --- CALLS (CRITICAL FIX: INLINE CACHE) ---
            case OpCode::CALL: {
                uint16_t dst = read_u16(code, ip);
                uint16_t fn = read_u16(code, ip);
                uint16_t arg = read_u16(code, ip);
                uint16_t argc = read_u16(code, ip);
                
                std::format_to(std::back_inserter(line), "r{} = r{}(argc={}, args=r{})", dst, fn, argc, arg);
                
                // SKIP INLINE CACHE (16 bytes)
                ip += CALL_IC_SIZE; 
                break;
            }
            case OpCode::CALL_VOID: {
                uint16_t fn = read_u16(code, ip);
                uint16_t arg = read_u16(code, ip);
                uint16_t argc = read_u16(code, ip);

                std::format_to(std::back_inserter(line), "void r{}(argc={}, args=r{})", fn, argc, arg);
                
                // SKIP INLINE CACHE (16 bytes)
                ip += CALL_IC_SIZE;
                break;
            }
            case OpCode::TAIL_CALL: {
                uint16_t dst = read_u16(code, ip); (void)dst;
                uint16_t fn = read_u16(code, ip);
                uint16_t arg = read_u16(code, ip);
                uint16_t argc = read_u16(code, ip);

                std::format_to(std::back_inserter(line), "tail r{}(argc={}, args=r{})", fn, argc, arg);
                
                // SKIP INLINE CACHE (16 bytes)
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
                break; // No operands

            default:
                line += "<unknown_operands>";
                break;
        }
    } catch (...) {
        line += " <DECODE_ERROR>";
    }

    return {line, ip};
}

std::string disassemble_chunk(const Chunk& chunk, const char* name) noexcept {
    std::string out;
    // Dự đoán kích thước buffer để giảm reallocation: 64 bytes/line
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

    // 1. Quét toàn bộ Chunk để map ranh giới Instruction
    size_t scan_ip = 0;
    while (scan_ip < chunk.get_code_size()) {
        lines.push_back(scan_ip);
        
        // Safety break để tránh loop vô tận nếu disassemble lỗi
        auto [_, next] = disassemble_instruction(chunk, scan_ip);
        if (next <= scan_ip) scan_ip++; 
        else scan_ip = next;
    }

    std::string out;
    out.reserve(1024);

    // 2. Tìm instruction chứa target_ip (Fuzzy Search)
    // Tìm vị trí đầu tiên mà ip > target_ip
    auto it = std::upper_bound(lines.begin(), lines.end(), target_ip);
    
    size_t found_idx = 0;
    bool is_aligned = false;

    if (it == lines.begin()) {
        // Target IP nhỏ hơn instruction đầu tiên?? (Impossible trừ khi target_ip rất lớn wrap around)
        std::format_to(std::back_inserter(out), "CRITICAL ERROR: Target IP {} is before start of chunk!\n", target_ip);
        found_idx = 0;
    } else {
        // Lùi lại 1 bước để lấy instruction bắt đầu trước hoặc tại target_ip
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

    // 3. Tính toán vùng in (Context)
    size_t start_idx = (found_idx > (size_t)context_lines) ? found_idx - context_lines : 0;
    size_t end_idx = std::min(lines.size(), found_idx + context_lines + 1);

    // 4. In ra
    for (size_t i = start_idx; i < end_idx; ++i) {
        size_t ip = lines[i];
        auto [str, next_ip] = disassemble_instruction(chunk, ip);
        
        // Highlight logic
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