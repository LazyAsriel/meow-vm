#include "lexer.h"
#include <cctype>

std::unordered_map<std::string_view, OpCode> OP_MAP;

void init_op_map() {
    if (!OP_MAP.empty()) return;
    #define O(x) OP_MAP[#x] = OpCode::x;
    O(LOAD_CONST) O(LOAD_NULL) O(LOAD_TRUE) O(LOAD_FALSE) O(LOAD_INT) O(LOAD_FLOAT) O(MOVE)
    O(ADD) O(SUB) O(MUL) O(DIV) O(MOD) O(POW)
    O(EQ) O(NEQ) O(GT) O(GE) O(LT) O(LE)
    O(NEG) O(NOT)
    O(GET_GLOBAL) O(SET_GLOBAL) O(GET_UPVALUE) O(SET_UPVALUE) O(CLOSURE) O(CLOSE_UPVALUES)
    O(JUMP) O(JUMP_IF_FALSE) O(JUMP_IF_TRUE)
    O(CALL) O(CALL_VOID) O(RETURN) O(HALT)
    O(NEW_ARRAY) O(NEW_HASH) O(GET_INDEX) O(SET_INDEX) O(GET_KEYS) O(GET_VALUES)
    O(NEW_CLASS) O(NEW_INSTANCE) O(GET_PROP) O(SET_PROP) O(SET_METHOD) O(INHERIT) O(GET_SUPER)
    O(BIT_AND) O(BIT_OR) O(BIT_XOR) O(BIT_NOT) O(LSHIFT) O(RSHIFT)
    O(THROW) O(SETUP_TRY) O(POP_TRY)
    O(IMPORT_MODULE) O(EXPORT) O(GET_EXPORT) O(IMPORT_ALL)
    #undef O
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!is_at_end()) {
        char c = peek();
        if (isspace(c)) {
            if (c == '\n') line_++;
            advance();
            continue;
        }
        if (c == '#') {
            while (peek() != '\n' && !is_at_end()) advance();
            continue;
        }
        if (c == '.') { tokens.push_back(scan_directive()); continue; }
        if (c == '"' || c == '\'') { tokens.push_back(scan_string()); continue; }
        if (isdigit(c) || (c == '-' && isdigit(peek(1)))) { tokens.push_back(scan_number()); continue; }
        if (isalpha(c) || c == '_' || c == '@') { tokens.push_back(scan_identifier()); continue; }
        advance();
    }
    tokens.push_back({TokenType::END_OF_FILE, "", line_});
    return tokens;
}

bool Lexer::is_at_end() const { return pos_ >= src_.size(); }
char Lexer::peek(int offset) const { 
    if (pos_ + offset >= src_.size()) return '\0';
    return src_[pos_ + offset]; 
}
char Lexer::advance() { return src_[pos_++]; }

Token Lexer::scan_directive() {
    size_t start = pos_;
    advance(); 
    while (isalnum(peek()) || peek() == '_') advance();
    std::string_view text = src_.substr(start, pos_ - start);
    
    TokenType type = TokenType::UNKNOWN;
    if (text == ".func") type = TokenType::DIR_FUNC;
    else if (text == ".endfunc") type = TokenType::DIR_ENDFUNC;
    else if (text == ".registers") type = TokenType::DIR_REGISTERS;
    else if (text == ".upvalues") type = TokenType::DIR_UPVALUES;
    else if (text == ".upvalue") type = TokenType::DIR_UPVALUE;
    else if (text == ".const") type = TokenType::DIR_CONST;
    
    return {type, text, line_};
}

Token Lexer::scan_string() {
    char quote = advance();
    size_t start = pos_ - 1; 
    while (peek() != quote && !is_at_end()) {
        if (peek() == '\\') advance();
        advance();
    }
    if (!is_at_end()) advance();
    return {TokenType::STRING, src_.substr(start, pos_ - start), line_};
}

Token Lexer::scan_number() {
    size_t start = pos_;
    if (peek() == '-') advance();
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); advance();
        while (isxdigit(peek())) advance();
        return {TokenType::NUMBER_INT, src_.substr(start, pos_ - start), line_};
    }
    bool is_float = false;
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek(1))) {
        is_float = true;
        advance();
        while (isdigit(peek())) advance();
    }
    return {is_float ? TokenType::NUMBER_FLOAT : TokenType::NUMBER_INT, src_.substr(start, pos_ - start), line_};
}

Token Lexer::scan_identifier() {
    size_t start = pos_;
    while (isalnum(peek()) || peek() == '_' || peek() == '@') advance();
    
    if (peek() == ':') {
        advance(); 
        return {TokenType::LABEL_DEF, src_.substr(start, pos_ - start - 1), line_};
    }
    std::string_view text = src_.substr(start, pos_ - start);
    if (OP_MAP.count(text)) return {TokenType::OPCODE, text, line_};
    return {TokenType::IDENTIFIER, text, line_};
}