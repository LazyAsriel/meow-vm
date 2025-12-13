#include <meow/masm/lexer.h>
#include <meow/compiler/op_codes.h> // Cần để lấy OpCode enum
#include <cctype>
#include <string_view>
#include <utility> // std::index_sequence

namespace meow::masm {

std::unordered_map<std::string_view, meow::OpCode> OP_MAP;

// ============================================================================
// MAGIC ENUM (Embedded Version)
// Tự động map OpCode::ADD -> chuỗi "ADD" ngay tại thời điểm biên dịch
// ============================================================================
namespace {
    template <auto V>
    consteval std::string_view get_raw_name() {
#if defined(__clang__) || defined(__GNUC__)
        return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
        return __FUNCSIG__;
#else
        return "";
#endif
    }

    template <auto V>
    consteval std::string_view get_enum_name() {
        constexpr std::string_view raw = get_raw_name<V>();
        
#if defined(__clang__) || defined(__GNUC__)
        constexpr auto end_pos = raw.size() - 1;
        constexpr auto last_colon = raw.find_last_of(':', end_pos);
        if (last_colon == std::string_view::npos) return ""; 
        return raw.substr(last_colon + 1, end_pos - (last_colon + 1));
#else
        return "UNKNOWN";
#endif
    }
    template <size_t... Is>
    void build_map_impl(std::index_sequence<Is...>) {
        (..., (OP_MAP[get_enum_name<static_cast<meow::OpCode>(Is)>()] = static_cast<meow::OpCode>(Is)));
    }
}

void init_op_map() {
    if (!OP_MAP.empty()) return;

    constexpr size_t Count = static_cast<size_t>(meow::OpCode::TOTAL_OPCODES);
    build_map_impl(std::make_index_sequence<Count>{});

    OP_MAP.erase("__BEGIN_OPERATOR__");
    OP_MAP.erase("__END_OPERATOR__");
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

} // namespace meow::masm