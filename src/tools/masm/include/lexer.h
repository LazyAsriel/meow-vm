#pragma once
#include "common.h"
#include <vector>

class Lexer {
    std::string_view src_;
    size_t pos_ = 0;
    size_t line_ = 1;

public:
    explicit Lexer(std::string_view src) : src_(src) {}
    std::vector<Token> tokenize();

private:
    bool is_at_end() const;
    char peek(int offset = 0) const;
    char advance();
    
    Token scan_directive();
    Token scan_string();
    Token scan_number();
    Token scan_identifier();
};