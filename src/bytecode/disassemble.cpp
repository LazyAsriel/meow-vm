#include "bytecode/disassemble.h"
#include "common/definitions.h"
#include "core/objects.h"
#include "bytecode/op_codes.h"
#include "core/value.h"
#include "bytecode/chunk.h"

namespace meow {

static constexpr std::array<std::string_view, static_cast<size_t>(OpCode::TOTAL_OPCODES)> opcode_strings = {
    "LOAD_CONST", "LOAD_NULL",     "LOAD_TRUE",     "LOAD_FALSE", "LOAD_INT",   "LOAD_FLOAT",   "MOVE",        "ADD",       "SUB",
    "MUL",        "DIV",           "MOD",           "POW",        "EQ",         "NEQ",          "GT",          "GE",        "LT",
    "LE",         "NEG",           "NOT",           "GET_GLOBAL", "SET_GLOBAL", "GET_UPVALUE",  "SET_UPVALUE", "CLOSURE",   "CLOSE_UPVALUES",
    "JUMP",       "JUMP_IF_FALSE", "JUMP_IF_TRUE",  "CALL",       "CALL_VOID",  "RETURN",       "HALT",        "NEW_ARRAY", "NEW_HASH",
    "GET_INDEX",  "SET_INDEX",     "GET_KEYS",      "GET_VALUES", "NEW_CLASS",  "NEW_INSTANCE", "GET_PROP",    "SET_PROP",  "SET_METHOD",
    "INHERIT",    "GET_SUPER",     "BIT_AND",       "BIT_OR",     "BIT_XOR",    "BIT_NOT",      "LSHIFT",      "RSHIFT",    "THROW",
    "SETUP_TRY",  "POP_TRY",       "IMPORT_MODULE", "EXPORT",     "GET_EXPORT", "IMPORT_ALL",
};

inline static std::string_view opcode_to_string(OpCode op) noexcept {
    return opcode_strings[static_cast<size_t>(op)];
}

static inline uint16_t read_u16_le(const uint8_t* code, size_t& ip, size_t code_size) noexcept {
    if (ip + 2 > code_size) {
        ip = code_size;
        return 0;
    }
    uint16_t val = static_cast<uint16_t>(code[ip]) | (static_cast<uint16_t>(code[ip + 1]) << 8);
    ip += 2;
    return val;
}

static inline uint64_t read_u64_le(const uint8_t* code, size_t& ip, size_t code_size) noexcept {
    if (ip + 8 > code_size) {
        ip = code_size;
        return 0;
    }
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (static_cast<uint64_t>(code[ip + i]) << (i * 8));
    }
    ip += 8;
    return val;
}

static inline int64_t read_i64_le(const uint8_t* code, size_t& ip, size_t code_size) noexcept {
    return std::bit_cast<int64_t>(read_u64_le(code, ip, code_size));
}

static inline double read_f64_le(const uint8_t* code, size_t& ip, size_t code_size) noexcept {
    return std::bit_cast<double>(read_u64_le(code, ip, code_size));
}

std::string disassemble_chunk(const Chunk& chunk) noexcept {
    std::ostringstream os;
    const uint8_t* code = chunk.get_code();
    size_t code_size = chunk.get_code_size();

    auto value_to_string = [&](const value_t& value) -> std::string {
        if (value.is_null()) return "<null>";
        if (value.is_bool()) return value.as_bool() ? "true" : "false";
        if (value.is_int()) return std::to_string(value.as_int());
        if (value.is_float()) {
            std::ostringstream temp_os;
            double r = value.as_float();
            if (std::isnan(r)) return "NaN";
            if (std::isinf(r)) return (r > 0) ? "Infinity" : "-Infinity";
            temp_os << r;
            return temp_os.str();
        }
        if (value.is_native()) return "<native_fn>";

        if (value.is_string()) {
            return "\"" + std::string(value.as_string()->c_str()) + "\"";
        }
        if (value.is_proto()) {
            auto name = value.as_proto()->get_name();
            return "<proto '" + (name ? std::string(name->c_str()) : "??") + "'>";
        }
        if (value.is_function()) return "<function>";
        if (value.is_class()) {
             auto name = value.as_class()->get_name();
            return "<class '" + (name ? std::string(name->c_str()) : "??") + "'>";
        }
        if (value.is_array()) return "<array>";
        if (value.is_hash_table()) return "<hash_table>";
        if (value.is_instance()) return "<instance>";
        if (value.is_bound_method()) return "<bound_method>";
        if (value.is_upvalue()) return "<upvalue>";
        // if (value.is_native()) return "<native_fn>";
        if (value.is_module()) {
            auto name = value.as_module()->get_file_name();
            return "<module '" + (name ? std::string(name->c_str()) : "??") + "'>";
        }
        
        return "<unknown_object>";
    };

    os << "  - Bytecode:\n";
    for (size_t ip = 0; ip < code_size;) {
        size_t inst_offset = ip;
        uint8_t raw_opcode = code[ip++];
        OpCode op = static_cast<OpCode>(raw_opcode);

        os << "     " << std::right << std::setw(4) << static_cast<int>(inst_offset) << ": ";
        std::string op_name = std::string(opcode_to_string(op));
        os << std::left << std::setw(12) << op_name;

        switch (op) {
            case OpCode::MOVE: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t src = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", src=" << src << "]";
                break;
            }
            case OpCode::LOAD_CONST: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t cidx = read_u16_le(code, ip, code_size);
                std::string val_str = (cidx < chunk.get_pool_size()) ? value_to_string(chunk.get_constant(cidx)) : "<const_oob>";
                os << "  args=[dst=" << dst << ", cidx=" << cidx << " -> " << val_str << "]";
                break;
            }
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                int64_t val = read_i64_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", val=" << val << "]";
                break;
            }
            case OpCode::LOAD_FLOAT: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                double val = read_f64_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", val=" << val << "]";
                break;
            }
            case OpCode::LOAD_NULL:
            case OpCode::LOAD_TRUE:
            case OpCode::LOAD_FALSE: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << "]";
                break;
            }
            case OpCode::ADD:
            case OpCode::SUB:
            case OpCode::MUL:
            case OpCode::DIV:
            case OpCode::MOD:
            case OpCode::POW:
            case OpCode::EQ:
            case OpCode::NEQ:
            case OpCode::GT:
            case OpCode::GE:
            case OpCode::LT:
            case OpCode::LE:
            case OpCode::BIT_AND:
            case OpCode::BIT_OR:
            case OpCode::BIT_XOR:
            case OpCode::LSHIFT:
            case OpCode::RSHIFT: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t r1 = read_u16_le(code, ip, code_size);
                uint16_t r2 = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", r1=" << r1 << ", r2=" << r2 << "]";
                break;
            }
            case OpCode::NEG:
            case OpCode::NOT:
            case OpCode::BIT_NOT: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t src = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", src=" << src << "]";
                break;
            }
            case OpCode::GET_GLOBAL: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t cidx = read_u16_le(code, ip, code_size);
                std::string name = (cidx < chunk.get_pool_size()) ? value_to_string(chunk.get_constant(cidx)) : "<bad_name>";
                os << "  args=[dst=" << dst << ", name_idx=" << cidx << " -> " << name << "]";
                break;
            }
            case OpCode::SET_GLOBAL: {
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                uint16_t src = read_u16_le(code, ip, code_size);
                std::string name = (name_idx < chunk.get_pool_size()) ? value_to_string(chunk.get_constant(name_idx)) : "<bad_name>";
                os << "  args=[name_idx=" << name_idx << " -> " << name << ", src=" << src << "]";
                break;
            }
            case OpCode::GET_UPVALUE: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t uv = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", uv_index=" << uv << "]";
                break;
            }
            case OpCode::SET_UPVALUE: {
                uint16_t uv = read_u16_le(code, ip, code_size);
                uint16_t src = read_u16_le(code, ip, code_size);
                os << "  args=[uv_index=" << uv << ", src=" << src << "]";
                break;
            }
            case OpCode::CLOSURE: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t proto_idx = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", proto_idx=" << proto_idx;

                if (proto_idx < chunk.get_pool_size() && chunk.get_constant(proto_idx).is_proto()) {
                    auto* proto = chunk.get_constant(proto_idx).as_proto();
                    if (proto) {
                        size_t num_upvalues = proto->get_num_upvalues();
                        os << ", upvalues=" << num_upvalues << " {";
                        for (size_t i = 0; i < num_upvalues; ++i) {
                            const auto& desc = proto->get_desc(i);
                            os << (i > 0 ? ", " : "") << (desc.is_local_ ? "local" : "parent") << ":" << desc.index_;
                        }
                        os << "}";
                    } else
                        os << ", <null_proto>";
                } else
                    os << ", <proto_not_found>";
                os << "]";
                break;
            }
            case OpCode::CLOSE_UPVALUES: {
                uint16_t start_slot = read_u16_le(code, ip, code_size);
                os << "  args=[start_slot=" << start_slot << "]";
                break;
            }
            case OpCode::JUMP:
            case OpCode::SETUP_TRY: {
                uint16_t target = read_u16_le(code, ip, code_size);
                os << "  args=[target=" << target << "]";
                break;
            }
            case OpCode::JUMP_IF_FALSE:
            case OpCode::JUMP_IF_TRUE: {
                uint16_t reg = read_u16_le(code, ip, code_size);
                uint16_t target = read_u16_le(code, ip, code_size);
                os << "  args=[reg=" << reg << ", target=" << target << "]";
                break;
            }
            case OpCode::CALL: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t fn_reg = read_u16_le(code, ip, code_size);
                uint16_t arg_start = read_u16_le(code, ip, code_size);
                uint16_t argc = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", fn_reg=" << fn_reg << ", arg_start=" << arg_start << ", argc=" << argc << "]";
                break;
            }
            case OpCode::CALL_VOID: {
                uint16_t fn_reg = read_u16_le(code, ip, code_size);
                uint16_t arg_start = read_u16_le(code, ip, code_size);
                uint16_t argc = read_u16_le(code, ip, code_size);
                os << "  args=[fn_reg=" << fn_reg << ", arg_start=" << arg_start << ", argc=" << argc << "]";
                break;
            }
            case OpCode::RETURN: {
                uint16_t ret_reg = read_u16_le(code, ip, code_size);
                os << "  args=[ret_reg=" << ret_reg << ((ret_reg == 0xFFFF) ? " (void)" : "") << "]";
                break;
            }
            case OpCode::HALT:
            case OpCode::POP_TRY: {
                os << "  args=[]";
                break;
            }
            case OpCode::NEW_ARRAY:
            case OpCode::NEW_HASH: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t start_idx = read_u16_le(code, ip, code_size);
                uint16_t count = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", start_idx=" << start_idx << ", count=" << count << "]";
                break;
            }
            case OpCode::GET_INDEX: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t src_reg = read_u16_le(code, ip, code_size);
                uint16_t key_reg = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", src=" << src_reg << ", key=" << key_reg << "]";
                break;
            }
            case OpCode::SET_INDEX: {
                uint16_t src_reg = read_u16_le(code, ip, code_size);
                uint16_t key_reg = read_u16_le(code, ip, code_size);
                uint16_t val_reg = read_u16_le(code, ip, code_size);
                os << "  args=[src=" << src_reg << ", key=" << key_reg << ", val=" << val_reg << "]";
                break;
            }
            case OpCode::GET_KEYS:
            case OpCode::GET_VALUES: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t src_reg = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", src=" << src_reg << "]";
                break;
            }
            case OpCode::IMPORT_MODULE: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t path_idx = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", path_idx=" << path_idx << "]";
                break;
            }
            case OpCode::EXPORT: {
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                uint16_t src_reg = read_u16_le(code, ip, code_size);
                os << "  args=[name_idx=" << name_idx << ", src=" << src_reg << "]";
                break;
            }
            case OpCode::GET_EXPORT: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t module_reg = read_u16_le(code, ip, code_size);
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", module_reg=" << module_reg << ", name_idx=" << name_idx << "]";
                break;
            }
            case OpCode::IMPORT_ALL: {
                uint16_t module_reg = read_u16_le(code, ip, code_size);
                os << "  args=[module_reg=" << module_reg << "]";
                break;
            }
            case OpCode::NEW_CLASS: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", name_idx=" << name_idx << "]";
                break;
            }
            case OpCode::NEW_INSTANCE: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t class_reg = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", class_reg=" << class_reg << "]";
                break;
            }
            case OpCode::GET_PROP: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t obj_reg = read_u16_le(code, ip, code_size);
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", obj_reg=" << obj_reg << ", name_idx=" << name_idx << "]";
                break;
            }
            case OpCode::SET_PROP: {
                uint16_t obj_reg = read_u16_le(code, ip, code_size);
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                uint16_t val_reg = read_u16_le(code, ip, code_size);
                os << "  args=[obj_reg=" << obj_reg << ", name_idx=" << name_idx << ", val_reg=" << val_reg << "]";
                break;
            }
            case OpCode::SET_METHOD: {
                uint16_t class_reg = read_u16_le(code, ip, code_size);
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                uint16_t method_reg = read_u16_le(code, ip, code_size);
                os << "  args=[class_reg=" << class_reg << ", name_idx=" << name_idx << ", method_reg=" << method_reg << "]";
                break;
            }
            case OpCode::INHERIT: {
                uint16_t sub_class_reg = read_u16_le(code, ip, code_size);
                uint16_t super_class_reg = read_u16_le(code, ip, code_size);
                os << "  args=[sub_class_reg=" << sub_class_reg << ", super_class_reg=" << super_class_reg << "]";
                break;
            }
            case OpCode::GET_SUPER: {
                uint16_t dst = read_u16_le(code, ip, code_size);
                uint16_t name_idx = read_u16_le(code, ip, code_size);
                os << "  args=[dst=" << dst << ", name_idx=" << name_idx << "]";
                break;
            }
            case OpCode::THROW: {
                uint16_t reg = read_u16_le(code, ip, code_size);
                os << "  args=[reg=" << reg << "]";
                break;
            }
            default: {
                os << "  args=[<unparsed>]";
                // Cố gắng nhảy qua các tham số nếu không biết
                // (Đây là phỏng đoán, có thể không chính xác)
                break;
            }
        }
        os << "\n";
    }
    return os.str();
}

}