#pragma once
#include "common.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace meow::masm {

class Assembler {
    const std::vector<Token>& tokens_;
    size_t current_ = 0;
    
    std::vector<Prototype> protos_;
    Prototype* curr_proto_ = nullptr;
    std::unordered_map<std::string, uint32_t> proto_name_map_;

    // --- State Management ---
    // Trạng thái cờ toàn cục (áp dụng cho các hàm mới tạo)
    ProtoFlags global_flags_ = ProtoFlags::NONE;

public:
    explicit Assembler(const std::vector<Token>& tokens);

    std::vector<uint8_t> assemble();
    void assemble_to_file(const std::string& output_file);
    static int get_arity(meow::OpCode op);

private:
    Token peek() const;
    Token previous() const;
    bool is_at_end() const;
    Token advance();
    Token consume(TokenType type, const std::string& msg);

    // Parsing
    void parse_statement();
    void parse_func();
    void parse_registers();
    void parse_upvalues_decl();
    void parse_upvalue_def();
    void parse_const();
    void parse_label();
    void parse_instruction();
    
    // --- Annotation Handler ---
    void parse_annotation(); 

    void optimize();
    std::string parse_string_literal(std::string_view sv);

    // Emit bytecode helpers
    void emit_byte(uint8_t b);
    void emit_u16(uint16_t v);
    void emit_u32(uint32_t v);
    void emit_u64(uint64_t v);

    // Finalize
    void link_proto_refs();
    void patch_labels();
    std::vector<uint8_t> serialize_binary();
};

} // namespace meow::masm