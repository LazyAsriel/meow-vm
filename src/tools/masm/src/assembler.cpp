#include <meow/masm/assembler.h>
#include <charconv> 
#include <bit>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace meow::masm {

Assembler::Assembler(Lexer& lexer) noexcept : lexer_(lexer) {
    protos_.reserve(8); 
    current_token_ = lexer_.next_token();
}

Token Assembler::advance() { 
    Token prev = current_token_;
    if (current_token_.type != TokenType::END_OF_FILE) {
        current_token_ = lexer_.next_token();
    }
    return prev;
}

Status Assembler::consume(TokenType type, ErrorCode err, Token* out_token) {
    if (peek().type == type) [[likely]] {
        Token t = advance();
        if (out_token) *out_token = t;
        return Status::ok();
    }
    if (peek().type == TokenType::UNKNOWN) {
        return Status::error(ErrorCode::UNEXPECTED_TOKEN, peek().line, peek().col);
    }
    Token bad = peek();
    return Status::error(err, bad.line, bad.col);
}

// --- Emitters ---

inline void Assembler::emit_byte(uint8_t b) { curr_proto_->bytecode.push_back(b); }
inline void Assembler::emit_u16(uint16_t v) { 
    emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); 
}
inline void Assembler::emit_u32(uint32_t v) { 
    emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); 
    emit_byte((v >> 16) & 0xFF); emit_byte((v >> 24) & 0xFF); 
}
inline void Assembler::emit_u64(uint64_t v) { 
    for(int i=0; i<8; ++i) emit_byte((v >> (i*8)) & 0xFF); 
}

// --- Parsers ---

[[gnu::hot]] Status Assembler::parse_statement() {
    Token tk = peek();
    switch (tk.type) {
        case TokenType::OPCODE:        return parse_instruction();
        case TokenType::LABEL_DEF:     return parse_label();
        case TokenType::DIR_CONST:     return parse_const();
        case TokenType::DIR_FUNC:      return parse_func();
        case TokenType::DIR_REGISTERS: return parse_registers();
        case TokenType::DIR_UPVALUES:  return parse_upvalues_decl();
        case TokenType::DIR_UPVALUE:   return parse_upvalue_def();
        case TokenType::ANNOTATION:    return parse_annotation(); 
        case TokenType::DIR_ENDFUNC:   advance(); curr_proto_ = nullptr; return Status::ok();
        case TokenType::END_OF_FILE:   return Status::ok();
        case TokenType::UNKNOWN:       return Status::error(ErrorCode::UNEXPECTED_TOKEN, tk.line, tk.col);
        default: 
            return Status::error(ErrorCode::UNEXPECTED_TOKEN, tk.line, tk.col);
    }
}

Status Assembler::parse_annotation() {
    Token ann = advance(); 
    std::string_view key = ann.lexeme;
    ProtoFlags* target = (curr_proto_) ? &curr_proto_->flags : &global_flags_;

    if (key == "debug") *target = *target | ProtoFlags::HAS_DEBUG_INFO;
    else if (key == "no_debug") *target = *target & (~ProtoFlags::HAS_DEBUG_INFO);
    else if (key == "vararg") *target = *target | ProtoFlags::IS_VARARG;
    else return Status::error(ErrorCode::UNKNOWN_ANNOTATION, ann.line, ann.col);
    
    return Status::ok();
}

Status Assembler::parse_func() {
    advance(); 
    Token name;
    MASM_CHECK(consume(TokenType::IDENTIFIER, ErrorCode::EXPECTED_FUNC_NAME, &name));
    
    std::string func_name(name.lexeme);
    if (func_name.starts_with("@")) func_name = func_name.substr(1);
    
    protos_.emplace_back();
    curr_proto_ = &protos_.back();
    curr_proto_->name = func_name;
    curr_proto_->flags = global_flags_;
    proto_name_map_[func_name] = static_cast<uint32_t>(protos_.size() - 1);
    
    return Status::ok();
}

Status Assembler::parse_registers() {
    if (!curr_proto_) [[unlikely]] return Status::error(ErrorCode::OUTSIDE_FUNC, peek().line, peek().col);
    advance();
    Token num;
    MASM_CHECK(consume(TokenType::NUMBER_INT, ErrorCode::EXPECTED_NUMBER, &num));
    std::from_chars(num.lexeme.data(), num.lexeme.data() + num.lexeme.size(), curr_proto_->num_regs);
    return Status::ok();
}

Status Assembler::parse_upvalues_decl() {
    if (!curr_proto_) [[unlikely]] return Status::error(ErrorCode::OUTSIDE_FUNC, peek().line, peek().col);
    advance();
    Token num;
    MASM_CHECK(consume(TokenType::NUMBER_INT, ErrorCode::EXPECTED_NUMBER, &num));
    std::from_chars(num.lexeme.data(), num.lexeme.data() + num.lexeme.size(), curr_proto_->num_upvalues);
    curr_proto_->upvalues.resize(curr_proto_->num_upvalues);
    return Status::ok();
}

Status Assembler::parse_upvalue_def() {
    if (!curr_proto_) [[unlikely]] return Status::error(ErrorCode::OUTSIDE_FUNC, peek().line, peek().col);
    advance();
    
    Token idx_tk, type_tk, slot_tk;
    MASM_CHECK(consume(TokenType::NUMBER_INT, ErrorCode::EXPECTED_NUMBER, &idx_tk));
    MASM_CHECK(consume(TokenType::IDENTIFIER, ErrorCode::EXPECTED_TYPE, &type_tk));
    MASM_CHECK(consume(TokenType::NUMBER_INT, ErrorCode::EXPECTED_SLOT, &slot_tk));

    uint32_t idx = 0, slot = 0;
    std::from_chars(idx_tk.lexeme.data(), idx_tk.lexeme.data() + idx_tk.lexeme.size(), idx);
    std::from_chars(slot_tk.lexeme.data(), slot_tk.lexeme.data() + slot_tk.lexeme.size(), slot);
    
    if (idx >= curr_proto_->upvalues.size()) [[unlikely]]
        return Status::error(ErrorCode::INDEX_OUT_OF_BOUNDS, idx_tk.line, idx_tk.col);

    curr_proto_->upvalues[idx] = { (type_tk.lexeme == "local"), slot };
    return Status::ok();
}

Status Assembler::parse_const() {
    if (!curr_proto_) [[unlikely]] return Status::error(ErrorCode::OUTSIDE_FUNC, peek().line, peek().col);
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
        std::from_chars(tk.lexeme.data(), tk.lexeme.data() + tk.lexeme.size(), c.val_f64);
        advance();
    } else if (tk.type == TokenType::IDENTIFIER) {
        if (tk.lexeme == "null") { c.type = ConstType::NULL_T; advance(); }
        else if (tk.lexeme == "true") { c.type = ConstType::INT_T; c.val_i64 = 1; advance(); } 
        else if (tk.lexeme == "false") { c.type = ConstType::INT_T; c.val_i64 = 0; advance(); }
        else if (tk.lexeme.starts_with("@")) {
            c.type = ConstType::PROTO_REF_T;
            c.val_str = tk.lexeme.substr(1);
            advance();
        } else return Status::error(ErrorCode::UNKNOWN_CONSTANT, tk.line, tk.col);
    } else return Status::error(ErrorCode::UNKNOWN_CONSTANT, tk.line, tk.col);
    
    curr_proto_->constants.push_back(c);
    return Status::ok();
}

Status Assembler::parse_label() {
    Token lbl = advance();
    std::string_view lbl_name = lbl.lexeme; 
    
    if (curr_proto_->labels.count(lbl_name)) 
        return Status::error(ErrorCode::LABEL_REDEFINITION, lbl.line, lbl.col);
        
    curr_proto_->labels[lbl_name] = curr_proto_->bytecode.size();
    return Status::ok();
}

[[gnu::hot]] Status Assembler::parse_instruction() {
    if (!curr_proto_) [[unlikely]] return Status::error(ErrorCode::OUTSIDE_FUNC, peek().line, peek().col);
    
    uint32_t current_offset = static_cast<uint32_t>(curr_proto_->bytecode.size());
    Token op_tok = advance();
    
    // Lấy OpCode trực tiếp từ payload
    meow::OpCode op = static_cast<meow::OpCode>(op_tok.payload);
    emit_byte(static_cast<uint8_t>(op));

    auto parse_u16_arg = [&](Token& t) -> Status {
        return consume(TokenType::NUMBER_INT, ErrorCode::EXPECTED_U16, &t);
    };

    // Lấy metadata
    const auto info = meow::get_op_info(op);

    switch (op) {
        // --- Các lệnh có cấu trúc đặc biệt (Custom Parser) ---
        case meow::OpCode::LOAD_INT: {
            Token dst, val_tk;
            MASM_CHECK(parse_u16_arg(dst));
            MASM_CHECK(consume(TokenType::NUMBER_INT, ErrorCode::EXPECTED_INT64, &val_tk));
            uint16_t reg; std::from_chars(dst.lexeme.data(), dst.lexeme.data() + dst.lexeme.size(), reg); emit_u16(reg);
            int64_t val; std::from_chars(val_tk.lexeme.data(), val_tk.lexeme.data() + val_tk.lexeme.size(), val); emit_u64(std::bit_cast<uint64_t>(val));
            break;
        }
        case meow::OpCode::LOAD_FLOAT: {
            Token dst, val_tk;
            MASM_CHECK(parse_u16_arg(dst));
            MASM_CHECK(consume(TokenType::NUMBER_FLOAT, ErrorCode::EXPECTED_DOUBLE, &val_tk));
            uint16_t reg; std::from_chars(dst.lexeme.data(), dst.lexeme.data() + dst.lexeme.size(), reg); emit_u16(reg);
            double val; std::from_chars(val_tk.lexeme.data(), val_tk.lexeme.data() + val_tk.lexeme.size(), val); emit_u64(std::bit_cast<uint64_t>(val));
            break;
        }
        case meow::OpCode::JUMP: case meow::OpCode::SETUP_TRY: {
             if (peek().type == TokenType::IDENTIFIER) {
                Token target = advance();
                curr_proto_->try_patches.push_back({curr_proto_->bytecode.size(), target.lexeme});
                emit_u16(0xFFFF);
                if (op == meow::OpCode::SETUP_TRY) { // setup try có thêm reg
                    Token r; MASM_CHECK(parse_u16_arg(r));
                    uint16_t reg; std::from_chars(r.lexeme.data(), r.lexeme.data()+r.lexeme.size(), reg); emit_u16(reg);
                }
            } else {
                Token off; MASM_CHECK(parse_u16_arg(off));
                uint16_t val; std::from_chars(off.lexeme.data(), off.lexeme.data()+off.lexeme.size(), val); emit_u16(val);
                if (op == meow::OpCode::SETUP_TRY) {
                    Token r; MASM_CHECK(parse_u16_arg(r));
                    uint16_t reg; std::from_chars(r.lexeme.data(), r.lexeme.data()+r.lexeme.size(), reg); emit_u16(reg);
                }
            }
            break;
        }
        case meow::OpCode::JUMP_IF_FALSE: case meow::OpCode::JUMP_IF_TRUE: {
            Token r; MASM_CHECK(parse_u16_arg(r));
            uint16_t reg; std::from_chars(r.lexeme.data(), r.lexeme.data()+r.lexeme.size(), reg); emit_u16(reg);
            if (peek().type == TokenType::IDENTIFIER) {
                Token target = advance();
                curr_proto_->jump_patches.push_back({curr_proto_->bytecode.size(), target.lexeme});
                emit_u16(0xFFFF);
            } else {
                 Token off; MASM_CHECK(parse_u16_arg(off));
                 uint16_t val; std::from_chars(off.lexeme.data(), off.lexeme.data()+off.lexeme.size(), val); emit_u16(val);
            }
            break;
        }
        
        // --- Xử lý Generic cho tất cả các lệnh chuẩn ---
        default: {
            // Tự động parse số lượng register dựa trên arity
            for(int i = 0; i < info.arity; ++i) {
                Token t; MASM_CHECK(parse_u16_arg(t));
                uint16_t v; std::from_chars(t.lexeme.data(), t.lexeme.data()+t.lexeme.size(), v);
                emit_u16(v);
            }

            // Xử lý Padding (Code cũ có padding cho CALL, INVOKE, PROP)
            // Tính số byte đã ghi: arity * 2. 
            // Số byte cần ghi thêm = operand_bytes - (arity * 2).
            int written_bytes = info.arity * 2;
            int padding_bytes = info.operand_bytes - written_bytes;

            if (padding_bytes > 0) {
                for (int k = 0; k < padding_bytes; ++k) emit_byte(0);
            }
            break;
        }
    }

    if (peek().type == TokenType::DEBUG_INFO) {
        Token dbg = advance();
        std::string s(dbg.lexeme);
        std::stringstream ss(s);
        std::string file_part;
        char c;
        if (ss >> std::ws && ss.peek() == '"') ss >> std::quoted(file_part);
        uint32_t line = 0, col = 0;
        ss >> line >> c >> col; 
        
        uint32_t file_idx = curr_proto_->add_file(file_part);
        curr_proto_->lines.push_back({current_offset, line, col, file_idx});
    }
    return Status::ok();
}

Status Assembler::link_proto_refs() {
    for (auto& p : protos_) {
        for (auto& c : p.constants) {
            if (c.type == ConstType::PROTO_REF_T) {
                if (proto_name_map_.count(c.val_str)) c.proto_index = proto_name_map_[c.val_str];
                else return Status::error(ErrorCode::UNDEFINED_PROTO_REF, 0, 0);
            }
        }
    }
    return Status::ok();
}

Status Assembler::patch_labels() {
    for (auto& p : protos_) {
        auto apply = [&](auto& patches) -> Status {
            for (auto& patch : patches) {
                if (!p.labels.count(patch.second)) 
                    return Status::error(ErrorCode::UNDEFINED_LABEL, 0, 0);
                size_t target = p.labels[patch.second];
                p.bytecode[patch.first] = target & 0xFF;
                p.bytecode[patch.first+1] = (target >> 8) & 0xFF;
            }
            return Status::ok();
        };
        MASM_CHECK(apply(p.jump_patches));
        MASM_CHECK(apply(p.try_patches));
    }
    return Status::ok();
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

void Assembler::write_binary(std::ostream& out) {
    auto write_u8 = [&](uint8_t v) { out.put(static_cast<char>(v)); };
    auto write_u32 = [&](uint32_t v) { 
        char buf[4] = {
            static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
            static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)
        };
        out.write(buf, 4);
    };
    auto write_u64 = [&](uint64_t v) { 
        char buf[8];
        for(int i=0; i<8; ++i) buf[i] = static_cast<char>((v >> (i*8)) & 0xFF);
        out.write(buf, 8);
    };
    auto write_f64 = [&](double v) { 
        uint64_t bit = std::bit_cast<uint64_t>(v); 
        write_u64(bit); 
    };
    auto write_str = [&](const std::string& s) { 
        write_u32(static_cast<uint32_t>(s.size())); 
        out.write(s.data(), s.size());
    };

    write_u32(0x4D454F57); 
    write_u32(2);          

    if (proto_name_map_.count("main")) write_u32(proto_name_map_["main"]);
    else write_u32(0);
    write_u32(static_cast<uint32_t>(protos_.size())); 

    for (const auto& p : protos_) {
        write_u32(p.num_regs);
        write_u32(p.num_upvalues);
        write_u8(static_cast<uint8_t>(p.flags));
        
        size_t const_count = p.constants.size();
        write_u32(static_cast<uint32_t>(const_count));     
        write_u32(static_cast<uint32_t>(const_count + 1)); 

        for (const auto& c : p.constants) {
            switch (c.type) {
                case ConstType::NULL_T: write_u8(0); break;
                case ConstType::INT_T:  write_u8(1); write_u64(c.val_i64); break;
                case ConstType::FLOAT_T:write_u8(2); write_f64(c.val_f64); break;
                case ConstType::STRING_T:write_u8(3); write_str(c.val_str); break;
                case ConstType::PROTO_REF_T: write_u8(4); write_u32(c.proto_index); break;
            }
        }
        write_u8(3); write_str(p.name); // Const name

        write_u32(static_cast<uint32_t>(p.upvalues.size()));
        for (const auto& u : p.upvalues) {
            write_u8(u.is_local ? 1 : 0);
            write_u32(u.index);
        }
        
        write_u32(static_cast<uint32_t>(p.bytecode.size()));
        if (!p.bytecode.empty()) {
            out.write(reinterpret_cast<const char*>(p.bytecode.data()), p.bytecode.size());
        }
        
        if (has_flag(p.flags, ProtoFlags::HAS_DEBUG_INFO)) {
            write_u32(static_cast<uint32_t>(p.source_files.size()));
            for (const auto& f : p.source_files) write_str(f);
            write_u32(static_cast<uint32_t>(p.lines.size()));
            for (const auto& l : p.lines) {
                write_u32(l.offset);
                write_u32(l.line);
                write_u32(l.col);
                write_u32(l.file_idx);
            }
        }
    }
}

Status Assembler::assemble() {
    while (!is_at_end()) {
        MASM_CHECK(parse_statement());
    }
    MASM_CHECK(link_proto_refs());
    MASM_CHECK(patch_labels());
    return Status::ok();
}

Status Assembler::assemble_to_file(const std::string& output_file) {
    MASM_CHECK(assemble());
    
    std::ofstream out(output_file, std::ios::binary);
    if (!out) [[unlikely]] return Status::error(ErrorCode::FILE_OPEN_FAILED, 0, 0);
    
    char buffer[65536];
    out.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
    
    write_binary(out);
    
    if (!out) return Status::error(ErrorCode::WRITE_ERROR, 0, 0);
    return Status::ok();
}

} // namespace meow::masm