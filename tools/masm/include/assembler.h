#pragma once
#include "common.h"
#include <vector>
#include <string>
#include <unordered_map>

class Assembler {
    const std::vector<Token>& tokens_;
    size_t current_ = 0;
    
    std::vector<Prototype> protos_;
    Prototype* curr_proto_ = nullptr;
    std::unordered_map<std::string, uint32_t> proto_name_map_;

    // [NEW] Bảng symbol cho biến Global
    // Map: Tên biến (String) -> Index (uint32_t)
    std::unordered_map<std::string, uint32_t> global_symbols_;

public:
    explicit Assembler(const std::vector<Token>& tokens);
    void assemble(const std::string& output_file);

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
    
    // Helpers
    int get_arity(OpCode op);
    uint32_t resolve_global(std::string_view name); // Hàm xử lý logic tìm/tạo index global

    // Emit bytecode
    void emit_byte(uint8_t b);
    void emit_u16(uint16_t v);
    void emit_u32(uint32_t v);
    void emit_u64(uint64_t v);

    // Finalize
    void link_proto_refs();
    void patch_labels();
    void write_binary(const std::string& filename);
    std::string parse_string_literal(std::string_view sv);
};