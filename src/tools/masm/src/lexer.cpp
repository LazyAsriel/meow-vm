#include <meow/masm/lexer.h>
#include <meow/bytecode/op_codes.h>
#include <meow_enum.h> 
#include <cstring>
#include <array>

namespace meow::masm {

std::unordered_map<std::string_view, meow::OpCode> OP_MAP;

[[gnu::cold]] void init_op_map() {
    if (!OP_MAP.empty()) [[likely]] return;
    OP_MAP.reserve(128); 
    for (auto op : meow::enum_values<meow::OpCode>()) {
        std::string_view name = meow::enum_name(op);
        if (name.empty() || name == "TOTAL_OPCODES") continue;
        OP_MAP[name] = op;
    }
}

enum CharType : uint8_t {
    CT_NONE=0, CT_SPACE=1, CT_ALPHA=2, CT_DIGIT=4, CT_IDENT=8, CT_QUOTE=16, CT_NL=32
};

static constexpr std::array<uint8_t, 256> CHAR_TABLE = []() {
    std::array<uint8_t, 256> table = {0};
    table[' '] = CT_SPACE; table['\t'] = CT_SPACE; table['\r'] = CT_SPACE;
    table['\n'] = CT_NL;
    for (int i='0'; i<='9'; ++i) table[i] |= CT_DIGIT | CT_IDENT;
    for (int i='a'; i<='z'; ++i) table[i] |= CT_ALPHA | CT_IDENT;
    for (int i='A'; i<='Z'; ++i) table[i] |= CT_ALPHA | CT_IDENT;
    table['_'] |= CT_ALPHA | CT_IDENT;
    table['@'] |= CT_ALPHA | CT_IDENT; 
    table['/'] |= CT_IDENT; table['-'] |= CT_IDENT; table['.'] |= CT_IDENT;
    table['"'] |= CT_QUOTE; table['\'']|= CT_QUOTE;
    return table;
}();

Lexer::Lexer(std::string_view src) noexcept 
    : src_(src), cursor_(src.data()), end_(src.data() + src.size()), line_start_(src.data()) 
{
    if (OP_MAP.empty()) [[unlikely]] init_op_map();
}

Token Lexer::next_token() {
    const auto* table = CHAR_TABLE.data();

    while (cursor_ < end_) [[likely]] {
        uint8_t type = table[static_cast<uint8_t>(*cursor_)];

        if (type & CT_SPACE) { cursor_++; continue; }

        if (type & CT_NL) {
            line_++;
            cursor_++;
            line_start_ = cursor_;
            continue;
        }

        if (type & CT_ALPHA) {
            const char* start = cursor_;
            do { cursor_++; } while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_IDENT));
            
            if (cursor_ < end_ && *cursor_ == ':') {
                uint32_t col = static_cast<uint32_t>(start - line_start_ + 1);
                Token t{TokenType::LABEL_DEF, {start, static_cast<size_t>(cursor_ - start)}, line_, col, 0};
                cursor_++;
                return t;
            }

            std::string_view text(start, cursor_ - start);
            uint16_t payload = 0;
            TokenType tk_type = TokenType::IDENTIFIER;

            if (text[0] >= 'A' && text[0] <= 'Z') {
                if (auto it = OP_MAP.find(text); it != OP_MAP.end()) {
                    tk_type = TokenType::OPCODE;
                    payload = static_cast<uint16_t>(it->second);
                }
            }
            return {tk_type, text, line_, static_cast<uint32_t>(start - line_start_ + 1), payload};
        }

        if (*cursor_ == '.') {
            const char* start = cursor_;
            cursor_++;
            while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_IDENT)) cursor_++;
            
            size_t len = cursor_ - start;
            TokenType tk_type = TokenType::IDENTIFIER;
            if (len > 4) {
                 switch (len) {
                    case 5: if (std::memcmp(start, ".func", 5)==0) tk_type = TokenType::DIR_FUNC; break;
                    case 6: if (std::memcmp(start, ".const", 6)==0) tk_type = TokenType::DIR_CONST; break;
                    case 8: if (std::memcmp(start, ".endfunc", 8)==0) tk_type = TokenType::DIR_ENDFUNC;
                            else if (std::memcmp(start, ".upvalue", 8)==0) tk_type = TokenType::DIR_UPVALUE; break;
                    case 9: if (std::memcmp(start, ".upvalues", 9)==0) tk_type = TokenType::DIR_UPVALUES; break;
                    case 10: if (std::memcmp(start, ".registers", 10)==0) tk_type = TokenType::DIR_REGISTERS; break;
                 }
            }
            return {tk_type, {start, len}, line_, static_cast<uint32_t>(start - line_start_ + 1), 0};
        }

        if (type & CT_DIGIT) {
            const char* start = cursor_;
            if (*cursor_ == '0' && cursor_ + 1 < end_ && (*(cursor_+1) | 32) == 'x') {
                cursor_ += 2;
                while (cursor_ < end_) {
                    char c = *cursor_;
                    if ((c >= '0' && c <= '9') || ((c|32) >= 'a' && (c|32) <= 'f')) cursor_++;
                    else break;
                }
                return {TokenType::NUMBER_INT, {start, static_cast<size_t>(cursor_ - start)}, line_, static_cast<uint32_t>(start - line_start_ + 1), 0};
            }
            
            bool is_float = false;
            while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_DIGIT)) cursor_++;
            if (cursor_ < end_ && *cursor_ == '.') {
                is_float = true;
                cursor_++;
                while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_DIGIT)) cursor_++;
            }
            return {is_float ? TokenType::NUMBER_FLOAT : TokenType::NUMBER_INT, {start, static_cast<size_t>(cursor_ - start)}, line_, static_cast<uint32_t>(start - line_start_ + 1), 0};
        }

        if (*cursor_ == '-') {
             if (cursor_ + 1 < end_ && (table[static_cast<uint8_t>(*(cursor_+1))] & CT_DIGIT)) {
                 const char* start = cursor_;
                 cursor_++; 
                 bool is_float = false;
                 while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_DIGIT)) cursor_++;
                 if (cursor_ < end_ && *cursor_ == '.') {
                    is_float = true;
                    cursor_++;
                    while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_DIGIT)) cursor_++;
                 }
                 return {is_float ? TokenType::NUMBER_FLOAT : TokenType::NUMBER_INT, {start, static_cast<size_t>(cursor_ - start)}, line_, static_cast<uint32_t>(start - line_start_ + 1), 0};
             }
             const char* start = cursor_;
             do { cursor_++; } while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_IDENT));
             return {TokenType::IDENTIFIER, {start, static_cast<size_t>(cursor_ - start)}, line_, static_cast<uint32_t>(start - line_start_ + 1), 0};
        }

        if (type & CT_QUOTE) {
            const char* start = cursor_;
            char quote = *cursor_;
            cursor_++;
            while (cursor_ < end_ && *cursor_ != quote) {
                if (*cursor_ == '\\') cursor_ += 2; 
                else cursor_++;
            }
            if (cursor_ < end_) cursor_++;
            return {TokenType::STRING, {start, static_cast<size_t>(cursor_ - start)}, line_, static_cast<uint32_t>(start - line_start_ + 1), 0};
        }

        if (*cursor_ == '#') {
             if (cursor_ + 1 < end_) {
                char next = *(cursor_ + 1);
                if (next == '^' || next == '@') {
                    bool is_dbg = (next == '^');
                    cursor_ += 2;
                    while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_SPACE)) cursor_++;
                    const char* start = cursor_;
                    if (is_dbg) while (cursor_ < end_ && *cursor_ != '\n') cursor_++;
                    else while (cursor_ < end_ && (table[static_cast<uint8_t>(*cursor_)] & CT_IDENT)) cursor_++;
                    
                    return {is_dbg ? TokenType::DEBUG_INFO : TokenType::ANNOTATION, {start, static_cast<size_t>(cursor_ - start)}, line_, static_cast<uint32_t>(start - line_start_ + 1), 0};
                }
             }
             while (cursor_ < end_ && *cursor_ != '\n') cursor_++;
             continue;
        }

        Token err{TokenType::UNKNOWN, {cursor_, 1}, line_, static_cast<uint32_t>(cursor_ - line_start_ + 1), 0};
        cursor_++;
        return err;
    }

    return {TokenType::END_OF_FILE, {}, line_, 0, 0};
}

} // namespace meow::masm