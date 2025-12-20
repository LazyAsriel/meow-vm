// =============================================================================
//  FILE PATH: src/tools/masm/include/meow/masm/assembler.h
// =============================================================================

     1	#pragma once
     2	#include "common.h"
     3	#include <vector>
     4	#include <string>
     5	#include <unordered_map>
     6	
     7	namespace meow::masm {
     8	
     9	class Assembler {
    10	    const std::vector<Token>& tokens_;
    11	    size_t current_ = 0;
    12	    
    13	    std::vector<Prototype> protos_;
    14	    Prototype* curr_proto_ = nullptr;
    15	    std::unordered_map<std::string, uint32_t> proto_name_map_;
    16	
    17	public:
    18	    explicit Assembler(const std::vector<Token>& tokens);
    19	
    20	    std::vector<uint8_t> assemble();
    21	    void assemble_to_file(const std::string& output_file);
    22	    
    23	    static int get_arity(meow::OpCode op);
    24	
    25	private:
    26	    Token peek() const;
    27	    Token previous() const;
    28	    bool is_at_end() const;
    29	    Token advance();
    30	    Token consume(TokenType type, const std::string& msg);
    31	
    32	    // Parsing
    33	    void parse_statement();
    34	    void parse_func();
    35	    void parse_registers();
    36	    void parse_upvalues_decl();
    37	    void parse_upvalue_def();
    38	    void parse_const();
    39	    void parse_label();
    40	    void parse_instruction();
    41	    
    42	    void optimize();
    43	    
    44	    std::string parse_string_literal(std::string_view sv);
    45	
    46	    // Emit bytecode helpers
    47	    void emit_byte(uint8_t b);
    48	    void emit_u16(uint16_t v);
    49	    void emit_u32(uint32_t v);
    50	    void emit_u64(uint64_t v);
    51	
    52	    // Finalize
    53	    void link_proto_refs();
    54	    void patch_labels();
    55	    std::vector<uint8_t> serialize_binary();
    56	};
    57	
    58	} // namespace meow::masm


// =============================================================================
//  FILE PATH: src/tools/masm/include/meow/masm/common.h
// =============================================================================

     1	#pragma once
     2	#include <cstdint>
     3	#include <string>
     4	#include <vector>
     5	#include <unordered_map>
     6	#include <string_view>
     7	#include <meow/bytecode/op_codes.h>
     8	
     9	namespace meow::masm {
    10	
    11	extern std::unordered_map<std::string_view, meow::OpCode> OP_MAP;
    12	void init_op_map();
    13	
    14	enum class TokenType {
    15	    DIR_FUNC, DIR_ENDFUNC, DIR_REGISTERS, DIR_UPVALUES, DIR_UPVALUE, DIR_CONST,
    16	    LABEL_DEF, IDENTIFIER, OPCODE,
    17	    NUMBER_INT, NUMBER_FLOAT, STRING,
    18	    END_OF_FILE, UNKNOWN
    19	};
    20	
    21	struct Token {
    22	    TokenType type;
    23	    std::string_view lexeme;
    24	    size_t line;
    25	};
    26	
    27	enum class ConstType { NULL_T, INT_T, FLOAT_T, STRING_T, PROTO_REF_T };
    28	
    29	struct Constant {
    30	    ConstType type;
    31	    int64_t val_i64 = 0;
    32	    double val_f64 = 0.0;
    33	    std::string val_str;
    34	    uint32_t proto_index = 0; 
    35	};
    36	
    37	struct UpvalueInfo {
    38	    bool is_local;
    39	    uint32_t index;
    40	};
    41	
    42	struct Prototype {
    43	    std::string name;
    44	    uint32_t num_regs = 0;
    45	    uint32_t num_upvalues = 0;
    46	    
    47	    std::vector<Constant> constants;
    48	    std::vector<UpvalueInfo> upvalues;
    49	    std::vector<uint8_t> bytecode;
    50	
    51	    std::unordered_map<std::string, size_t> labels;
    52	    std::vector<std::pair<size_t, std::string>> jump_patches;
    53	    std::vector<std::pair<size_t, std::string>> try_patches;
    54	};
    55	
    56	} // namespace meow::masm


// =============================================================================
//  FILE PATH: src/tools/masm/include/meow/masm/lexer.h
// =============================================================================

     1	#pragma once
     2	#include "common.h"
     3	#include <vector>
     4	
     5	namespace meow::masm {
     6	
     7	class Lexer {
     8	    std::string_view src_;
     9	    size_t pos_ = 0;
    10	    size_t line_ = 1;
    11	
    12	public:
    13	    explicit Lexer(std::string_view src) : src_(src) {}
    14	    std::vector<Token> tokenize();
    15	
    16	private:
    17	    bool is_at_end() const;
    18	    char peek(int offset = 0) const;
    19	    char advance();
    20	    
    21	    Token scan_directive();
    22	    Token scan_string();
    23	    Token scan_number();
    24	    Token scan_identifier();
    25	};
    26	
    27	} // namespace meow::masm


// =============================================================================
//  FILE PATH: src/tools/masm/src/assembler.cpp
// =============================================================================

     1	#include <meow/masm/assembler.h>
     2	#include <iostream>
     3	#include <fstream>
     4	#include <charconv> 
     5	#include <bit>
     6	#include <cstring>
     7	#include <stdexcept>
     8	
     9	namespace meow::masm {
    10	
    11	Assembler::Assembler(const std::vector<Token>& tokens) : tokens_(tokens) {}
    12	
    13	Token Assembler::peek() const { return tokens_[current_]; }
    14	Token Assembler::previous() const { return tokens_[current_ - 1]; }
    15	bool Assembler::is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
    16	Token Assembler::advance() { if (!is_at_end()) current_++; return previous(); }
    17	
    18	Token Assembler::consume(TokenType type, const std::string& msg) {
    19	    if (peek().type == type) return advance();
    20	    throw std::runtime_error(msg + " (Found: " + std::string(peek().lexeme) + " at line " + std::to_string(peek().line) + ")");
    21	}
    22	
    23	void Assembler::parse_statement() {
    24	    Token tk = peek();
    25	    switch (tk.type) {
    26	        case TokenType::DIR_FUNC:      parse_func(); break;
    27	        case TokenType::DIR_REGISTERS: parse_registers(); break;
    28	        case TokenType::DIR_UPVALUES:  parse_upvalues_decl(); break;
    29	        case TokenType::DIR_UPVALUE:   parse_upvalue_def(); break;
    30	        case TokenType::DIR_CONST:     parse_const(); break;
    31	        case TokenType::LABEL_DEF:     parse_label(); break;
    32	        case TokenType::OPCODE:        parse_instruction(); break;
    33	        case TokenType::DIR_ENDFUNC:   advance(); curr_proto_ = nullptr; break;
    34	        case TokenType::IDENTIFIER:    throw std::runtime_error("Unexpected identifier: " + std::string(tk.lexeme));
    35	        default: throw std::runtime_error("Unexpected token: " + std::string(tk.lexeme));
    36	    }
    37	}
    38	
    39	void Assembler::parse_func() {
    40	    advance(); 
    41	    Token name = consume(TokenType::IDENTIFIER, "Expected func name");
    42	    std::string func_name(name.lexeme);
    43	    if (func_name.starts_with("@")) func_name = func_name.substr(1);
    44	    protos_.emplace_back();
    45	    protos_.back().name = func_name;
    46	    
    47	    curr_proto_ = &protos_.back();
    48	    proto_name_map_[func_name] = protos_.size() - 1;
    49	}
    50	
    51	void Assembler::parse_registers() {
    52	    if (!curr_proto_) throw std::runtime_error("Outside .func");
    53	    advance();
    54	    Token num = consume(TokenType::NUMBER_INT, "Expected number");
    55	    curr_proto_->num_regs = std::stoi(std::string(num.lexeme));
    56	}
    57	
    58	void Assembler::parse_upvalues_decl() {
    59	    if (!curr_proto_) throw std::runtime_error("Outside .func");
    60	    advance();
    61	    Token num = consume(TokenType::NUMBER_INT, "Expected number");
    62	    curr_proto_->num_upvalues = std::stoi(std::string(num.lexeme));
    63	    curr_proto_->upvalues.resize(curr_proto_->num_upvalues);
    64	}
    65	
    66	void Assembler::parse_upvalue_def() {
    67	    if (!curr_proto_) throw std::runtime_error("Outside .func");
    68	    advance();
    69	    uint32_t idx = std::stoi(std::string(consume(TokenType::NUMBER_INT, "Idx").lexeme));
    70	    std::string type(consume(TokenType::IDENTIFIER, "Type").lexeme);
    71	    uint32_t slot = std::stoi(std::string(consume(TokenType::NUMBER_INT, "Slot").lexeme));
    72	    if (idx < curr_proto_->upvalues.size()) curr_proto_->upvalues[idx] = { (type == "local"), slot };
    73	}
    74	
    75	void Assembler::parse_const() {
    76	    if (!curr_proto_) throw std::runtime_error("Outside .func");
    77	    advance();
    78	    Constant c;
    79	    Token tk = peek();
    80	    
    81	    if (tk.type == TokenType::STRING) {
    82	        c.type = ConstType::STRING_T;
    83	        c.val_str = parse_string_literal(tk.lexeme);
    84	        advance();
    85	    } else if (tk.type == TokenType::NUMBER_INT) {
    86	        c.type = ConstType::INT_T;
    87	        std::string_view sv = tk.lexeme;
    88	        if (sv.starts_with("0x") || sv.starts_with("0X")) {
    89	            c.val_i64 = std::stoll(std::string(sv), nullptr, 16);
    90	        } else {
    91	            std::from_chars(sv.data(), sv.data() + sv.size(), c.val_i64);
    92	        }
    93	        advance();
    94	    } else if (tk.type == TokenType::NUMBER_FLOAT) {
    95	        c.type = ConstType::FLOAT_T;
    96	        c.val_f64 = std::stod(std::string(tk.lexeme));
    97	        advance();
    98	    } else if (tk.type == TokenType::IDENTIFIER) {
    99	        if (tk.lexeme == "null") { c.type = ConstType::NULL_T; advance(); }
   100	        else if (tk.lexeme == "true") { c.type = ConstType::INT_T; c.val_i64 = 1; advance(); } 
   101	        else if (tk.lexeme == "false") { c.type = ConstType::INT_T; c.val_i64 = 0; advance(); }
   102	        else if (tk.lexeme.starts_with("@")) {
   103	            c.type = ConstType::PROTO_REF_T;
   104	            c.val_str = tk.lexeme.substr(1);
   105	            advance();
   106	        } else throw std::runtime_error("Unknown constant identifier: " + std::string(tk.lexeme));
   107	    } else throw std::runtime_error("Invalid constant");
   108	    curr_proto_->constants.push_back(c);
   109	}
   110	
   111	void Assembler::parse_label() {
   112	    Token lbl = advance();
   113	    std::string labelName(lbl.lexeme); 
   114	    curr_proto_->labels[labelName] = curr_proto_->bytecode.size();
   115	}
   116	
   117	void Assembler::emit_byte(uint8_t b) { curr_proto_->bytecode.push_back(b); }
   118	void Assembler::emit_u16(uint16_t v) { emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); }
   119	void Assembler::emit_u32(uint32_t v) { 
   120	    emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); 
   121	    emit_byte((v >> 16) & 0xFF); emit_byte((v >> 24) & 0xFF); 
   122	}
   123	void Assembler::emit_u64(uint64_t v) { for(int i=0; i<8; ++i) emit_byte((v >> (i*8)) & 0xFF); }
   124	
   125	void Assembler::parse_instruction() {
   126	    if (!curr_proto_) throw std::runtime_error("Instruction outside .func");
   127	    Token op_tok = advance();
   128	    
   129	    // Check if OP exists
   130	    if (OP_MAP.find(op_tok.lexeme) == OP_MAP.end()) {
   131	        throw std::runtime_error("Unknown opcode: " + std::string(op_tok.lexeme));
   132	    }
   133	    meow::OpCode op = OP_MAP[op_tok.lexeme];
   134	    emit_byte(static_cast<uint8_t>(op));
   135	
   136	    auto parse_u16 = [&]() {
   137	        Token t = consume(TokenType::NUMBER_INT, "Expected u16");
   138	        emit_u16(static_cast<uint16_t>(std::stoi(std::string(t.lexeme))));
   139	    };
   140	
   141	    switch (op) {
   142	        case meow::OpCode::LOAD_INT: {
   143	            parse_u16(); // dst
   144	            Token t = consume(TokenType::NUMBER_INT, "Expected int64");
   145	            int64_t val;
   146	            std::from_chars(t.lexeme.data(), t.lexeme.data() + t.lexeme.size(), val);
   147	            emit_u64(std::bit_cast<uint64_t>(val));
   148	            break;
   149	        }
   150	        case meow::OpCode::LOAD_FLOAT: {
   151	            parse_u16(); // dst
   152	            Token t = consume(TokenType::NUMBER_FLOAT, "Expected double");
   153	            double val = std::stod(std::string(t.lexeme));
   154	            emit_u64(std::bit_cast<uint64_t>(val));
   155	            break;
   156	        }
   157	        case meow::OpCode::JUMP: case meow::OpCode::SETUP_TRY: {
   158	            if (peek().type == TokenType::IDENTIFIER) {
   159	                Token target = advance();
   160	                curr_proto_->try_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
   161	                emit_u16(0xFFFF);
   162	                if (op == meow::OpCode::SETUP_TRY) parse_u16(); 
   163	            } else {
   164	                parse_u16();
   165	                if (op == meow::OpCode::SETUP_TRY) parse_u16();
   166	            }
   167	            break;
   168	        }
   169	        case meow::OpCode::JUMP_IF_FALSE: case meow::OpCode::JUMP_IF_TRUE: {
   170	            parse_u16(); // reg
   171	            if (peek().type == TokenType::IDENTIFIER) {
   172	                Token target = advance();
   173	                curr_proto_->jump_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
   174	                emit_u16(0xFFFF);
   175	            } else parse_u16();
   176	            break;
   177	        }
   178	        case meow::OpCode::GET_PROP:
   179	        case meow::OpCode::SET_PROP: {
   180	            for(int i=0; i<3; ++i) parse_u16();
   181	            
   182	            // Emit Polymorphic Cache Placeholder (48 bytes)
   183	            for (int j = 0; j < 4; ++j) {
   184	                emit_u64(0); // Shape*
   185	                emit_u32(0); // Offset
   186	            }
   187	            break;
   188	        }
   189	        
   190	        case meow::OpCode::CALL:
   191	        case meow::OpCode::TAIL_CALL: {
   192	            for(int i=0; i<4; ++i) parse_u16(); // dst, fn, arg_start, argc
   193	            
   194	            // Padding cho Inline Cache (16 bytes)
   195	            emit_u64(0); // Cache Tag
   196	            emit_u64(0); // Cache Dest
   197	            break;
   198	        }
   199	
   200	        case meow::OpCode::CALL_VOID: {
   201	            for(int i=0; i<3; ++i) parse_u16(); // fn, arg_start, argc
   202	            emit_u64(0); emit_u64(0); // 16 bytes cache
   203	            break;
   204	        }
   205	
   206	        default: {
   207	            int args = get_arity(op);
   208	            for(int i=0; i<args; ++i) parse_u16();
   209	            break;
   210	        }
   211	    }
   212	}
   213	
   214	int Assembler::get_arity(meow::OpCode op) {
   215	    switch (op) {
   216	        case meow::OpCode::CLOSE_UPVALUES: case meow::OpCode::IMPORT_ALL: case meow::OpCode::THROW: 
   217	        case meow::OpCode::RETURN: case meow::OpCode::LOAD_NULL: case meow::OpCode::LOAD_TRUE: case meow::OpCode::LOAD_FALSE:
   218	        case meow::OpCode::INC: case meow::OpCode::DEC:
   219	            return 1;
   220	            
   221	        case meow::OpCode::LOAD_CONST: case meow::OpCode::MOVE: 
   222	        case meow::OpCode::NEG: case meow::OpCode::NOT: case meow::OpCode::BIT_NOT: 
   223	        case meow::OpCode::GET_UPVALUE: case meow::OpCode::SET_UPVALUE: 
   224	        case meow::OpCode::CLOSURE:
   225	        case meow::OpCode::NEW_CLASS: case meow::OpCode::NEW_INSTANCE: 
   226	        case meow::OpCode::IMPORT_MODULE:
   227	        case meow::OpCode::EXPORT: 
   228	        case meow::OpCode::GET_KEYS: case meow::OpCode::GET_VALUES:
   229	        case meow::OpCode::GET_SUPER: 
   230	        case meow::OpCode::GET_GLOBAL: case meow::OpCode::SET_GLOBAL:
   231	        case meow::OpCode::INHERIT:
   232	            return 2;
   233	
   234	        case meow::OpCode::GET_EXPORT: 
   235	        case meow::OpCode::ADD: case meow::OpCode::SUB: case meow::OpCode::MUL: case meow::OpCode::DIV:
   236	        case meow::OpCode::MOD: case meow::OpCode::POW: case meow::OpCode::EQ: case meow::OpCode::NEQ:
   237	        case meow::OpCode::GT: case meow::OpCode::GE: case meow::OpCode::LT: case meow::OpCode::LE:
   238	        case meow::OpCode::BIT_AND: case meow::OpCode::BIT_OR: case meow::OpCode::BIT_XOR:
   239	        case meow::OpCode::LSHIFT: case meow::OpCode::RSHIFT: 
   240	        case meow::OpCode::NEW_ARRAY: case meow::OpCode::NEW_HASH: 
   241	        case meow::OpCode::GET_INDEX: case meow::OpCode::SET_INDEX: 
   242	        case meow::OpCode::GET_PROP: case meow::OpCode::SET_PROP:
   243	        case meow::OpCode::SET_METHOD: case meow::OpCode::CALL_VOID:
   244	        // case meow::OpCode::INHERIT:
   245	            return 3;
   246	            
   247	        case meow::OpCode::CALL:
   248	        case meow::OpCode::TAIL_CALL:
   249	            return 4;
   250	        case meow::OpCode::INVOKE: return 5;
   251	        default: return 0;
   252	    }
   253	}
   254	
   255	void Assembler::link_proto_refs() {
   256	    for (auto& p : protos_) {
   257	        for (auto& c : p.constants) {
   258	            if (c.type == ConstType::PROTO_REF_T) {
   259	                if (proto_name_map_.count(c.val_str)) c.proto_index = proto_name_map_[c.val_str];
   260	                else throw std::runtime_error("Undefined proto: " + c.val_str);
   261	            }
   262	        }
   263	    }
   264	}
   265	
   266	void Assembler::patch_labels() {
   267	    for (auto& p : protos_) {
   268	        auto apply = [&](auto& patches) {
   269	            for (auto& patch : patches) {
   270	                if (!p.labels.count(patch.second)) throw std::runtime_error("Undefined label: " + patch.second);
   271	                size_t target = p.labels[patch.second];
   272	                p.bytecode[patch.first] = target & 0xFF;
   273	                p.bytecode[patch.first+1] = (target >> 8) & 0xFF;
   274	            }
   275	        };
   276	        apply(p.jump_patches);
   277	        apply(p.try_patches);
   278	    }
   279	}
   280	
   281	std::string Assembler::parse_string_literal(std::string_view sv) {
   282	    if (sv.length() >= 2) sv = sv.substr(1, sv.length() - 2);
   283	    std::string res; res.reserve(sv.length());
   284	    for (size_t i = 0; i < sv.length(); ++i) {
   285	        if (sv[i] == '\\' && i + 1 < sv.length()) {
   286	            char next = sv[++i];
   287	            if (next == 'n') res += '\n'; 
   288	            else if (next == 'r') res += '\r';
   289	            else if (next == 't') res += '\t';
   290	            else if (next == '\\') res += '\\'; 
   291	            else if (next == '"') res += '"';
   292	            else res += next;
   293	        } else res += sv[i];
   294	    }
   295	    return res;
   296	}
   297	
   298	std::vector<uint8_t> Assembler::serialize_binary() {
   299	    std::vector<uint8_t> buffer;
   300	    buffer.reserve(4096); 
   301	
   302	    auto write_u8 = [&](uint8_t v) { buffer.push_back(v); };
   303	    auto write_u32 = [&](uint32_t v) { 
   304	        buffer.push_back(v & 0xFF); buffer.push_back((v >> 8) & 0xFF);
   305	        buffer.push_back((v >> 16) & 0xFF); buffer.push_back((v >> 24) & 0xFF);
   306	    };
   307	    auto write_u64 = [&](uint64_t v) { 
   308	        for(int i=0; i<8; ++i) buffer.push_back((v >> (i*8)) & 0xFF); 
   309	    };
   310	    auto write_f64 = [&](double v) { 
   311	        uint64_t bit; std::memcpy(&bit, &v, 8); write_u64(bit); 
   312	    };
   313	    auto write_str = [&](const std::string& s) { 
   314	        write_u32(s.size()); 
   315	        buffer.insert(buffer.end(), s.begin(), s.end()); 
   316	    };
   317	
   318	    write_u32(0x4D454F57); // Magic
   319	    write_u32(1);          // Version
   320	
   321	    if (proto_name_map_.count("main")) write_u32(proto_name_map_["main"]);
   322	    else write_u32(0);
   323	
   324	    write_u32(protos_.size()); 
   325	
   326	    for (const auto& p : protos_) {
   327	        write_u32(p.num_regs);
   328	        write_u32(p.num_upvalues);
   329	        
   330	        std::vector<Constant> write_consts = p.constants;
   331	        write_consts.push_back({ConstType::STRING_T, 0, 0, p.name, 0});
   332	        
   333	        write_u32(write_consts.size() - 1); 
   334	        write_u32(write_consts.size());     
   335	
   336	        for (const auto& c : write_consts) {
   337	            switch (c.type) {
   338	                case ConstType::NULL_T: write_u8(0); break;
   339	                case ConstType::INT_T:  write_u8(1); write_u64(c.val_i64); break;
   340	                case ConstType::FLOAT_T:write_u8(2); write_f64(c.val_f64); break;
   341	                case ConstType::STRING_T:write_u8(3); write_str(c.val_str); break;
   342	                case ConstType::PROTO_REF_T: write_u8(4); write_u32(c.proto_index); break;
   343	            }
   344	        }
   345	        write_u32(p.upvalues.size());
   346	        for (const auto& u : p.upvalues) {
   347	            write_u8(u.is_local ? 1 : 0);
   348	            write_u32(u.index);
   349	        }
   350	        write_u32(p.bytecode.size());
   351	        buffer.insert(buffer.end(), p.bytecode.begin(), p.bytecode.end());
   352	    }
   353	    return buffer;
   354	}
   355	
   356	static size_t get_instr_size(meow::OpCode op) {
   357	    using namespace meow;
   358	    switch (op) {
   359	        // --- Nhóm 1 byte (Không tham số) ---
   360	        case OpCode::HALT: 
   361	        case OpCode::POP_TRY: 
   362	        case OpCode::CLOSE_UPVALUES: 
   363	        case OpCode::RETURN: 
   364	        case OpCode::IMPORT_ALL:
   365	        case OpCode::THROW:
   366	            return 1 + 2; // Op(1) + U16(2) -> Wait, check get_arity
   367	        case OpCode::GET_PROP: 
   368	        case OpCode::SET_PROP: 
   369	            return 1 + 6 + 48; // Op + 3*U16 + 48(Cache) = 55 bytes
   370	            
   371	        case OpCode::CALL: 
   372	        case OpCode::TAIL_CALL: 
   373	            return 1 + 8 + 16; // Op + 4*U16 + 16(Cache) = 25 bytes
   374	            
   375	        case OpCode::CALL_VOID: 
   376	            return 1 + 6 + 16; // Op + 3*U16 + 16(Cache) = 23 bytes
   377	            
   378	        case OpCode::INVOKE: 
   379	            return 80;         // Fat Instruction (Size cố định)
   380	
   381	        // 2. Các lệnh Byte-code (Optimized B instructions)
   382	        case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: 
   383	        case OpCode::DIV_B: case OpCode::MOD_B: 
   384	        case OpCode::EQ_B:  case OpCode::NEQ_B: case OpCode::GT_B: 
   385	        case OpCode::GE_B:  case OpCode::LT_B:  case OpCode::LE_B:
   386	        case OpCode::BIT_AND_B: case OpCode::BIT_OR_B: case OpCode::BIT_XOR_B:
   387	        case OpCode::LSHIFT_B:  case OpCode::RSHIFT_B:
   388	        case OpCode::MOVE_B:
   389	            return 1 + 3; // Op(1) + Dst(1) + R1(1) + R2(1) = 4 bytes
   390	            
   391	        case OpCode::LOAD_INT_B:
   392	            return 1 + 1 + 1; // Op(1) + Dst(1) + Val(1) = 3 bytes
   393	
   394	        case OpCode::JUMP_IF_TRUE_B:
   395	        case OpCode::JUMP_IF_FALSE_B:
   396	            return 1 + 1 + 2; // Op(1) + Cond(1) + Offset(2) = 4 bytes
   397	
   398	        // 3. Các lệnh Load hằng số lớn (64-bit)
   399	        case OpCode::LOAD_INT: 
   400	        case OpCode::LOAD_FLOAT: 
   401	            return 1 + 2 + 8; // Op(1) + Dst(2) + Value(8) = 11 bytes
   402	
   403	        // 4. Các lệnh Jump (Offset 16-bit)
   404	        case OpCode::JUMP: 
   405	            return 1 + 2; // Op + Offset = 3 bytes
   406	            
   407	        case OpCode::JUMP_IF_TRUE: 
   408	        case OpCode::JUMP_IF_FALSE: 
   409	            return 1 + 2 + 2; // Op + Cond + Offset = 5 bytes (Struct JumpCondArgs)
   410	
   411	        case OpCode::SETUP_TRY:
   412	            return 1 + 2 + 2; // Op + Offset + CatchReg = 5 bytes
   413	
   414	        // 5. Default case (Dựa vào arity)
   415	        default: {
   416	            int arity = Assembler::get_arity(op);
   417	            return 1 + (arity * 2);
   418	        }
   419	    }
   420	}
   421	
   422	void Assembler::optimize() {
   423	    std::cout << "[MASM] Starting Optimization Pass..." << std::endl;
   424	    int optimized_count = 0;
   425	
   426	    for (auto& proto : protos_) {
   427	        std::vector<uint8_t>& code = proto.bytecode;
   428	        size_t ip = 0;
   429	
   430	        while (ip < code.size()) {
   431	            meow::OpCode op = static_cast<meow::OpCode>(code[ip]);
   432	            size_t instr_size = get_instr_size(op); // GET_PROP = 55 bytes
   433	
   434	            if (op == meow::OpCode::GET_PROP) {
   435	                size_t next_ip = ip + instr_size;
   436	
   437	                if (next_ip < code.size()) {
   438	                    uint8_t next_op_raw = code[next_ip];
   439	                    meow::OpCode next_op = static_cast<meow::OpCode>(next_op_raw);
   440	
   441	                    // Decode GET_PROP: [OP] [Dst] [Obj] [Name] [Cache]
   442	                    uint16_t prop_dst = (uint16_t)code[ip+1] | ((uint16_t)code[ip+2] << 8);
   443	                    uint16_t obj_reg  = (uint16_t)code[ip+3] | ((uint16_t)code[ip+4] << 8);
   444	                    uint16_t name_idx = (uint16_t)code[ip+5] | ((uint16_t)code[ip+6] << 8);
   445	
   446	                    // ---------------------------------------------------------
   447	                    // TRƯỜNG HỢP 1: GET_PROP -> CALL (Chuẩn, 80 bytes)
   448	                    // ---------------------------------------------------------
   449	                    if (next_op == meow::OpCode::CALL) {
   450	                        size_t call_base = next_ip;
   451	                        // CALL: [OP] [Dst] [Fn] ...
   452	                        uint16_t call_dst = (uint16_t)code[call_base+1] | ((uint16_t)code[call_base+2] << 8);
   453	                        uint16_t fn_reg   = (uint16_t)code[call_base+3] | ((uint16_t)code[call_base+4] << 8);
   454	                        uint16_t arg_start= (uint16_t)code[call_base+5] | ((uint16_t)code[call_base+6] << 8);
   455	                        uint16_t argc     = (uint16_t)code[call_base+7] | ((uint16_t)code[call_base+8] << 8);
   456	
   457	                        if (prop_dst == fn_reg) {
   458	                            std::cout << "[MASM] MERGED (Direct): GET_PROP + CALL -> INVOKE at " << ip << "\n";
   459	                            
   460	                            // Ghi đè INVOKE (80 bytes)
   461	                            size_t write = ip;
   462	                            code[write++] = static_cast<uint8_t>(meow::OpCode::INVOKE);
   463	                            
   464	                            auto emit_u16 = [&](uint16_t v) { code[write++] = v & 0xFF; code[write++] = (v >> 8) & 0xFF; };
   465	                            emit_u16(call_dst); emit_u16(obj_reg); emit_u16(name_idx); emit_u16(arg_start); emit_u16(argc);
   466	                            
   467	                            std::memset(&code[write], 0, 48); write += 48; // IC
   468	                            std::memset(&code[write], 0, 21); // Padding (80 - 11 - 48 = 21)
   469	                            
   470	                            ip += 80; optimized_count++; continue;
   471	                        }
   472	                    }
   473	                    // ---------------------------------------------------------
   474	                    // TRƯỜNG HỢP 2: GET_PROP -> MOVE -> CALL (Có rác, 85 bytes)
   475	                    // ---------------------------------------------------------
   476	                    else if (next_op == meow::OpCode::MOVE) {
   477	                        size_t move_size = 5; // OP(1) + DST(2) + SRC(2)
   478	                        size_t call_ip = next_ip + move_size;
   479	
   480	                        if (call_ip < code.size() && static_cast<meow::OpCode>(code[call_ip]) == meow::OpCode::CALL) {
   481	                            // Decode MOVE: [OP] [Dst] [Src]
   482	                            uint16_t move_dst = (uint16_t)code[next_ip+1] | ((uint16_t)code[next_ip+2] << 8);
   483	                            uint16_t move_src = (uint16_t)code[next_ip+3] | ((uint16_t)code[next_ip+4] << 8);
   484	
   485	                            // Decode CALL
   486	                            uint16_t call_dst = (uint16_t)code[call_ip+1] | ((uint16_t)code[call_ip+2] << 8);
   487	                            uint16_t fn_reg   = (uint16_t)code[call_ip+3] | ((uint16_t)code[call_ip+4] << 8);
   488	                            uint16_t arg_start= (uint16_t)code[call_ip+5] | ((uint16_t)code[call_ip+6] << 8);
   489	                            uint16_t argc     = (uint16_t)code[call_ip+7] | ((uint16_t)code[call_ip+8] << 8);
   490	
   491	                            // Logic check: GET_PROP -> rA, MOVE rB, rA, CALL rB
   492	                            if (prop_dst == move_src && move_dst == fn_reg) {
   493	                                std::cout << "[MASM] MERGED (Jump Move): GET_PROP + MOVE + CALL -> INVOKE at " << ip << "\n";
   494	
   495	                                // 1. Ghi INVOKE (80 bytes)
   496	                                size_t write = ip;
   497	                                code[write++] = static_cast<uint8_t>(meow::OpCode::INVOKE);
   498	                                
   499	                                auto emit_u16 = [&](uint16_t v) { code[write++] = v & 0xFF; code[write++] = (v >> 8) & 0xFF; };
   500	                                emit_u16(call_dst); emit_u16(obj_reg); emit_u16(name_idx); emit_u16(arg_start); emit_u16(argc);
   501	                                
   502	                                std::memset(&code[write], 0, 48); write += 48;
   503	                                std::memset(&code[write], 0, 21); write += 21;
   504	
   505	                                // 2. Xử lý 5 bytes thừa (MOVE cũ)
   506	                                // Tổng block cũ là 55 + 5 + 25 = 85 bytes.
   507	                                // INVOKE mới dùng 80 bytes. Còn dư 5 bytes cuối cùng.
   508	                                // Ta ghi đè 5 bytes này bằng lệnh MOVE r0, r0 (No-Op) để giữ alignment.
   509	                                
   510	                                code[write++] = static_cast<uint8_t>(meow::OpCode::MOVE);
   511	                                emit_u16(0); // Dst r0
   512	                                emit_u16(0); // Src r0
   513	                                
   514	                                ip += 85; optimized_count++; continue;
   515	                            }
   516	                        }
   517	                    }
   518	                }
   519	            }
   520	            ip += instr_size;
   521	        }
   522	    }
   523	    std::cout << "[MASM] Optimization finished. Total merged: " << optimized_count << std::endl;
   524	}
   525	
   526	std::vector<uint8_t> Assembler::assemble() {
   527	    while (!is_at_end()) {
   528	        parse_statement();
   529	    }
   530	    link_proto_refs();
   531	    patch_labels();
   532	    // optimize();
   533	    return serialize_binary();
   534	}
   535	
   536	void Assembler::assemble_to_file(const std::string& output_file) {
   537	    auto buffer = assemble();
   538	    std::ofstream out(output_file, std::ios::binary);
   539	    if (!out) throw std::runtime_error("Cannot open output file");
   540	    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
   541	    out.close();
   542	}
   543	
   544	} // namespace meow::masm


// =============================================================================
//  FILE PATH: src/tools/masm/src/lexer.cpp
// =============================================================================

     1	#include <meow/masm/lexer.h>
     2	#include <meow/bytecode/op_codes.h>
     3	#include <cctype>
     4	#include <string_view>
     5	#include <utility>
     6	
     7	namespace meow::masm {
     8	
     9	std::unordered_map<std::string_view, meow::OpCode> OP_MAP;
    10	
    11	namespace {
    12	    template <auto V>
    13	    consteval std::string_view get_raw_name() {
    14	#if defined(__clang__) || defined(__GNUC__)
    15	        return __PRETTY_FUNCTION__;
    16	#elif defined(_MSC_VER)
    17	        return __FUNCSIG__;
    18	#else
    19	        return "";
    20	#endif
    21	    }
    22	
    23	    template <auto V>
    24	    consteval std::string_view get_enum_name() {
    25	        constexpr std::string_view raw = get_raw_name<V>();
    26	        
    27	#if defined(__clang__) || defined(__GNUC__)
    28	        constexpr auto end_pos = raw.size() - 1;
    29	        constexpr auto last_colon = raw.find_last_of(':', end_pos);
    30	        if (last_colon == std::string_view::npos) return ""; 
    31	        return raw.substr(last_colon + 1, end_pos - (last_colon + 1));
    32	#else
    33	        return "UNKNOWN";
    34	#endif
    35	    }
    36	    template <size_t... Is>
    37	    void build_map_impl(std::index_sequence<Is...>) {
    38	        (..., (OP_MAP[get_enum_name<static_cast<meow::OpCode>(Is)>()] = static_cast<meow::OpCode>(Is)));
    39	    }
    40	}
    41	
    42	void init_op_map() {
    43	    if (!OP_MAP.empty()) return;
    44	
    45	    constexpr size_t Count = static_cast<size_t>(meow::OpCode::TOTAL_OPCODES);
    46	    build_map_impl(std::make_index_sequence<Count>{});
    47	
    48	    OP_MAP.erase("__BEGIN_OPERATOR__");
    49	    OP_MAP.erase("__END_OPERATOR__");
    50	}
    51	
    52	std::vector<Token> Lexer::tokenize() {
    53	    std::vector<Token> tokens;
    54	    while (!is_at_end()) {
    55	        char c = peek();
    56	        if (isspace(c)) {
    57	            if (c == '\n') line_++;
    58	            advance();
    59	            continue;
    60	        }
    61	        if (c == '#') {
    62	            while (peek() != '\n' && !is_at_end()) advance();
    63	            continue;
    64	        }
    65	        if (c == '.') { tokens.push_back(scan_directive()); continue; }
    66	        if (c == '"' || c == '\'') { tokens.push_back(scan_string()); continue; }
    67	        if (isdigit(c) || (c == '-' && isdigit(peek(1)))) { tokens.push_back(scan_number()); continue; }
    68	        if (isalpha(c) || c == '_' || c == '@') { tokens.push_back(scan_identifier()); continue; }
    69	        advance();
    70	    }
    71	    tokens.push_back({TokenType::END_OF_FILE, "", line_});
    72	    return tokens;
    73	}
    74	
    75	bool Lexer::is_at_end() const { return pos_ >= src_.size(); }
    76	char Lexer::peek(int offset) const { 
    77	    if (pos_ + offset >= src_.size()) return '\0';
    78	    return src_[pos_ + offset]; 
    79	}
    80	char Lexer::advance() { return src_[pos_++]; }
    81	
    82	Token Lexer::scan_directive() {
    83	    size_t start = pos_;
    84	    advance(); 
    85	    while (isalnum(peek()) || peek() == '_' || peek() == '-' || peek() == '/' || peek() == '.') advance();
    86	    
    87	    std::string_view text = src_.substr(start, pos_ - start);
    88	    
    89	    TokenType type = TokenType::IDENTIFIER; 
    90	    
    91	    if (text == ".func") type = TokenType::DIR_FUNC;
    92	    else if (text == ".endfunc") type = TokenType::DIR_ENDFUNC;
    93	    else if (text == ".registers") type = TokenType::DIR_REGISTERS;
    94	    else if (text == ".upvalues") type = TokenType::DIR_UPVALUES;
    95	    else if (text == ".upvalue") type = TokenType::DIR_UPVALUE;
    96	    else if (text == ".const") type = TokenType::DIR_CONST;
    97	    
    98	    return {type, text, line_};
    99	}
   100	
   101	Token Lexer::scan_string() {
   102	    char quote = advance();
   103	    size_t start = pos_ - 1; 
   104	    while (peek() != quote && !is_at_end()) {
   105	        if (peek() == '\\') advance();
   106	        advance();
   107	    }
   108	    if (!is_at_end()) advance();
   109	    return {TokenType::STRING, src_.substr(start, pos_ - start), line_};
   110	}
   111	
   112	Token Lexer::scan_number() {
   113	    size_t start = pos_;
   114	    if (peek() == '-') advance();
   115	    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
   116	        advance(); advance();
   117	        while (isxdigit(peek())) advance();
   118	        return {TokenType::NUMBER_INT, src_.substr(start, pos_ - start), line_};
   119	    }
   120	    bool is_float = false;
   121	    while (isdigit(peek())) advance();
   122	    if (peek() == '.' && isdigit(peek(1))) {
   123	        is_float = true;
   124	        advance();
   125	        while (isdigit(peek())) advance();
   126	    }
   127	    return {is_float ? TokenType::NUMBER_FLOAT : TokenType::NUMBER_INT, src_.substr(start, pos_ - start), line_};
   128	}
   129	
   130	Token Lexer::scan_identifier() {
   131	    size_t start = pos_;
   132	    while (isalnum(peek()) || peek() == '_' || peek() == '@' || peek() == '/' || peek() == '-' || peek() == '.') advance();
   133	    
   134	    if (peek() == ':') {
   135	        advance(); 
   136	        return {TokenType::LABEL_DEF, src_.substr(start, pos_ - start - 1), line_};
   137	    }
   138	    std::string_view text = src_.substr(start, pos_ - start);
   139	    
   140	    if (OP_MAP.count(text)) return {TokenType::OPCODE, text, line_};
   141	    
   142	    return {TokenType::IDENTIFIER, text, line_};
   143	}
   144	
   145	} // namespace meow::masm


// =============================================================================
//  FILE PATH: src/tools/masm/src/main.cpp
// =============================================================================

     1	#include <meow/masm/common.h>
     2	#include <meow/masm/lexer.h>
     3	#include <meow/masm/assembler.h>
     4	#include <iostream>
     5	#include <fstream>
     6	#include <vector>
     7	
     8	using namespace meow::masm;
     9	
    10	int main(int argc, char* argv[]) {
    11	    if (argc < 2) {
    12	        std::cout << "Usage: masm <input.meow> [output.meowb]\n";
    13	        return 1;
    14	    }
    15	
    16	    // Khởi tạo bảng opcode (quan trọng!)
    17	    init_op_map();
    18	
    19	    std::string input_path = argv[1];
    20	    std::string output_path = (argc >= 3) ? argv[2] : "out.meowc";
    21	    
    22	    if (argc < 3 && input_path.size() > 6 && input_path.ends_with(".meowb")) {
    23	        output_path = input_path.substr(0, input_path.size()-6) + ".meowc";
    24	    }
    25	
    26	    std::ifstream f(input_path, std::ios::ate); // Mở ở cuối để lấy size
    27	    if (!f) {
    28	        std::cerr << "Cannot open input file: " << input_path << "\n";
    29	        return 1;
    30	    }
    31	    
    32	    std::streamsize size = f.tellg();
    33	    f.seekg(0, std::ios::beg);
    34	
    35	    std::string source(size, '\0');
    36	    if (!f.read(&source[0], size)) {
    37	         std::cerr << "Read error\n";
    38	         return 1;
    39	    }
    40	    f.close();
    41	
    42	    // Lexing
    43	    Lexer lexer(source);
    44	    auto tokens = lexer.tokenize();
    45	
    46	    // Assembling
    47	    try {
    48	        Assembler asm_tool(tokens);
    49	        asm_tool.assemble_to_file(output_path);
    50	    } catch (const std::exception& e) {
    51	        std::cerr << "[Assembly Error] " << e.what() << "\n";
    52	        return 1;
    53	    }
    54	
    55	    return 0;
    56	}


