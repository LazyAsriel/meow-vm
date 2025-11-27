#include "assembler.h"
#include <iostream>
#include <fstream>
#include <charconv> // C++17/23
#include <bit>
#include <cstring>
#include <stdexcept>

Assembler::Assembler(const std::vector<Token>& tokens) : tokens_(tokens) {}

Token Assembler::peek() const { return tokens_[current_]; }
Token Assembler::previous() const { return tokens_[current_ - 1]; }
bool Assembler::is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
Token Assembler::advance() { if (!is_at_end()) current_++; return previous(); }

Token Assembler::consume(TokenType type, const std::string& msg) {
    if (peek().type == type) return advance();
    throw std::runtime_error(msg + " (Line " + std::to_string(peek().line) + ")");
}

// [NEW] Logic quan trọng: Biến đổi tên Global thành Index
uint32_t Assembler::resolve_global(std::string_view name) {
    std::string s(name); // Chuyển view sang string để lưu key
    if (global_symbols_.count(s)) {
        return global_symbols_[s];
    }
    // Nếu chưa có, tạo index mới
    uint32_t index = static_cast<uint32_t>(global_symbols_.size());
    global_symbols_[s] = index;
    return index;
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
        case TokenType::IDENTIFIER:    throw std::runtime_error("Unexpected identifier: " + std::string(tk.lexeme));
        default: throw std::runtime_error("Unexpected token: " + std::string(tk.lexeme));
    }
}

void Assembler::parse_func() {
    advance(); 
    Token name = consume(TokenType::IDENTIFIER, "Expected func name");
    std::string func_name(name.lexeme);
    if (func_name.starts_with("@")) func_name = func_name.substr(1);
    protos_.push_back(Prototype{.name = func_name});
    curr_proto_ = &protos_.back();
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
            // C++23: std::from_chars (rất nhanh)
            std::from_chars(sv.data(), sv.data() + sv.size(), c.val_i64);
        }
        advance();
    } else if (tk.type == TokenType::NUMBER_FLOAT) {
        c.type = ConstType::FLOAT_T;
        c.val_f64 = std::stod(std::string(tk.lexeme));
        advance();
    } else if (tk.type == TokenType::IDENTIFIER) {
        if (tk.lexeme == "null") { c.type = ConstType::NULL_T; advance(); }
        else if (tk.lexeme.starts_with("@")) {
            c.type = ConstType::PROTO_REF_T;
            c.val_str = tk.lexeme.substr(1);
            advance();
        } else throw std::runtime_error("Unknown constant identifier");
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
    Token op_tok = advance();
    OpCode op = OP_MAP[op_tok.lexeme];
    emit_byte(static_cast<uint8_t>(op));

    auto parse_u16 = [&]() {
        Token t = consume(TokenType::NUMBER_INT, "Expected u16");
        emit_u16(static_cast<uint16_t>(std::stoi(std::string(t.lexeme))));
    };

    // [OPTIMIZATION] Xử lý riêng cho Global: Dùng Index thay vì Constant Pool
    if (op == OpCode::GET_GLOBAL || op == OpCode::SET_GLOBAL) {
        Token t = consume(TokenType::IDENTIFIER, "Expected global variable name");
        // Giải quyết tên -> index ngay tại compile time
        uint32_t global_idx = resolve_global(t.lexeme);
        // Emit u32 index (để hỗ trợ > 65k globals nếu cần, hoặc dùng u16 cũng được, ở đây dùng u32 cho an toàn)
        emit_u32(global_idx);
        return;
    }

    // Các instruction khác
    switch (op) {
        case OpCode::LOAD_INT: {
            parse_u16(); // dst
            Token t = consume(TokenType::NUMBER_INT, "Expected int64");
            int64_t val;
            std::from_chars(t.lexeme.data(), t.lexeme.data() + t.lexeme.size(), val);
            emit_u64(std::bit_cast<uint64_t>(val));
            break;
        }
        case OpCode::LOAD_FLOAT: {
            parse_u16(); // dst
            Token t = consume(TokenType::NUMBER_FLOAT, "Expected double");
            double val = std::stod(std::string(t.lexeme));
            emit_u64(std::bit_cast<uint64_t>(val));
            break;
        }
        case OpCode::JUMP: case OpCode::SETUP_TRY: {
            if (peek().type == TokenType::IDENTIFIER) {
                Token target = advance();
                curr_proto_->try_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
                emit_u16(0xFFFF);
            } else parse_u16();
            break;
        }
        case OpCode::JUMP_IF_FALSE: case OpCode::JUMP_IF_TRUE: {
            parse_u16(); // reg
            if (peek().type == TokenType::IDENTIFIER) {
                Token target = advance();
                curr_proto_->jump_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
                emit_u16(0xFFFF);
            } else parse_u16();
            break;
        }
        default: {
            int args = get_arity(op);
            for(int i=0; i<args; ++i) parse_u16();
            break;
        }
    }
}

int Assembler::get_arity(OpCode op) {
    // Helper đơn giản
    switch (op) {
        case OpCode::CLOSE_UPVALUES: case OpCode::IMPORT_ALL: case OpCode::THROW: case OpCode::RETURN: return 1;
        case OpCode::LOAD_CONST: case OpCode::MOVE: case OpCode::NEG: case OpCode::NOT:
        case OpCode::BIT_NOT: case OpCode::GET_UPVALUE: case OpCode::SET_UPVALUE: case OpCode::CLOSURE:
        case OpCode::NEW_CLASS: case OpCode::NEW_INSTANCE: case OpCode::IMPORT_MODULE:
        case OpCode::EXPORT: case OpCode::GET_KEYS: case OpCode::GET_VALUES:
        case OpCode::GET_SUPER: case OpCode::GET_EXPORT: return 2;
        // GET/SET_GLOBAL đã được handle riêng ở trên
        case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::DIV:
        case OpCode::MOD: case OpCode::POW: case OpCode::EQ: case OpCode::NEQ:
        case OpCode::GT: case OpCode::GE: case OpCode::LT: case OpCode::LE:
        case OpCode::BIT_AND: case OpCode::BIT_OR: case OpCode::BIT_XOR:
        case OpCode::LSHIFT: case OpCode::RSHIFT: 
        case OpCode::NEW_ARRAY: case OpCode::NEW_HASH: case OpCode::GET_INDEX:
        case OpCode::SET_INDEX: case OpCode::GET_PROP: case OpCode::SET_PROP:
        case OpCode::SET_METHOD: case OpCode::CALL_VOID:
            return 3;
        case OpCode::CALL: return 4;
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

void Assembler::write_binary(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output file");

    auto write_u8 = [&](uint8_t v) { out.write((char*)&v, 1); };
    auto write_u32 = [&](uint32_t v) { out.write((char*)&v, 4); };
    auto write_u64 = [&](uint64_t v) { out.write((char*)&v, 8); };
    auto write_f64 = [&](double v) { uint64_t bit; std::memcpy(&bit, &v, 8); out.write((char*)&bit, 8); };
    auto write_str = [&](const std::string& s) { write_u32(s.size()); out.write(s.data(), s.size()); };

    write_u32(0x4D454F57); // Magic
    write_u32(1);          // Version

    // [NEW] Ghi số lượng biến Global vào Header để VM allocate vector
    write_u32(static_cast<uint32_t>(global_symbols_.size()));
    std::cout << "[Info] Total Globals: " << global_symbols_.size() << "\n";

    if (proto_name_map_.count("main")) write_u32(proto_name_map_["main"]);
    else write_u32(0);

    write_u32(protos_.size()); // Proto Count

    for (const auto& p : protos_) {
        write_u32(p.num_regs);
        write_u32(p.num_upvalues);
        
        // Ghi hằng số (Thêm tên func vào cuối pool để debug)
        std::vector<Constant> write_consts = p.constants;
        write_consts.push_back({ConstType::STRING_T, 0, 0, p.name, 0});
        
        write_u32(write_consts.size() - 1); // Name index
        write_u32(write_consts.size());     // Pool size

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
        out.write((char*)p.bytecode.data(), p.bytecode.size());
    }
    out.close();
    std::cout << "Assembled: " << filename << "\n";
}

std::string Assembler::parse_string_literal(std::string_view sv) {
    if (sv.length() >= 2) sv = sv.substr(1, sv.length() - 2);
    std::string res; res.reserve(sv.length());
    for (size_t i = 0; i < sv.length(); ++i) {
        if (sv[i] == '\\' && i + 1 < sv.length()) {
            char next = sv[++i];
            if (next == 'n') res += '\n'; else if (next == 't') res += '\t';
            else if (next == '\\') res += '\\'; else if (next == '"') res += '"';
            else res += next;
        } else res += sv[i];
    }
    return res;
}