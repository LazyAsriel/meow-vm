#pragma once
#include "common.h"
#include "lexer.h" 
#include <vector>
#include <iostream>
#include <unordered_map>

namespace meow::masm {

class Assembler {
public:
    explicit Assembler(Lexer& lexer) noexcept;

    [[nodiscard]] Status assemble();
    [[nodiscard]] Status assemble_to_file(const std::string& output_file);
    
    void write_binary(std::ostream& out);

private:
    Lexer& lexer_;
    Token current_token_;
    
    std::vector<Prototype> protos_;
    Prototype* curr_proto_ = nullptr;
    std::unordered_map<std::string, uint32_t> proto_name_map_;
    ProtoFlags global_flags_ = ProtoFlags::NONE;

    [[gnu::always_inline]] Token peek() const { return current_token_; }
    [[gnu::always_inline]] bool is_at_end() const { return current_token_.type == TokenType::END_OF_FILE; }
    
    Token advance(); 
    
    [[nodiscard]] Status consume(TokenType type, ErrorCode err, Token* out_token = nullptr);

    Status parse_statement();
    Status parse_func();
    Status parse_registers();
    Status parse_upvalues_decl();
    Status parse_upvalue_def();
    Status parse_const();
    Status parse_label();
    Status parse_instruction();
    Status parse_annotation(); 

    Status parse_arg(ArgType type, OpCode op);

    std::string parse_string_literal(std::string_view sv);
    Status link_proto_refs();
    Status patch_labels();

    inline void emit_byte(uint8_t b);
    inline void emit_u16(uint16_t v);
    inline void emit_u32(uint32_t v);
    inline void emit_u64(uint64_t v);
};

} // namespace meow::masm