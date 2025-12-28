#include <meow/masm/assembler.h>
#include <iostream>
#include <fstream>
#include <charconv> 
#include <bit>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <iomanip> // cho std::quoted

namespace meow::masm {

Assembler::Assembler(const std::vector<Token>& tokens) : tokens_(tokens) {}

Token Assembler::peek() const { return tokens_[current_]; }
Token Assembler::previous() const { return tokens_[current_ - 1]; }
bool Assembler::is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
Token Assembler::advance() { if (!is_at_end()) current_++; return previous(); }

Token Assembler::consume(TokenType type, const std::string& msg) {
    if (peek().type == type) return advance();
    throw std::runtime_error(msg + " (Found: " + std::string(peek().lexeme) + " at line " + std::to_string(peek().line) + ")");
}

void Assembler::parse_statement() {
    Token tk = peek();
    switch (tk.type) {
        case TokenType::DIR_FUNC:      parse_func(); break;
        case TokenType::DIR_REGISTERS: parse_registers(); break;
        case TokenType::DIR_UPVALUES:  parse_upvalues_decl(); break;
        case TokenType::DIR_UPVALUE:   parse_upvalue_def(); break;
        case TokenType::DIR_CONST:     parse_const(); break;
        case TokenType::LABEL_DEF:     parse_label(); break;
        case TokenType::OPCODE:        parse_instruction(); break;
        case TokenType::DIR_ENDFUNC:   advance(); curr_proto_ = nullptr; break;
        
        // Xử lý Directive #@ (Annotation)
        case TokenType::ANNOTATION:    parse_annotation(); break; 
        
        case TokenType::IDENTIFIER:    throw std::runtime_error("Unexpected identifier: " + std::string(tk.lexeme));
        default: throw std::runtime_error("Unexpected token: " + std::string(tk.lexeme));
    }
}

void Assembler::parse_annotation() {
    Token ann = advance(); 
    std::string key(ann.lexeme);

    // Xác định target: Nếu trong hàm thì sửa cờ hàm, nếu ngoài hàm thì sửa cờ toàn cục
    ProtoFlags* target = (curr_proto_) ? &curr_proto_->flags : &global_flags_;

    if (key == "debug") {
        *target = *target | ProtoFlags::HAS_DEBUG_INFO;
    } 
    else if (key == "no_debug") {
        *target = *target & (~ProtoFlags::HAS_DEBUG_INFO);
    }
    else if (key == "vararg") {
        *target = *target | ProtoFlags::IS_VARARG;
    }
    else {
        std::cout << "[MASM] Warning: Unknown annotation '#@ " << key << "'" << std::endl;
    }
}

void Assembler::parse_func() {
    advance(); 
    Token name = consume(TokenType::IDENTIFIER, "Expected func name");
    std::string func_name(name.lexeme);
    if (func_name.starts_with("@")) func_name = func_name.substr(1);
    protos_.emplace_back();
    protos_.back().name = func_name;
    
    curr_proto_ = &protos_.back();
    
    // Kế thừa cờ từ cấu hình toàn cục
    curr_proto_->flags = global_flags_;

    proto_name_map_[func_name] = protos_.size() - 1;
}

void Assembler::parse_registers() {
    if (!curr_proto_) throw std::runtime_error("Outside .func");
    advance();
    Token num = consume(TokenType::NUMBER_INT, "Expected number");
    curr_proto_->num_regs = std::stoi(std::string(num.lexeme));
}

void Assembler::parse_upvalues_decl() {
    if (!curr_proto_) throw std::runtime_error("Outside .func");
    advance();
    Token num = consume(TokenType::NUMBER_INT, "Expected number");
    curr_proto_->num_upvalues = std::stoi(std::string(num.lexeme));
    curr_proto_->upvalues.resize(curr_proto_->num_upvalues);
}

void Assembler::parse_upvalue_def() {
    if (!curr_proto_) throw std::runtime_error("Outside .func");
    advance();
    uint32_t idx = std::stoi(std::string(consume(TokenType::NUMBER_INT, "Idx").lexeme));
    std::string type(consume(TokenType::IDENTIFIER, "Type").lexeme);
    uint32_t slot = std::stoi(std::string(consume(TokenType::NUMBER_INT, "Slot").lexeme));
    if (idx < curr_proto_->upvalues.size()) curr_proto_->upvalues[idx] = { (type == "local"), slot };
}

void Assembler::parse_const() {
    if (!curr_proto_) throw std::runtime_error("Outside .func");
    advance();
    Constant c;
    Token tk = peek();
    
    if (tk.type == TokenType::STRING) {
        c.type = ConstType::STRING_T;
        c.val_str = parse_string_literal(tk.lexeme);
        advance();
    } else if (tk.type == TokenType::NUMBER_INT) {
        c.type = ConstType::INT_T;
        std::string_view sv = tk.lexeme;
        if (sv.starts_with("0x") || sv.starts_with("0X")) {
            c.val_i64 = std::stoll(std::string(sv), nullptr, 16);
        } else {
            std::from_chars(sv.data(), sv.data() + sv.size(), c.val_i64);
        }
        advance();
    } else if (tk.type == TokenType::NUMBER_FLOAT) {
        c.type = ConstType::FLOAT_T;
        c.val_f64 = std::stod(std::string(tk.lexeme));
        advance();
    } else if (tk.type == TokenType::IDENTIFIER) {
        if (tk.lexeme == "null") { c.type = ConstType::NULL_T; advance(); }
        else if (tk.lexeme == "true") { c.type = ConstType::INT_T; c.val_i64 = 1; advance(); } 
        else if (tk.lexeme == "false") { c.type = ConstType::INT_T; c.val_i64 = 0; advance(); }
        else if (tk.lexeme.starts_with("@")) {
            c.type = ConstType::PROTO_REF_T;
            c.val_str = tk.lexeme.substr(1);
            advance();
        } else throw std::runtime_error("Unknown constant identifier: " + std::string(tk.lexeme));
    } else throw std::runtime_error("Invalid constant");
    curr_proto_->constants.push_back(c);
}

void Assembler::parse_label() {
    Token lbl = advance();
    std::string labelName(lbl.lexeme); 
    curr_proto_->labels[labelName] = curr_proto_->bytecode.size();
}

void Assembler::emit_byte(uint8_t b) { curr_proto_->bytecode.push_back(b); }
void Assembler::emit_u16(uint16_t v) { emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); }
void Assembler::emit_u32(uint32_t v) { 
    emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); 
    emit_byte((v >> 16) & 0xFF); emit_byte((v >> 24) & 0xFF); 
}
void Assembler::emit_u64(uint64_t v) { for(int i=0; i<8; ++i) emit_byte((v >> (i*8)) & 0xFF); }

void Assembler::parse_instruction() {
    if (!curr_proto_) throw std::runtime_error("Instruction outside .func");
    
    // Lưu offset cho Debug Info
    uint32_t current_offset = curr_proto_->bytecode.size();

    Token op_tok = advance();
    if (OP_MAP.find(op_tok.lexeme) == OP_MAP.end()) {
        throw std::runtime_error("Unknown opcode: " + std::string(op_tok.lexeme));
    }
    meow::OpCode op = OP_MAP[op_tok.lexeme];
    emit_byte(static_cast<uint8_t>(op));

    auto parse_u16 = [&]() {
        Token t = consume(TokenType::NUMBER_INT, "Expected u16");
        emit_u16(static_cast<uint16_t>(std::stoi(std::string(t.lexeme))));
    };

    switch (op) {
        case meow::OpCode::LOAD_INT: {
            parse_u16(); // dst
            Token t = consume(TokenType::NUMBER_INT, "Expected int64");
            int64_t val;
            std::from_chars(t.lexeme.data(), t.lexeme.data() + t.lexeme.size(), val);
            emit_u64(std::bit_cast<uint64_t>(val));
            break;
        }
        case meow::OpCode::LOAD_FLOAT: {
            parse_u16(); // dst
            Token t = consume(TokenType::NUMBER_FLOAT, "Expected double");
            double val = std::stod(std::string(t.lexeme));
            emit_u64(std::bit_cast<uint64_t>(val));
            break;
        }
        case meow::OpCode::JUMP: case meow::OpCode::SETUP_TRY: {
            if (peek().type == TokenType::IDENTIFIER) {
                Token target = advance();
                curr_proto_->try_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
                emit_u16(0xFFFF);
                if (op == meow::OpCode::SETUP_TRY) parse_u16(); 
            } else {
                parse_u16();
                if (op == meow::OpCode::SETUP_TRY) parse_u16();
            }
            break;
        }
        case meow::OpCode::JUMP_IF_FALSE: case meow::OpCode::JUMP_IF_TRUE: {
            parse_u16(); // reg
            if (peek().type == TokenType::IDENTIFIER) {
                Token target = advance();
                curr_proto_->jump_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
                emit_u16(0xFFFF);
            } else parse_u16();
            break;
        }
        case meow::OpCode::GET_PROP: case meow::OpCode::SET_PROP: {
            for(int i=0; i<3; ++i) parse_u16();
            for (int j = 0; j < 4; ++j) { emit_u64(0); emit_u32(0); } // Cache
            break;
        }
        case meow::OpCode::CALL: case meow::OpCode::TAIL_CALL: {
            for(int i=0; i<4; ++i) parse_u16();
            emit_u64(0); emit_u64(0); // Cache
            break;
        }
        case meow::OpCode::CALL_VOID: {
            for(int i=0; i<3; ++i) parse_u16();
            emit_u64(0); emit_u64(0); 
            break;
        }
        default: {
            int args = get_arity(op);
            for(int i=0; i<args; ++i) parse_u16();
            break;
        }
    }

    // Xử lý Debug Info từ source (#^ "file" line:col)
    if (peek().type == TokenType::DEBUG_INFO) {
        Token dbg = advance();
        
        std::string s(dbg.lexeme);
        std::stringstream ss(s);
        std::string file_part;
        char c;
        
        if (ss >> std::ws && ss.peek() == '"') {
             ss >> std::quoted(file_part);
        }
        
        uint32_t line = 0, col = 0;
        ss >> line >> c >> col; 
        
        uint32_t file_idx = curr_proto_->add_file(file_part);
        curr_proto_->lines.push_back({current_offset, line, col, file_idx});
    }
}

int Assembler::get_arity(meow::OpCode op) {
    switch (op) {
        case meow::OpCode::CLOSE_UPVALUES: case meow::OpCode::IMPORT_ALL: case meow::OpCode::THROW: 
        case meow::OpCode::RETURN: case meow::OpCode::LOAD_NULL: case meow::OpCode::LOAD_TRUE: case meow::OpCode::LOAD_FALSE:
        case meow::OpCode::INC: case meow::OpCode::DEC:
            return 1;
        case meow::OpCode::LOAD_CONST: case meow::OpCode::MOVE: 
        case meow::OpCode::NEG: case meow::OpCode::NOT: case meow::OpCode::BIT_NOT: 
        case meow::OpCode::GET_UPVALUE: case meow::OpCode::SET_UPVALUE: 
        case meow::OpCode::CLOSURE:
        case meow::OpCode::NEW_CLASS: case meow::OpCode::NEW_INSTANCE: 
        case meow::OpCode::IMPORT_MODULE:
        case meow::OpCode::EXPORT: 
        case meow::OpCode::GET_KEYS: case meow::OpCode::GET_VALUES:
        case meow::OpCode::GET_SUPER: 
        case meow::OpCode::GET_GLOBAL: case meow::OpCode::SET_GLOBAL:
        case meow::OpCode::INHERIT:
            return 2;
        case meow::OpCode::GET_EXPORT: 
        case meow::OpCode::ADD: case meow::OpCode::SUB: case meow::OpCode::MUL: case meow::OpCode::DIV:
        case meow::OpCode::MOD: case meow::OpCode::POW: case meow::OpCode::EQ: case meow::OpCode::NEQ:
        case meow::OpCode::GT: case meow::OpCode::GE: case meow::OpCode::LT: case meow::OpCode::LE:
        case meow::OpCode::BIT_AND: case meow::OpCode::BIT_OR: case meow::OpCode::BIT_XOR:
        case meow::OpCode::LSHIFT: case meow::OpCode::RSHIFT: 
        case meow::OpCode::NEW_ARRAY: case meow::OpCode::NEW_HASH: 
        case meow::OpCode::GET_INDEX: case meow::OpCode::SET_INDEX: 
        case meow::OpCode::GET_PROP: case meow::OpCode::SET_PROP:
        case meow::OpCode::SET_METHOD: case meow::OpCode::CALL_VOID:
            return 3;
        case meow::OpCode::CALL:
        case meow::OpCode::TAIL_CALL:
            return 4;
        case meow::OpCode::INVOKE: return 5;
        default: return 0;
    }
}

void Assembler::link_proto_refs() {
    for (auto& p : protos_) {
        for (auto& c : p.constants) {
            if (c.type == ConstType::PROTO_REF_T) {
                if (proto_name_map_.count(c.val_str)) c.proto_index = proto_name_map_[c.val_str];
                else throw std::runtime_error("Undefined proto: " + c.val_str);
            }
        }
    }
}

void Assembler::patch_labels() {
    for (auto& p : protos_) {
        auto apply = [&](auto& patches) {
            for (auto& patch : patches) {
                if (!p.labels.count(patch.second)) throw std::runtime_error("Undefined label: " + patch.second);
                size_t target = p.labels[patch.second];
                p.bytecode[patch.first] = target & 0xFF;
                p.bytecode[patch.first+1] = (target >> 8) & 0xFF;
            }
        };
        apply(p.jump_patches);
        apply(p.try_patches);
    }
}

std::string Assembler::parse_string_literal(std::string_view sv) {
    if (sv.length() >= 2) sv = sv.substr(1, sv.length() - 2);
    std::string res; res.reserve(sv.length());
    for (size_t i = 0; i < sv.length(); ++i) {
        if (sv[i] == '\\' && i + 1 < sv.length()) {
            char next = sv[++i];
            if (next == 'n') res += '\n'; 
            else if (next == 'r') res += '\r';
            else if (next == 't') res += '\t';
            else if (next == '\\') res += '\\'; 
            else if (next == '"') res += '"';
            else res += next;
        } else res += sv[i];
    }
    return res;
}

std::vector<uint8_t> Assembler::serialize_binary() {
    std::vector<uint8_t> buffer;
    buffer.reserve(4096); 

    auto write_u8 = [&](uint8_t v) { buffer.push_back(v); };
    auto write_u32 = [&](uint32_t v) { 
        buffer.push_back(v & 0xFF); buffer.push_back((v >> 8) & 0xFF);
        buffer.push_back((v >> 16) & 0xFF); buffer.push_back((v >> 24) & 0xFF);
    };
    auto write_u64 = [&](uint64_t v) { for(int i=0; i<8; ++i) buffer.push_back((v >> (i*8)) & 0xFF); };
    auto write_f64 = [&](double v) { uint64_t bit; std::memcpy(&bit, &v, 8); write_u64(bit); };
    auto write_str = [&](const std::string& s) { 
        write_u32(s.size()); buffer.insert(buffer.end(), s.begin(), s.end()); 
    };

    write_u32(0x4D454F57); // Magic
    write_u32(2);          // Version 2 (CÓ Flags & Debug Info)

    if (proto_name_map_.count("main")) write_u32(proto_name_map_["main"]);
    else write_u32(0);
    write_u32(protos_.size()); 

    for (const auto& p : protos_) {
        write_u32(p.num_regs);
        write_u32(p.num_upvalues);
        
        // [V2 Only] Ghi byte Flags
        write_u8(static_cast<uint8_t>(p.flags));
        
        std::vector<Constant> write_consts = p.constants;
        write_consts.push_back({ConstType::STRING_T, 0, 0, p.name, 0});
        
        write_u32(write_consts.size() - 1); 
        write_u32(write_consts.size());     

        for (const auto& c : write_consts) {
            switch (c.type) {
                case ConstType::NULL_T: write_u8(0); break;
                case ConstType::INT_T:  write_u8(1); write_u64(c.val_i64); break;
                case ConstType::FLOAT_T:write_u8(2); write_f64(c.val_f64); break;
                case ConstType::STRING_T:write_u8(3); write_str(c.val_str); break;
                case ConstType::PROTO_REF_T: write_u8(4); write_u32(c.proto_index); break;
            }
        }
        write_u32(p.upvalues.size());
        for (const auto& u : p.upvalues) {
            write_u8(u.is_local ? 1 : 0);
            write_u32(u.index);
        }
        write_u32(p.bytecode.size());
        buffer.insert(buffer.end(), p.bytecode.begin(), p.bytecode.end());
        
        // [V2 Only] Ghi Debug Tables nếu cờ được bật
        if (has_flag(p.flags, ProtoFlags::HAS_DEBUG_INFO)) {
            write_u32(p.source_files.size());
            for (const auto& f : p.source_files) write_str(f);
            
            write_u32(p.lines.size());
            for (const auto& l : p.lines) {
                write_u32(l.offset);
                write_u32(l.line);
                write_u32(l.col);
                write_u32(l.file_idx);
            }
        }
    }
    return buffer;
}

void Assembler::optimize() {
    // Logic optimize
}

// void Assembler::optimize() {
//     std::cout << "[MASM] Starting Optimization Pass..." << std::endl;
//     int optimized_count = 0;

//     for (auto& proto : protos_) {
//         std::vector<uint8_t>& code = proto.bytecode;
//         size_t ip = 0;

//         while (ip < code.size()) {
//             meow::OpCode op = static_cast<meow::OpCode>(code[ip]);
//             size_t instr_size = get_instr_size(op); // GET_PROP = 55 bytes

//             if (op == meow::OpCode::GET_PROP) {
//                 size_t next_ip = ip + instr_size;

//                 if (next_ip < code.size()) {
//                     uint8_t next_op_raw = code[next_ip];
//                     meow::OpCode next_op = static_cast<meow::OpCode>(next_op_raw);

//                     // Decode GET_PROP: [OP] [Dst] [Obj] [Name] [Cache]
//                     uint16_t prop_dst = (uint16_t)code[ip+1] | ((uint16_t)code[ip+2] << 8);
//                     uint16_t obj_reg  = (uint16_t)code[ip+3] | ((uint16_t)code[ip+4] << 8);
//                     uint16_t name_idx = (uint16_t)code[ip+5] | ((uint16_t)code[ip+6] << 8);

//                     // ---------------------------------------------------------
//                     // TRƯỜNG HỢP 1: GET_PROP -> CALL (Chuẩn, 80 bytes)
//                     // ---------------------------------------------------------
//                     if (next_op == meow::OpCode::CALL) {
//                         size_t call_base = next_ip;
//                         // CALL: [OP] [Dst] [Fn] ...
//                         uint16_t call_dst = (uint16_t)code[call_base+1] | ((uint16_t)code[call_base+2] << 8);
//                         uint16_t fn_reg   = (uint16_t)code[call_base+3] | ((uint16_t)code[call_base+4] << 8);
//                         uint16_t arg_start= (uint16_t)code[call_base+5] | ((uint16_t)code[call_base+6] << 8);
//                         uint16_t argc     = (uint16_t)code[call_base+7] | ((uint16_t)code[call_base+8] << 8);

//                         if (prop_dst == fn_reg) {
//                             std::cout << "[MASM] MERGED (Direct): GET_PROP + CALL -> INVOKE at " << ip << "\n";
                            
//                             // Ghi đè INVOKE (80 bytes)
//                             size_t write = ip;
//                             code[write++] = static_cast<uint8_t>(meow::OpCode::INVOKE);
                            
//                             auto emit_u16 = [&](uint16_t v) { code[write++] = v & 0xFF; code[write++] = (v >> 8) & 0xFF; };
//                             emit_u16(call_dst); emit_u16(obj_reg); emit_u16(name_idx); emit_u16(arg_start); emit_u16(argc);
                            
//                             std::memset(&code[write], 0, 48); write += 48; // IC
//                             std::memset(&code[write], 0, 21); // Padding (80 - 11 - 48 = 21)
                            
//                             ip += 80; optimized_count++; continue;
//                         }
//                     }
//                     // ---------------------------------------------------------
//                     // TRƯỜNG HỢP 2: GET_PROP -> MOVE -> CALL (Có rác, 85 bytes)
//                     // ---------------------------------------------------------
//                     else if (next_op == meow::OpCode::MOVE) {
//                         size_t move_size = 5; // OP(1) + DST(2) + SRC(2)
//                         size_t call_ip = next_ip + move_size;

//                         if (call_ip < code.size() && static_cast<meow::OpCode>(code[call_ip]) == meow::OpCode::CALL) {
//                             // Decode MOVE: [OP] [Dst] [Src]
//                             uint16_t move_dst = (uint16_t)code[next_ip+1] | ((uint16_t)code[next_ip+2] << 8);
//                             uint16_t move_src = (uint16_t)code[next_ip+3] | ((uint16_t)code[next_ip+4] << 8);

//                             // Decode CALL
//                             uint16_t call_dst = (uint16_t)code[call_ip+1] | ((uint16_t)code[call_ip+2] << 8);
//                             uint16_t fn_reg   = (uint16_t)code[call_ip+3] | ((uint16_t)code[call_ip+4] << 8);
//                             uint16_t arg_start= (uint16_t)code[call_ip+5] | ((uint16_t)code[call_ip+6] << 8);
//                             uint16_t argc     = (uint16_t)code[call_ip+7] | ((uint16_t)code[call_ip+8] << 8);

//                             // Logic check: GET_PROP -> rA, MOVE rB, rA, CALL rB
//                             if (prop_dst == move_src && move_dst == fn_reg) {
//                                 std::cout << "[MASM] MERGED (Jump Move): GET_PROP + MOVE + CALL -> INVOKE at " << ip << "\n";

//                                 // 1. Ghi INVOKE (80 bytes)
//                                 size_t write = ip;
//                                 code[write++] = static_cast<uint8_t>(meow::OpCode::INVOKE);
                                
//                                 auto emit_u16 = [&](uint16_t v) { code[write++] = v & 0xFF; code[write++] = (v >> 8) & 0xFF; };
//                                 emit_u16(call_dst); emit_u16(obj_reg); emit_u16(name_idx); emit_u16(arg_start); emit_u16(argc);
                                
//                                 std::memset(&code[write], 0, 48); write += 48;
//                                 std::memset(&code[write], 0, 21); write += 21;

//                                 // 2. Xử lý 5 bytes thừa (MOVE cũ)
//                                 // Tổng block cũ là 55 + 5 + 25 = 85 bytes.
//                                 // INVOKE mới dùng 80 bytes. Còn dư 5 bytes cuối cùng.
//                                 // Ta ghi đè 5 bytes này bằng lệnh MOVE r0, r0 (No-Op) để giữ alignment.
                                
//                                 code[write++] = static_cast<uint8_t>(meow::OpCode::MOVE);
//                                 emit_u16(0); // Dst r0
//                                 emit_u16(0); // Src r0
                                
//                                 ip += 85; optimized_count++; continue;
//                             }
//                         }
//                     }
//                 }
//             }
//             ip += instr_size;
//         }
//     }
//     std::cout << "[MASM] Optimization finished. Total merged: " << optimized_count << std::endl;
// }

std::vector<uint8_t> Assembler::assemble() {
    while (!is_at_end()) {
        parse_statement();
    }
    link_proto_refs();
    patch_labels();
    // optimize();
    return serialize_binary();
}

void Assembler::assemble_to_file(const std::string& output_file) {
    auto buffer = assemble();
    std::ofstream out(output_file, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output file");
    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    out.close();
}

} // namespace meow::masm