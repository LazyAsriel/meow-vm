/**
 * @file assembler.cpp
 * @brief MeowVM Assembler - Converts textual assembly to .meowb bytecode
 * @details Fixed initialization order and aligned with BinaryLoader format.
 * Compile: clang++ -std=c++23 -O3 tools/assembler.cpp -o meow-asm
 */

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>

// ============================================================================
// 1. DEFINITIONS (OpCodes & Types)
// ============================================================================

enum class OpCode : uint8_t {
    // Load/Store
    LOAD_CONST, LOAD_NULL, LOAD_TRUE, LOAD_FALSE,
    LOAD_INT, LOAD_FLOAT,
    MOVE,
    __BEGIN_OPERATOR__,
    // Binary
    ADD, SUB, MUL, DIV, MOD, POW,
    EQ, NEQ, GT, GE, LT, LE,
    // Unary
    NEG, NOT,
    // Bitwise
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, LSHIFT, RSHIFT,
    __END_OPERATOR__,
    // Variables
    GET_GLOBAL, SET_GLOBAL,
    GET_UPVALUE, SET_UPVALUE,
    CLOSURE, CLOSE_UPVALUES,
    // Control Flow
    JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE,
    CALL, CALL_VOID, RETURN, HALT,
    // Data Structures
    NEW_ARRAY, NEW_HASH,
    GET_INDEX, SET_INDEX,
    GET_KEYS, GET_VALUES,
    // OOP
    NEW_CLASS, NEW_INSTANCE,
    GET_PROP, SET_PROP, SET_METHOD,
    INHERIT, GET_SUPER,
    // Exceptions
    THROW, SETUP_TRY, POP_TRY,
    // Modules
    IMPORT_MODULE, EXPORT, GET_EXPORT, IMPORT_ALL,
    
    TOTAL_OPCODES
};

// Mapping OpCode String -> Enum
static std::unordered_map<std::string, OpCode> OP_MAP;

void init_op_map() {
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

enum class TokenType {
    DIR_FUNC, DIR_ENDFUNC, DIR_REGISTERS, DIR_UPVALUES, DIR_UPVALUE, DIR_CONST,
    LABEL_DEF, IDENTIFIER, OPCODE,
    NUMBER_INT, NUMBER_FLOAT, STRING,
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
    size_t line;
};

// ============================================================================
// 2. LEXER
// ============================================================================

class Lexer {
    std::string src_;
    size_t pos_ = 0, line_ = 1;

public:
    explicit Lexer(std::string src) : src_(std::move(src)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (!is_at_end()) {
            char c = peek();
            
            // Whitespace
            if (isspace(c)) {
                if (c == '\n') line_++;
                advance();
                continue;
            }
            // Comments
            if (c == '#') {
                while (peek() != '\n' && !is_at_end()) advance();
                continue;
            }
            // Directives
            if (c == '.') {
                tokens.push_back(scan_directive());
                continue;
            }
            // Strings
            if (c == '"' || c == '\'') {
                tokens.push_back(scan_string());
                continue;
            }
            // Numbers
            if (isdigit(c) || (c == '-' && isdigit(peek(1)))) {
                tokens.push_back(scan_number());
                continue;
            }
            // Identifiers / Opcodes / Labels
            if (isalpha(c) || c == '_' || c == '@') {
                tokens.push_back(scan_identifier());
                continue;
            }
            // Skip unknown
            advance();
        }
        tokens.push_back({TokenType::END_OF_FILE, "", line_});
        return tokens;
    }

private:
    bool is_at_end() const { return pos_ >= src_.size(); }
    char peek(int offset = 0) const { 
        if (pos_ + offset >= src_.size()) return '\0';
        return src_[pos_ + offset]; 
    }
    char advance() { return src_[pos_++]; }

    Token scan_directive() {
        size_t start = pos_;
        advance(); // eat .
        while (isalnum(peek()) || peek() == '_') advance();
        std::string text = src_.substr(start, pos_ - start);
        
        TokenType type = TokenType::UNKNOWN;
        if (text == ".func") type = TokenType::DIR_FUNC;
        else if (text == ".endfunc") type = TokenType::DIR_ENDFUNC;
        else if (text == ".registers") type = TokenType::DIR_REGISTERS;
        else if (text == ".upvalues") type = TokenType::DIR_UPVALUES;
        else if (text == ".upvalue") type = TokenType::DIR_UPVALUE;
        else if (text == ".const") type = TokenType::DIR_CONST;
        
        return {type, text, line_};
    }

    Token scan_string() {
        char quote = advance();
        std::string res;
        while (peek() != quote && !is_at_end()) {
            if (peek() == '\\') {
                advance();
                if (peek() == 'n') res += '\n';
                else if (peek() == 't') res += '\t';
                else if (peek() == '\\') res += '\\';
                else if (peek() == '"') res += '"';
                else res += peek();
            } else {
                res += peek();
            }
            advance();
        }
        if (!is_at_end()) advance(); // closing quote
        return {TokenType::STRING, res, line_};
    }

    Token scan_number() {
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

    Token scan_identifier() {
        size_t start = pos_;
        while (isalnum(peek()) || peek() == '_' || peek() == '@') advance();
        
        if (peek() == ':') {
            advance(); 
            return {TokenType::LABEL_DEF, src_.substr(start, pos_ - start - 1), line_};
        }

        std::string text = src_.substr(start, pos_ - start);
        // Check OP_MAP here. Important: OP_MAP must be initialized!
        if (OP_MAP.count(text)) return {TokenType::OPCODE, text, line_};
        
        return {TokenType::IDENTIFIER, text, line_};
    }
};

// ============================================================================
// 3. ASSEMBLER DATA STRUCTURES
// ============================================================================

enum class ConstType { NULL_T, INT_T, FLOAT_T, STRING_T, PROTO_REF_T };

struct Constant {
    ConstType type;
    int64_t val_i64 = 0;
    double val_f64 = 0.0;
    std::string val_str;
    uint32_t proto_index = 0; 
};

struct UpvalueInfo {
    bool is_local;
    uint32_t index;
};

struct Prototype {
    std::string name;
    uint32_t num_regs = 0;
    uint32_t num_upvalues = 0;
    
    std::vector<Constant> constants = {};
    std::vector<UpvalueInfo> upvalues = {};
    std::vector<uint8_t> bytecode = {};

    std::unordered_map<std::string, size_t> labels = {};
    std::vector<std::pair<size_t, std::string>> jump_patches = {};
    std::vector<std::pair<size_t, std::string>> try_patches = {};
};

// ============================================================================
// 4. ASSEMBLER LOGIC
// ============================================================================

class Assembler {
    std::vector<Token> tokens_;
    size_t current_ = 0;
    
    std::vector<Prototype> protos_;
    Prototype* curr_proto_ = nullptr;
    std::unordered_map<std::string, uint32_t> proto_name_map_;

public:
    Assembler(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    void assemble(const std::string& output_file) {
        while (!is_at_end()) {
            try {
                parse_statement();
            } catch (const std::exception& e) {
                std::cerr << "[Error] Line " << peek().line << ": " << e.what() << "\n";
                exit(1);
            }
        }

        link_proto_refs();
        patch_labels();
        write_binary(output_file);
    }

private:
    Token peek() const { return tokens_[current_]; }
    Token previous() const { return tokens_[current_ - 1]; }
    bool is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
    Token advance() { if (!is_at_end()) current_++; return previous(); }
    
    Token consume(TokenType type, const std::string& msg) {
        if (peek().type == type) return advance();
        throw std::runtime_error(msg + " (Found: " + peek().lexeme + " at line " + std::to_string(peek().line) + ")");
    }

    void parse_statement() {
        Token tk = peek();
        switch (tk.type) {
            case TokenType::DIR_FUNC:      parse_func(); break;
            case TokenType::DIR_REGISTERS: parse_registers(); break;
            case TokenType::DIR_UPVALUES:  parse_upvalues_decl(); break;
            case TokenType::DIR_UPVALUE:   parse_upvalue_def(); break;
            case TokenType::DIR_CONST:     parse_const(); break;
            case TokenType::LABEL_DEF:     parse_label(); break;
            case TokenType::OPCODE:        parse_instruction(); break;
            case TokenType::DIR_ENDFUNC:   
                advance(); 
                curr_proto_ = nullptr; 
                break;
            case TokenType::IDENTIFIER:
                throw std::runtime_error("Unexpected identifier '" + tk.lexeme + "'. Did you forget a colon for a label?");
            default:
                throw std::runtime_error("Unexpected token: " + tk.lexeme);
        }
    }

    void parse_func() {
        advance(); 
        Token name = consume(TokenType::IDENTIFIER, "Expected function name");
        std::string func_name = name.lexeme;
        if (func_name.starts_with("@")) func_name = func_name.substr(1);

        protos_.push_back(Prototype{.name = func_name});
        curr_proto_ = &protos_.back();
        proto_name_map_[func_name] = protos_.size() - 1;
    }

    void parse_registers() {
        if (!curr_proto_) throw std::runtime_error("Outside .func");
        advance();
        Token num = consume(TokenType::NUMBER_INT, "Expected number");
        curr_proto_->num_regs = std::stoi(num.lexeme);
    }

    void parse_upvalues_decl() {
        if (!curr_proto_) throw std::runtime_error("Outside .func");
        advance();
        Token num = consume(TokenType::NUMBER_INT, "Expected number");
        curr_proto_->num_upvalues = std::stoi(num.lexeme);
        curr_proto_->upvalues.resize(curr_proto_->num_upvalues);
    }

    void parse_upvalue_def() {
        if (!curr_proto_) throw std::runtime_error("Outside .func");
        advance();
        uint32_t idx = std::stoi(consume(TokenType::NUMBER_INT, "Index").lexeme);
        std::string type = consume(TokenType::IDENTIFIER, "Type").lexeme;
        uint32_t slot = std::stoi(consume(TokenType::NUMBER_INT, "Slot").lexeme);
        if (idx >= curr_proto_->upvalues.size()) throw std::runtime_error("Upvalue index out of range");
        curr_proto_->upvalues[idx] = { (type == "local"), slot };
    }

    void parse_const() {
        if (!curr_proto_) throw std::runtime_error("Outside .func");
        advance();
        Constant c;
        Token tk = peek();
        if (tk.type == TokenType::STRING) {
            c.type = ConstType::STRING_T;
            c.val_str = tk.lexeme;
            advance();
        } else if (tk.type == TokenType::NUMBER_INT) {
            c.type = ConstType::INT_T;
            if (tk.lexeme.size() > 2 && tk.lexeme.substr(0,2) == "0x") {
                c.val_i64 = std::stoll(tk.lexeme, nullptr, 16);
            } else {
                c.val_i64 = std::stoll(tk.lexeme);
            }
            advance();
        } else if (tk.type == TokenType::NUMBER_FLOAT) {
            c.type = ConstType::FLOAT_T;
            c.val_f64 = std::stod(tk.lexeme);
            advance();
        } else if (tk.type == TokenType::IDENTIFIER) {
            if (tk.lexeme == "null") {
                c.type = ConstType::NULL_T;
                advance();
            } else if (tk.lexeme.starts_with("@")) {
                c.type = ConstType::PROTO_REF_T;
                c.val_str = tk.lexeme.substr(1); 
                advance();
            } else {
                throw std::runtime_error("Unknown constant: " + tk.lexeme);
            }
        } else {
            throw std::runtime_error("Invalid constant token");
        }
        curr_proto_->constants.push_back(c);
    }

    void parse_label() {
        if (!curr_proto_) throw std::runtime_error("Label outside .func");
        Token lbl = advance();
        curr_proto_->labels[lbl.lexeme] = curr_proto_->bytecode.size();
    }

    void emit_byte(uint8_t b) { curr_proto_->bytecode.push_back(b); }
    void emit_u16(uint16_t v) { emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); }
    void emit_u64(uint64_t v) { for(int i=0; i<8; ++i) emit_byte((v >> (i*8)) & 0xFF); }

    void parse_instruction() {
        if (!curr_proto_) throw std::runtime_error("Instruction outside .func");
        Token op_tok = advance();
        OpCode op = OP_MAP[op_tok.lexeme];
        
        emit_byte(static_cast<uint8_t>(op));

        auto parse_u16 = [&]() {
            Token t = consume(TokenType::NUMBER_INT, "Expected u16 operand");
            emit_u16(static_cast<uint16_t>(std::stoi(t.lexeme)));
        };

        switch (op) {
            case OpCode::LOAD_NULL: case OpCode::LOAD_TRUE: case OpCode::LOAD_FALSE:
            case OpCode::HALT: case OpCode::POP_TRY: 
                // These usually have operands in MeowVM (dst), check arity or code
                // Based on op_codes.md: LOAD_NULL/TRUE/FALSE take dst: u16. HALT takes 0.
                if (op != OpCode::HALT && op != OpCode::POP_TRY) parse_u16(); 
                break;

            case OpCode::LOAD_INT: {
                parse_u16(); 
                Token t = consume(TokenType::NUMBER_INT, "Expected int64");
                int64_t val = std::stoll(t.lexeme);
                emit_u64(std::bit_cast<uint64_t>(val));
                break;
            }
            case OpCode::LOAD_FLOAT: {
                parse_u16(); 
                Token t = consume(TokenType::NUMBER_FLOAT, "Expected double");
                double val = std::stod(t.lexeme);
                emit_u64(std::bit_cast<uint64_t>(val));
                break;
            }
            case OpCode::JUMP: case OpCode::SETUP_TRY: {
                Token target = peek();
                if (target.type == TokenType::IDENTIFIER) {
                    advance();
                    curr_proto_->try_patches.push_back({curr_proto_->bytecode.size(), target.lexeme});
                    emit_u16(0xFFFF);
                } else {
                    parse_u16();
                }
                break;
            }
            case OpCode::JUMP_IF_FALSE: case OpCode::JUMP_IF_TRUE: {
                parse_u16();
                Token target = peek();
                if (target.type == TokenType::IDENTIFIER) {
                    advance();
                    curr_proto_->jump_patches.push_back({curr_proto_->bytecode.size(), target.lexeme});
                    emit_u16(0xFFFF);
                } else {
                    parse_u16();
                }
                break;
            }
            case OpCode::RETURN:
                parse_u16(); // Takes 1 arg (ret_reg)
                break;
            
            default: {
                // Heuristic for other opcodes based on generic arity
                // Simple rule: consume u16s until newline? No, parser needs structure.
                // Using helper
                int args = get_arity(op);
                for(int i=0; i<args; ++i) parse_u16();
                break;
            }
        }
    }

    int get_arity(OpCode op) {
        // Helper based on op_codes.md
        switch (op) {
            case OpCode::CLOSE_UPVALUES: case OpCode::IMPORT_ALL: case OpCode::THROW: return 1;
            case OpCode::LOAD_CONST: case OpCode::MOVE: case OpCode::NEG: case OpCode::NOT:
            case OpCode::BIT_NOT: case OpCode::GET_GLOBAL: case OpCode::SET_GLOBAL:
            case OpCode::GET_UPVALUE: case OpCode::SET_UPVALUE: case OpCode::CLOSURE:
            case OpCode::NEW_CLASS: case OpCode::NEW_INSTANCE: case OpCode::IMPORT_MODULE:
            case OpCode::EXPORT: case OpCode::GET_KEYS: case OpCode::GET_VALUES:
            case OpCode::GET_SUPER: case OpCode::GET_EXPORT: 
                return 2;
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

    void link_proto_refs() {
        for (auto& p : protos_) {
            for (auto& c : p.constants) {
                if (c.type == ConstType::PROTO_REF_T) {
                    if (proto_name_map_.count(c.val_str)) {
                        c.proto_index = proto_name_map_[c.val_str];
                    } else {
                        throw std::runtime_error("Undefined proto: " + c.val_str);
                    }
                }
            }
        }
    }

    void patch_labels() {
        for (auto& p : protos_) {
            auto apply_patch = [&](auto& patches) {
                for (auto& patch : patches) {
                    size_t offset = patch.first;
                    std::string lbl = patch.second;
                    if (!p.labels.count(lbl)) throw std::runtime_error("Undefined label: " + lbl);
                    size_t target = p.labels[lbl];
                    p.bytecode[offset] = target & 0xFF;
                    p.bytecode[offset+1] = (target >> 8) & 0xFF;
                }
            };
            apply_patch(p.jump_patches);
            apply_patch(p.try_patches);
        }
    }

    void write_binary(const std::string& filename) {
        std::ofstream out(filename, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot open output file");

        auto write_u8 = [&](uint8_t v) { out.write((char*)&v, 1); };
        auto write_u32 = [&](uint32_t v) { out.write((char*)&v, 4); };
        auto write_u64 = [&](uint64_t v) { out.write((char*)&v, 8); };
        auto write_f64 = [&](double v) { 
            uint64_t bit; std::memcpy(&bit, &v, 8);
            out.write((char*)&bit, 8); 
        };
        auto write_str = [&](const std::string& s) {
            write_u32(s.size());
            out.write(s.data(), s.size());
        };

        // Header
        write_u32(0x4D454F57); // Magic
        write_u32(1);          // Version
        
        // Main Proto Index
        if (proto_name_map_.count("main")) write_u32(proto_name_map_["main"]);
        else write_u32(0);

        write_u32(protos_.size()); // Proto Count

        for (const auto& p : protos_) {
            write_u32(p.num_regs);
            write_u32(p.num_upvalues);
            
            // Prepare constants (Name at index 0)
            std::vector<Constant> write_consts = p.constants;
            Constant name_const; 
            name_const.type = ConstType::STRING_T; 
            name_const.val_str = p.name;

            write_consts.push_back(name_const);

            write_u32(write_consts.size() - 1); // Name Index
            write_u32(write_consts.size()); // Pool Size

            for (const auto& c : write_consts) {
                switch (c.type) {
                    case ConstType::NULL_T: write_u8(0); break;
                    case ConstType::INT_T:  write_u8(1); write_u64(c.val_i64); break;
                    case ConstType::FLOAT_T:write_u8(2); write_f64(c.val_f64); break;
                    case ConstType::STRING_T:write_u8(3); write_str(c.val_str); break;
                    case ConstType::PROTO_REF_T:
                        write_u8(4); 
                        write_u32(c.proto_index);
                        break;
                }
            }

            write_u32(p.upvalues.size()); // Upvalue Count
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
};

// ============================================================================
// 5. MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: meow-asm <input.meow> [output.meowb]\n";
        return 1;
    }

    // --- CRITICAL FIX: Initialize OpCode Map BEFORE Lexer runs ---
    init_op_map();

    std::string input_path = argv[1];
    std::string output_path = (argc >= 3) ? argv[2] : "out.meowb";
    
    if (argc < 3 && input_path.size() > 5 && input_path.substr(input_path.size()-5) == ".meow") {
        output_path = input_path.substr(0, input_path.size()-5) + ".meowb";
    }

    std::ifstream f(input_path);
    if (!f) {
        std::cerr << "Cannot open input file: " << input_path << "\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(f)), {});
    f.close();

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Assembler asm_tool(std::move(tokens));
    asm_tool.assemble(output_path);

    return 0;
}