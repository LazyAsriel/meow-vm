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
    17	    // --- State Management ---
    18	    // Trạng thái cờ toàn cục (áp dụng cho các hàm mới tạo)
    19	    ProtoFlags global_flags_ = ProtoFlags::NONE;
    20	
    21	public:
    22	    explicit Assembler(const std::vector<Token>& tokens);
    23	
    24	    std::vector<uint8_t> assemble();
    25	    void assemble_to_file(const std::string& output_file);
    26	    static int get_arity(meow::OpCode op);
    27	
    28	private:
    29	    Token peek() const;
    30	    Token previous() const;
    31	    bool is_at_end() const;
    32	    Token advance();
    33	    Token consume(TokenType type, const std::string& msg);
    34	
    35	    // Parsing
    36	    void parse_statement();
    37	    void parse_func();
    38	    void parse_registers();
    39	    void parse_upvalues_decl();
    40	    void parse_upvalue_def();
    41	    void parse_const();
    42	    void parse_label();
    43	    void parse_instruction();
    44	    
    45	    // --- Annotation Handler ---
    46	    void parse_annotation(); 
    47	
    48	    void optimize();
    49	    std::string parse_string_literal(std::string_view sv);
    50	
    51	    // Emit bytecode helpers
    52	    void emit_byte(uint8_t b);
    53	    void emit_u16(uint16_t v);
    54	    void emit_u32(uint32_t v);
    55	    void emit_u64(uint64_t v);
    56	
    57	    // Finalize
    58	    void link_proto_refs();
    59	    void patch_labels();
    60	    std::vector<uint8_t> serialize_binary();
    61	};
    62	
    63	} // namespace meow::masm


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
    18	    
    19	    // --- Token mới ---
    20	    DEBUG_INFO, // #^ "file" line:col
    21	    ANNOTATION, // #@ directive
    22	
    23	    END_OF_FILE, UNKNOWN
    24	};
    25	
    26	struct Token {
    27	    TokenType type;
    28	    std::string_view lexeme;
    29	    size_t line;
    30	};
    31	
    32	enum class ConstType { NULL_T, INT_T, FLOAT_T, STRING_T, PROTO_REF_T };
    33	
    34	struct Constant {
    35	    ConstType type;
    36	    int64_t val_i64 = 0;
    37	    double val_f64 = 0.0;
    38	    std::string val_str;
    39	    uint32_t proto_index = 0; 
    40	};
    41	
    42	struct UpvalueInfo {
    43	    bool is_local;
    44	    uint32_t index;
    45	};
    46	
    47	// Cấu trúc lưu vị trí dòng code
    48	struct LineInfo {
    49	    uint32_t offset;    // Bytecode offset
    50	    uint32_t line;
    51	    uint32_t col;
    52	    uint32_t file_idx;  // Index vào bảng tên file
    53	};
    54	
    55	// Enum cờ cho Prototype (Dùng bitmask)
    56	enum class ProtoFlags : uint8_t {
    57	    NONE = 0,
    58	    HAS_DEBUG_INFO = 1 << 0, // Bit 0: Có debug info
    59	    IS_VARARG      = 1 << 1  // Bit 1: Hàm variadic (Dành cho tương lai)
    60	};
    61	
    62	// Operator overloading để dùng phép bitwise với enum class
    63	inline ProtoFlags operator|(ProtoFlags a, ProtoFlags b) {
    64	    return static_cast<ProtoFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    65	}
    66	inline ProtoFlags operator&(ProtoFlags a, ProtoFlags b) {
    67	    return static_cast<ProtoFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    68	}
    69	inline ProtoFlags operator~(ProtoFlags a) {
    70	    return static_cast<ProtoFlags>(~static_cast<uint8_t>(a));
    71	}
    72	inline bool has_flag(ProtoFlags flags, ProtoFlags check) {
    73	    return (flags & check) != ProtoFlags::NONE;
    74	}
    75	
    76	struct Prototype {
    77	    std::string name;
    78	    uint32_t num_regs = 0;
    79	    uint32_t num_upvalues = 0;
    80	    
    81	    // --- Flags ---
    82	    ProtoFlags flags = ProtoFlags::NONE; 
    83	
    84	    std::vector<Constant> constants;
    85	    std::vector<UpvalueInfo> upvalues;
    86	    std::vector<uint8_t> bytecode;
    87	
    88	    // --- Debug Info Storage ---
    89	    std::vector<LineInfo> lines;
    90	    std::vector<std::string> source_files;
    91	    std::unordered_map<std::string, uint32_t> file_map; 
    92	
    93	    std::unordered_map<std::string, size_t> labels;
    94	    std::vector<std::pair<size_t, std::string>> jump_patches;
    95	    std::vector<std::pair<size_t, std::string>> try_patches;
    96	
    97	    // Helper: Thêm file và trả về index (có deduplicate)
    98	    uint32_t add_file(const std::string& file) {
    99	        if (file_map.count(file)) return file_map[file];
   100	        uint32_t idx = source_files.size();
   101	        source_files.push_back(file);
   102	        file_map[file] = idx;
   103	        return idx;
   104	    }
   105	};
   106	
   107	} // namespace meow::masm


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
    25	    
    26	    // --- Hàm mới ---
    27	    Token scan_debug_info(); // Xử lý #^
    28	    Token scan_annotation(); // Xử lý #@
    29	};
    30	
    31	} // namespace meow::masm


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
     8	#include <sstream>
     9	#include <iomanip> // cho std::quoted
    10	
    11	namespace meow::masm {
    12	
    13	Assembler::Assembler(const std::vector<Token>& tokens) : tokens_(tokens) {}
    14	
    15	Token Assembler::peek() const { return tokens_[current_]; }
    16	Token Assembler::previous() const { return tokens_[current_ - 1]; }
    17	bool Assembler::is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
    18	Token Assembler::advance() { if (!is_at_end()) current_++; return previous(); }
    19	
    20	Token Assembler::consume(TokenType type, const std::string& msg) {
    21	    if (peek().type == type) return advance();
    22	    throw std::runtime_error(msg + " (Found: " + std::string(peek().lexeme) + " at line " + std::to_string(peek().line) + ")");
    23	}
    24	
    25	void Assembler::parse_statement() {
    26	    Token tk = peek();
    27	    switch (tk.type) {
    28	        case TokenType::DIR_FUNC:      parse_func(); break;
    29	        case TokenType::DIR_REGISTERS: parse_registers(); break;
    30	        case TokenType::DIR_UPVALUES:  parse_upvalues_decl(); break;
    31	        case TokenType::DIR_UPVALUE:   parse_upvalue_def(); break;
    32	        case TokenType::DIR_CONST:     parse_const(); break;
    33	        case TokenType::LABEL_DEF:     parse_label(); break;
    34	        case TokenType::OPCODE:        parse_instruction(); break;
    35	        case TokenType::DIR_ENDFUNC:   advance(); curr_proto_ = nullptr; break;
    36	        
    37	        // --- Xử lý Annotation ở cả trong và ngoài hàm ---
    38	        case TokenType::ANNOTATION:    parse_annotation(); break; 
    39	        
    40	        case TokenType::IDENTIFIER:    throw std::runtime_error("Unexpected identifier: " + std::string(tk.lexeme));
    41	        default: throw std::runtime_error("Unexpected token: " + std::string(tk.lexeme));
    42	    }
    43	}
    44	
    45	// Xử lý logic bật/tắt Flags từ directive #@
    46	void Assembler::parse_annotation() {
    47	    Token ann = advance(); // Consume ANNOTATION token
    48	    std::string key(ann.lexeme);
    49	
    50	    // Nếu đang trong hàm -> sửa flags của hàm. Nếu không -> sửa global flags.
    51	    ProtoFlags* target = (curr_proto_) ? &curr_proto_->flags : &global_flags_;
    52	
    53	    if (key == "debug") {
    54	        *target = *target | ProtoFlags::HAS_DEBUG_INFO;
    55	    } 
    56	    else if (key == "no_debug") {
    57	        *target = *target & (~ProtoFlags::HAS_DEBUG_INFO);
    58	    }
    59	    else if (key == "vararg") {
    60	        *target = *target | ProtoFlags::IS_VARARG;
    61	    }
    62	    else {
    63	        std::cout << "[MASM] Warning: Unknown annotation '#@ " << key << "'" << std::endl;
    64	    }
    65	}
    66	
    67	void Assembler::parse_func() {
    68	    advance(); 
    69	    Token name = consume(TokenType::IDENTIFIER, "Expected func name");
    70	    std::string func_name(name.lexeme);
    71	    if (func_name.starts_with("@")) func_name = func_name.substr(1);
    72	    protos_.emplace_back();
    73	    protos_.back().name = func_name;
    74	    
    75	    curr_proto_ = &protos_.back();
    76	    
    77	    // --- Kế thừa cờ từ global state ---
    78	    curr_proto_->flags = global_flags_;
    79	
    80	    proto_name_map_[func_name] = protos_.size() - 1;
    81	}
    82	
    83	void Assembler::parse_registers() {
    84	    if (!curr_proto_) throw std::runtime_error("Outside .func");
    85	    advance();
    86	    Token num = consume(TokenType::NUMBER_INT, "Expected number");
    87	    curr_proto_->num_regs = std::stoi(std::string(num.lexeme));
    88	}
    89	
    90	void Assembler::parse_upvalues_decl() {
    91	    if (!curr_proto_) throw std::runtime_error("Outside .func");
    92	    advance();
    93	    Token num = consume(TokenType::NUMBER_INT, "Expected number");
    94	    curr_proto_->num_upvalues = std::stoi(std::string(num.lexeme));
    95	    curr_proto_->upvalues.resize(curr_proto_->num_upvalues);
    96	}
    97	
    98	void Assembler::parse_upvalue_def() {
    99	    if (!curr_proto_) throw std::runtime_error("Outside .func");
   100	    advance();
   101	    uint32_t idx = std::stoi(std::string(consume(TokenType::NUMBER_INT, "Idx").lexeme));
   102	    std::string type(consume(TokenType::IDENTIFIER, "Type").lexeme);
   103	    uint32_t slot = std::stoi(std::string(consume(TokenType::NUMBER_INT, "Slot").lexeme));
   104	    if (idx < curr_proto_->upvalues.size()) curr_proto_->upvalues[idx] = { (type == "local"), slot };
   105	}
   106	
   107	void Assembler::parse_const() {
   108	    if (!curr_proto_) throw std::runtime_error("Outside .func");
   109	    advance();
   110	    Constant c;
   111	    Token tk = peek();
   112	    
   113	    if (tk.type == TokenType::STRING) {
   114	        c.type = ConstType::STRING_T;
   115	        c.val_str = parse_string_literal(tk.lexeme);
   116	        advance();
   117	    } else if (tk.type == TokenType::NUMBER_INT) {
   118	        c.type = ConstType::INT_T;
   119	        std::string_view sv = tk.lexeme;
   120	        if (sv.starts_with("0x") || sv.starts_with("0X")) {
   121	            c.val_i64 = std::stoll(std::string(sv), nullptr, 16);
   122	        } else {
   123	            std::from_chars(sv.data(), sv.data() + sv.size(), c.val_i64);
   124	        }
   125	        advance();
   126	    } else if (tk.type == TokenType::NUMBER_FLOAT) {
   127	        c.type = ConstType::FLOAT_T;
   128	        c.val_f64 = std::stod(std::string(tk.lexeme));
   129	        advance();
   130	    } else if (tk.type == TokenType::IDENTIFIER) {
   131	        if (tk.lexeme == "null") { c.type = ConstType::NULL_T; advance(); }
   132	        else if (tk.lexeme == "true") { c.type = ConstType::INT_T; c.val_i64 = 1; advance(); } 
   133	        else if (tk.lexeme == "false") { c.type = ConstType::INT_T; c.val_i64 = 0; advance(); }
   134	        else if (tk.lexeme.starts_with("@")) {
   135	            c.type = ConstType::PROTO_REF_T;
   136	            c.val_str = tk.lexeme.substr(1);
   137	            advance();
   138	        } else throw std::runtime_error("Unknown constant identifier: " + std::string(tk.lexeme));
   139	    } else throw std::runtime_error("Invalid constant");
   140	    curr_proto_->constants.push_back(c);
   141	}
   142	
   143	void Assembler::parse_label() {
   144	    Token lbl = advance();
   145	    std::string labelName(lbl.lexeme); 
   146	    curr_proto_->labels[labelName] = curr_proto_->bytecode.size();
   147	}
   148	
   149	void Assembler::emit_byte(uint8_t b) { curr_proto_->bytecode.push_back(b); }
   150	void Assembler::emit_u16(uint16_t v) { emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); }
   151	void Assembler::emit_u32(uint32_t v) { 
   152	    emit_byte(v & 0xFF); emit_byte((v >> 8) & 0xFF); 
   153	    emit_byte((v >> 16) & 0xFF); emit_byte((v >> 24) & 0xFF); 
   154	}
   155	void Assembler::emit_u64(uint64_t v) { for(int i=0; i<8; ++i) emit_byte((v >> (i*8)) & 0xFF); }
   156	
   157	void Assembler::parse_instruction() {
   158	    if (!curr_proto_) throw std::runtime_error("Instruction outside .func");
   159	    
   160	    // Lưu lại offset của lệnh hiện tại
   161	    uint32_t current_offset = curr_proto_->bytecode.size();
   162	
   163	    Token op_tok = advance();
   164	    if (OP_MAP.find(op_tok.lexeme) == OP_MAP.end()) {
   165	        throw std::runtime_error("Unknown opcode: " + std::string(op_tok.lexeme));
   166	    }
   167	    meow::OpCode op = OP_MAP[op_tok.lexeme];
   168	    emit_byte(static_cast<uint8_t>(op));
   169	
   170	    auto parse_u16 = [&]() {
   171	        Token t = consume(TokenType::NUMBER_INT, "Expected u16");
   172	        emit_u16(static_cast<uint16_t>(std::stoi(std::string(t.lexeme))));
   173	    };
   174	
   175	    switch (op) {
   176	        case meow::OpCode::LOAD_INT: {
   177	            parse_u16(); // dst
   178	            Token t = consume(TokenType::NUMBER_INT, "Expected int64");
   179	            int64_t val;
   180	            std::from_chars(t.lexeme.data(), t.lexeme.data() + t.lexeme.size(), val);
   181	            emit_u64(std::bit_cast<uint64_t>(val));
   182	            break;
   183	        }
   184	        case meow::OpCode::LOAD_FLOAT: {
   185	            parse_u16(); // dst
   186	            Token t = consume(TokenType::NUMBER_FLOAT, "Expected double");
   187	            double val = std::stod(std::string(t.lexeme));
   188	            emit_u64(std::bit_cast<uint64_t>(val));
   189	            break;
   190	        }
   191	        case meow::OpCode::JUMP: case meow::OpCode::SETUP_TRY: {
   192	            if (peek().type == TokenType::IDENTIFIER) {
   193	                Token target = advance();
   194	                curr_proto_->try_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
   195	                emit_u16(0xFFFF);
   196	                if (op == meow::OpCode::SETUP_TRY) parse_u16(); 
   197	            } else {
   198	                parse_u16();
   199	                if (op == meow::OpCode::SETUP_TRY) parse_u16();
   200	            }
   201	            break;
   202	        }
   203	        case meow::OpCode::JUMP_IF_FALSE: case meow::OpCode::JUMP_IF_TRUE: {
   204	            parse_u16(); // reg
   205	            if (peek().type == TokenType::IDENTIFIER) {
   206	                Token target = advance();
   207	                curr_proto_->jump_patches.push_back({curr_proto_->bytecode.size(), std::string(target.lexeme)});
   208	                emit_u16(0xFFFF);
   209	            } else parse_u16();
   210	            break;
   211	        }
   212	        case meow::OpCode::GET_PROP: case meow::OpCode::SET_PROP: {
   213	            for(int i=0; i<3; ++i) parse_u16();
   214	            for (int j = 0; j < 4; ++j) { emit_u64(0); emit_u32(0); } // Cache
   215	            break;
   216	        }
   217	        case meow::OpCode::CALL: case meow::OpCode::TAIL_CALL: {
   218	            for(int i=0; i<4; ++i) parse_u16();
   219	            emit_u64(0); emit_u64(0); // Cache
   220	            break;
   221	        }
   222	        case meow::OpCode::CALL_VOID: {
   223	            for(int i=0; i<3; ++i) parse_u16();
   224	            emit_u64(0); emit_u64(0); 
   225	            break;
   226	        }
   227	        default: {
   228	            int args = get_arity(op);
   229	            for(int i=0; i<args; ++i) parse_u16();
   230	            break;
   231	        }
   232	    }
   233	
   234	    // --- XỬ LÝ DEBUG INFO (#^) ---
   235	    // Parse và lưu vào struct, nhưng việc ghi ra file phụ thuộc vào Flags
   236	    if (peek().type == TokenType::DEBUG_INFO) {
   237	        Token dbg = advance();
   238	        
   239	        // Format: "file" line:col
   240	        std::string s(dbg.lexeme);
   241	        std::stringstream ss(s);
   242	        std::string file_part;
   243	        char c;
   244	        
   245	        // Bỏ qua khoảng trắng đầu string nếu có
   246	        if (ss >> std::ws && ss.peek() == '"') {
   247	             ss >> std::quoted(file_part);
   248	        }
   249	        
   250	        uint32_t line = 0, col = 0;
   251	        ss >> line >> c >> col; 
   252	        
   253	        // Lưu vào proto
   254	        uint32_t file_idx = curr_proto_->add_file(file_part);
   255	        curr_proto_->lines.push_back({current_offset, line, col, file_idx});
   256	    }
   257	}
   258	
   259	int Assembler::get_arity(meow::OpCode op) {
   260	    switch (op) {
   261	        case meow::OpCode::CLOSE_UPVALUES: case meow::OpCode::IMPORT_ALL: case meow::OpCode::THROW: 
   262	        case meow::OpCode::RETURN: case meow::OpCode::LOAD_NULL: case meow::OpCode::LOAD_TRUE: case meow::OpCode::LOAD_FALSE:
   263	        case meow::OpCode::INC: case meow::OpCode::DEC:
   264	            return 1;
   265	        case meow::OpCode::LOAD_CONST: case meow::OpCode::MOVE: 
   266	        case meow::OpCode::NEG: case meow::OpCode::NOT: case meow::OpCode::BIT_NOT: 
   267	        case meow::OpCode::GET_UPVALUE: case meow::OpCode::SET_UPVALUE: 
   268	        case meow::OpCode::CLOSURE:
   269	        case meow::OpCode::NEW_CLASS: case meow::OpCode::NEW_INSTANCE: 
   270	        case meow::OpCode::IMPORT_MODULE:
   271	        case meow::OpCode::EXPORT: 
   272	        case meow::OpCode::GET_KEYS: case meow::OpCode::GET_VALUES:
   273	        case meow::OpCode::GET_SUPER: 
   274	        case meow::OpCode::GET_GLOBAL: case meow::OpCode::SET_GLOBAL:
   275	        case meow::OpCode::INHERIT:
   276	            return 2;
   277	        case meow::OpCode::GET_EXPORT: 
   278	        case meow::OpCode::ADD: case meow::OpCode::SUB: case meow::OpCode::MUL: case meow::OpCode::DIV:
   279	        case meow::OpCode::MOD: case meow::OpCode::POW: case meow::OpCode::EQ: case meow::OpCode::NEQ:
   280	        case meow::OpCode::GT: case meow::OpCode::GE: case meow::OpCode::LT: case meow::OpCode::LE:
   281	        case meow::OpCode::BIT_AND: case meow::OpCode::BIT_OR: case meow::OpCode::BIT_XOR:
   282	        case meow::OpCode::LSHIFT: case meow::OpCode::RSHIFT: 
   283	        case meow::OpCode::NEW_ARRAY: case meow::OpCode::NEW_HASH: 
   284	        case meow::OpCode::GET_INDEX: case meow::OpCode::SET_INDEX: 
   285	        case meow::OpCode::GET_PROP: case meow::OpCode::SET_PROP:
   286	        case meow::OpCode::SET_METHOD: case meow::OpCode::CALL_VOID:
   287	            return 3;
   288	        case meow::OpCode::CALL:
   289	        case meow::OpCode::TAIL_CALL:
   290	            return 4;
   291	        case meow::OpCode::INVOKE: return 5;
   292	        default: return 0;
   293	    }
   294	}
   295	
   296	void Assembler::link_proto_refs() {
   297	    for (auto& p : protos_) {
   298	        for (auto& c : p.constants) {
   299	            if (c.type == ConstType::PROTO_REF_T) {
   300	                if (proto_name_map_.count(c.val_str)) c.proto_index = proto_name_map_[c.val_str];
   301	                else throw std::runtime_error("Undefined proto: " + c.val_str);
   302	            }
   303	        }
   304	    }
   305	}
   306	
   307	void Assembler::patch_labels() {
   308	    for (auto& p : protos_) {
   309	        auto apply = [&](auto& patches) {
   310	            for (auto& patch : patches) {
   311	                if (!p.labels.count(patch.second)) throw std::runtime_error("Undefined label: " + patch.second);
   312	                size_t target = p.labels[patch.second];
   313	                p.bytecode[patch.first] = target & 0xFF;
   314	                p.bytecode[patch.first+1] = (target >> 8) & 0xFF;
   315	            }
   316	        };
   317	        apply(p.jump_patches);
   318	        apply(p.try_patches);
   319	    }
   320	}
   321	
   322	std::string Assembler::parse_string_literal(std::string_view sv) {
   323	    if (sv.length() >= 2) sv = sv.substr(1, sv.length() - 2);
   324	    std::string res; res.reserve(sv.length());
   325	    for (size_t i = 0; i < sv.length(); ++i) {
   326	        if (sv[i] == '\\' && i + 1 < sv.length()) {
   327	            char next = sv[++i];
   328	            if (next == 'n') res += '\n'; 
   329	            else if (next == 'r') res += '\r';
   330	            else if (next == 't') res += '\t';
   331	            else if (next == '\\') res += '\\'; 
   332	            else if (next == '"') res += '"';
   333	            else res += next;
   334	        } else res += sv[i];
   335	    }
   336	    return res;
   337	}
   338	
   339	std::vector<uint8_t> Assembler::serialize_binary() {
   340	    std::vector<uint8_t> buffer;
   341	    buffer.reserve(4096); 
   342	
   343	    auto write_u8 = [&](uint8_t v) { buffer.push_back(v); };
   344	    auto write_u32 = [&](uint32_t v) { 
   345	        buffer.push_back(v & 0xFF); buffer.push_back((v >> 8) & 0xFF);
   346	        buffer.push_back((v >> 16) & 0xFF); buffer.push_back((v >> 24) & 0xFF);
   347	    };
   348	    auto write_u64 = [&](uint64_t v) { for(int i=0; i<8; ++i) buffer.push_back((v >> (i*8)) & 0xFF); };
   349	    auto write_f64 = [&](double v) { uint64_t bit; std::memcpy(&bit, &v, 8); write_u64(bit); };
   350	    auto write_str = [&](const std::string& s) { 
   351	        write_u32(s.size()); buffer.insert(buffer.end(), s.begin(), s.end()); 
   352	    };
   353	
   354	    write_u32(0x4D454F57); // Magic
   355	    write_u32(1);          // Version
   356	    if (proto_name_map_.count("main")) write_u32(proto_name_map_["main"]);
   357	    else write_u32(0);
   358	    write_u32(protos_.size()); 
   359	
   360	    for (const auto& p : protos_) {
   361	        write_u32(p.num_regs);
   362	        write_u32(p.num_upvalues);
   363	        
   364	        // --- GHI FLAGS ---
   365	        // Byte này khớp với Loader::read_u8() ở phía VM
   366	        write_u8(static_cast<uint8_t>(p.flags));
   367	        
   368	        std::vector<Constant> write_consts = p.constants;
   369	        write_consts.push_back({ConstType::STRING_T, 0, 0, p.name, 0});
   370	        
   371	        write_u32(write_consts.size() - 1); 
   372	        write_u32(write_consts.size());     
   373	
   374	        for (const auto& c : write_consts) {
   375	            switch (c.type) {
   376	                case ConstType::NULL_T: write_u8(0); break;
   377	                case ConstType::INT_T:  write_u8(1); write_u64(c.val_i64); break;
   378	                case ConstType::FLOAT_T:write_u8(2); write_f64(c.val_f64); break;
   379	                case ConstType::STRING_T:write_u8(3); write_str(c.val_str); break;
   380	                case ConstType::PROTO_REF_T: write_u8(4); write_u32(c.proto_index); break;
   381	            }
   382	        }
   383	        write_u32(p.upvalues.size());
   384	        for (const auto& u : p.upvalues) {
   385	            write_u8(u.is_local ? 1 : 0);
   386	            write_u32(u.index);
   387	        }
   388	        write_u32(p.bytecode.size());
   389	        buffer.insert(buffer.end(), p.bytecode.begin(), p.bytecode.end());
   390	        
   391	        // --- GHI DEBUG TABLES (NẾU CỜ ĐƯỢC BẬT) ---
   392	        if (has_flag(p.flags, ProtoFlags::HAS_DEBUG_INFO)) {
   393	            // Table Source Files
   394	            write_u32(p.source_files.size());
   395	            for (const auto& f : p.source_files) write_str(f);
   396	            
   397	            // Table Lines
   398	            write_u32(p.lines.size());
   399	            for (const auto& l : p.lines) {
   400	                write_u32(l.offset);
   401	                write_u32(l.line);
   402	                write_u32(l.col);
   403	                write_u32(l.file_idx);
   404	            }
   405	        }
   406	    }
   407	    return buffer;
   408	}
   409	
   410	// void Assembler::optimize() {
   411	//     std::cout << "[MASM] Starting Optimization Pass..." << std::endl;
   412	//     int optimized_count = 0;
   413	
   414	//     for (auto& proto : protos_) {
   415	//         std::vector<uint8_t>& code = proto.bytecode;
   416	//         size_t ip = 0;
   417	
   418	//         while (ip < code.size()) {
   419	//             meow::OpCode op = static_cast<meow::OpCode>(code[ip]);
   420	//             size_t instr_size = get_instr_size(op); // GET_PROP = 55 bytes
   421	
   422	//             if (op == meow::OpCode::GET_PROP) {
   423	//                 size_t next_ip = ip + instr_size;
   424	
   425	//                 if (next_ip < code.size()) {
   426	//                     uint8_t next_op_raw = code[next_ip];
   427	//                     meow::OpCode next_op = static_cast<meow::OpCode>(next_op_raw);
   428	
   429	//                     // Decode GET_PROP: [OP] [Dst] [Obj] [Name] [Cache]
   430	//                     uint16_t prop_dst = (uint16_t)code[ip+1] | ((uint16_t)code[ip+2] << 8);
   431	//                     uint16_t obj_reg  = (uint16_t)code[ip+3] | ((uint16_t)code[ip+4] << 8);
   432	//                     uint16_t name_idx = (uint16_t)code[ip+5] | ((uint16_t)code[ip+6] << 8);
   433	
   434	//                     // ---------------------------------------------------------
   435	//                     // TRƯỜNG HỢP 1: GET_PROP -> CALL (Chuẩn, 80 bytes)
   436	//                     // ---------------------------------------------------------
   437	//                     if (next_op == meow::OpCode::CALL) {
   438	//                         size_t call_base = next_ip;
   439	//                         // CALL: [OP] [Dst] [Fn] ...
   440	//                         uint16_t call_dst = (uint16_t)code[call_base+1] | ((uint16_t)code[call_base+2] << 8);
   441	//                         uint16_t fn_reg   = (uint16_t)code[call_base+3] | ((uint16_t)code[call_base+4] << 8);
   442	//                         uint16_t arg_start= (uint16_t)code[call_base+5] | ((uint16_t)code[call_base+6] << 8);
   443	//                         uint16_t argc     = (uint16_t)code[call_base+7] | ((uint16_t)code[call_base+8] << 8);
   444	
   445	//                         if (prop_dst == fn_reg) {
   446	//                             std::cout << "[MASM] MERGED (Direct): GET_PROP + CALL -> INVOKE at " << ip << "\n";
   447	                            
   448	//                             // Ghi đè INVOKE (80 bytes)
   449	//                             size_t write = ip;
   450	//                             code[write++] = static_cast<uint8_t>(meow::OpCode::INVOKE);
   451	                            
   452	//                             auto emit_u16 = [&](uint16_t v) { code[write++] = v & 0xFF; code[write++] = (v >> 8) & 0xFF; };
   453	//                             emit_u16(call_dst); emit_u16(obj_reg); emit_u16(name_idx); emit_u16(arg_start); emit_u16(argc);
   454	                            
   455	//                             std::memset(&code[write], 0, 48); write += 48; // IC
   456	//                             std::memset(&code[write], 0, 21); // Padding (80 - 11 - 48 = 21)
   457	                            
   458	//                             ip += 80; optimized_count++; continue;
   459	//                         }
   460	//                     }
   461	//                     // ---------------------------------------------------------
   462	//                     // TRƯỜNG HỢP 2: GET_PROP -> MOVE -> CALL (Có rác, 85 bytes)
   463	//                     // ---------------------------------------------------------
   464	//                     else if (next_op == meow::OpCode::MOVE) {
   465	//                         size_t move_size = 5; // OP(1) + DST(2) + SRC(2)
   466	//                         size_t call_ip = next_ip + move_size;
   467	
   468	//                         if (call_ip < code.size() && static_cast<meow::OpCode>(code[call_ip]) == meow::OpCode::CALL) {
   469	//                             // Decode MOVE: [OP] [Dst] [Src]
   470	//                             uint16_t move_dst = (uint16_t)code[next_ip+1] | ((uint16_t)code[next_ip+2] << 8);
   471	//                             uint16_t move_src = (uint16_t)code[next_ip+3] | ((uint16_t)code[next_ip+4] << 8);
   472	
   473	//                             // Decode CALL
   474	//                             uint16_t call_dst = (uint16_t)code[call_ip+1] | ((uint16_t)code[call_ip+2] << 8);
   475	//                             uint16_t fn_reg   = (uint16_t)code[call_ip+3] | ((uint16_t)code[call_ip+4] << 8);
   476	//                             uint16_t arg_start= (uint16_t)code[call_ip+5] | ((uint16_t)code[call_ip+6] << 8);
   477	//                             uint16_t argc     = (uint16_t)code[call_ip+7] | ((uint16_t)code[call_ip+8] << 8);
   478	
   479	//                             // Logic check: GET_PROP -> rA, MOVE rB, rA, CALL rB
   480	//                             if (prop_dst == move_src && move_dst == fn_reg) {
   481	//                                 std::cout << "[MASM] MERGED (Jump Move): GET_PROP + MOVE + CALL -> INVOKE at " << ip << "\n";
   482	
   483	//                                 // 1. Ghi INVOKE (80 bytes)
   484	//                                 size_t write = ip;
   485	//                                 code[write++] = static_cast<uint8_t>(meow::OpCode::INVOKE);
   486	                                
   487	//                                 auto emit_u16 = [&](uint16_t v) { code[write++] = v & 0xFF; code[write++] = (v >> 8) & 0xFF; };
   488	//                                 emit_u16(call_dst); emit_u16(obj_reg); emit_u16(name_idx); emit_u16(arg_start); emit_u16(argc);
   489	                                
   490	//                                 std::memset(&code[write], 0, 48); write += 48;
   491	//                                 std::memset(&code[write], 0, 21); write += 21;
   492	
   493	//                                 // 2. Xử lý 5 bytes thừa (MOVE cũ)
   494	//                                 // Tổng block cũ là 55 + 5 + 25 = 85 bytes.
   495	//                                 // INVOKE mới dùng 80 bytes. Còn dư 5 bytes cuối cùng.
   496	//                                 // Ta ghi đè 5 bytes này bằng lệnh MOVE r0, r0 (No-Op) để giữ alignment.
   497	                                
   498	//                                 code[write++] = static_cast<uint8_t>(meow::OpCode::MOVE);
   499	//                                 emit_u16(0); // Dst r0
   500	//                                 emit_u16(0); // Src r0
   501	                                
   502	//                                 ip += 85; optimized_count++; continue;
   503	//                             }
   504	//                         }
   505	//                     }
   506	//                 }
   507	//             }
   508	//             ip += instr_size;
   509	//         }
   510	//     }
   511	//     std::cout << "[MASM] Optimization finished. Total merged: " << optimized_count << std::endl;
   512	// }
   513	
   514	void Assembler::optimize() {
   515	    // Logic optimize nếu có
   516	}
   517	
   518	std::vector<uint8_t> Assembler::assemble() {
   519	    while (!is_at_end()) {
   520	        parse_statement();
   521	    }
   522	    link_proto_refs();
   523	    patch_labels();
   524	    // optimize();
   525	    return serialize_binary();
   526	}
   527	
   528	void Assembler::assemble_to_file(const std::string& output_file) {
   529	    auto buffer = assemble();
   530	    std::ofstream out(output_file, std::ios::binary);
   531	    if (!out) throw std::runtime_error("Cannot open output file");
   532	    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
   533	    out.close();
   534	}
   535	
   536	} // namespace meow::masm


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
    12	    template <auto V> consteval std::string_view get_raw_name() {
    13	#if defined(__clang__) || defined(__GNUC__)
    14	        return __PRETTY_FUNCTION__;
    15	#elif defined(_MSC_VER)
    16	        return __FUNCSIG__;
    17	#else
    18	        return "";
    19	#endif
    20	    }
    21	    template <auto V> consteval std::string_view get_enum_name() {
    22	        constexpr std::string_view raw = get_raw_name<V>();
    23	#if defined(__clang__) || defined(__GNUC__)
    24	        constexpr auto end_pos = raw.size() - 1;
    25	        constexpr auto last_colon = raw.find_last_of(':', end_pos);
    26	        if (last_colon == std::string_view::npos) return ""; 
    27	        return raw.substr(last_colon + 1, end_pos - (last_colon + 1));
    28	#else
    29	        return "UNKNOWN";
    30	#endif
    31	    }
    32	    template <size_t... Is> void build_map_impl(std::index_sequence<Is...>) {
    33	        (..., (OP_MAP[get_enum_name<static_cast<meow::OpCode>(Is)>()] = static_cast<meow::OpCode>(Is)));
    34	    }
    35	}
    36	
    37	void init_op_map() {
    38	    if (!OP_MAP.empty()) return;
    39	    constexpr size_t Count = static_cast<size_t>(meow::OpCode::TOTAL_OPCODES);
    40	    build_map_impl(std::make_index_sequence<Count>{});
    41	    OP_MAP.erase("__BEGIN_OPERATOR__");
    42	    OP_MAP.erase("__END_OPERATOR__");
    43	}
    44	
    45	std::vector<Token> Lexer::tokenize() {
    46	    std::vector<Token> tokens;
    47	    while (!is_at_end()) {
    48	        char c = peek();
    49	        if (isspace(c)) {
    50	            if (c == '\n') line_++;
    51	            advance();
    52	            continue;
    53	        }
    54	        
    55	        // Xử lý các loại comment và annotation bắt đầu bằng '#'
    56	        if (c == '#') {
    57	            if (peek(1) == '^') {
    58	                // Debug Info: #^ "file" line:col
    59	                advance(); advance(); // Consume #^
    60	                tokens.push_back(scan_debug_info());
    61	            } 
    62	            else if (peek(1) == '@') {
    63	                // Annotation: #@ directive
    64	                advance(); advance(); // Consume #@
    65	                tokens.push_back(scan_annotation());
    66	            } 
    67	            else {
    68	                // Comment thường: bỏ qua đến hết dòng
    69	                while (peek() != '\n' && !is_at_end()) advance();
    70	            }
    71	            continue;
    72	        }
    73	
    74	        if (c == '.') { tokens.push_back(scan_directive()); continue; }
    75	        if (c == '"' || c == '\'') { tokens.push_back(scan_string()); continue; }
    76	        if (isdigit(c) || (c == '-' && isdigit(peek(1)))) { tokens.push_back(scan_number()); continue; }
    77	        if (isalpha(c) || c == '_' || c == '@') { tokens.push_back(scan_identifier()); continue; }
    78	        advance();
    79	    }
    80	    tokens.push_back({TokenType::END_OF_FILE, "", line_});
    81	    return tokens;
    82	}
    83	
    84	bool Lexer::is_at_end() const { return pos_ >= src_.size(); }
    85	char Lexer::peek(int offset) const { 
    86	    if (pos_ + offset >= src_.size()) return '\0';
    87	    return src_[pos_ + offset]; 
    88	}
    89	char Lexer::advance() { return src_[pos_++]; }
    90	
    91	// Quét phần còn lại của dòng làm nội dung debug
    92	Token Lexer::scan_debug_info() {
    93	    size_t start = pos_;
    94	    while (peek() != '\n' && !is_at_end()) {
    95	        advance();
    96	    }
    97	    return {TokenType::DEBUG_INFO, src_.substr(start, pos_ - start), line_};
    98	}
    99	
   100	// Quét từ khóa annotation (bỏ qua khoảng trắng đầu)
   101	Token Lexer::scan_annotation() {
   102	    while (peek() == ' ' || peek() == '\t') advance(); // Skip space
   103	    
   104	    size_t start = pos_;
   105	    while (isalnum(peek()) || peek() == '_') advance();
   106	    
   107	    return {TokenType::ANNOTATION, src_.substr(start, pos_ - start), line_};
   108	}
   109	
   110	Token Lexer::scan_directive() {
   111	    size_t start = pos_;
   112	    advance(); 
   113	    while (isalnum(peek()) || peek() == '_' || peek() == '-' || peek() == '/' || peek() == '.') advance();
   114	    std::string_view text = src_.substr(start, pos_ - start);
   115	    TokenType type = TokenType::IDENTIFIER; 
   116	    
   117	    if (text == ".func") type = TokenType::DIR_FUNC;
   118	    else if (text == ".endfunc") type = TokenType::DIR_ENDFUNC;
   119	    else if (text == ".registers") type = TokenType::DIR_REGISTERS;
   120	    else if (text == ".upvalues") type = TokenType::DIR_UPVALUES;
   121	    else if (text == ".upvalue") type = TokenType::DIR_UPVALUE;
   122	    else if (text == ".const") type = TokenType::DIR_CONST;
   123	    return {type, text, line_};
   124	}
   125	
   126	Token Lexer::scan_string() {
   127	    char quote = advance();
   128	    size_t start = pos_ - 1; 
   129	    while (peek() != quote && !is_at_end()) {
   130	        if (peek() == '\\') advance();
   131	        advance();
   132	    }
   133	    if (!is_at_end()) advance();
   134	    return {TokenType::STRING, src_.substr(start, pos_ - start), line_};
   135	}
   136	
   137	Token Lexer::scan_number() {
   138	    size_t start = pos_;
   139	    if (peek() == '-') advance();
   140	    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
   141	        advance(); advance();
   142	        while (isxdigit(peek())) advance();
   143	        return {TokenType::NUMBER_INT, src_.substr(start, pos_ - start), line_};
   144	    }
   145	    bool is_float = false;
   146	    while (isdigit(peek())) advance();
   147	    if (peek() == '.' && isdigit(peek(1))) {
   148	        is_float = true;
   149	        advance();
   150	        while (isdigit(peek())) advance();
   151	    }
   152	    return {is_float ? TokenType::NUMBER_FLOAT : TokenType::NUMBER_INT, src_.substr(start, pos_ - start), line_};
   153	}
   154	
   155	Token Lexer::scan_identifier() {
   156	    size_t start = pos_;
   157	    while (isalnum(peek()) || peek() == '_' || peek() == '@' || peek() == '/' || peek() == '-' || peek() == '.') advance();
   158	    
   159	    if (peek() == ':') {
   160	        advance(); 
   161	        return {TokenType::LABEL_DEF, src_.substr(start, pos_ - start - 1), line_};
   162	    }
   163	    std::string_view text = src_.substr(start, pos_ - start);
   164	    if (OP_MAP.count(text)) return {TokenType::OPCODE, text, line_};
   165	    return {TokenType::IDENTIFIER, text, line_};
   166	}
   167	
   168	} // namespace meow::masm


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


