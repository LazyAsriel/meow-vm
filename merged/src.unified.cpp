// =============================================================================
//  FILE PATH: src/bytecode/disassemble.cpp
// =============================================================================

     1	#include <meow/bytecode/disassemble.h>
     2	#include <meow/common.h>
     3	#include <meow/core/objects.h>
     4	#include <meow/bytecode/op_codes.h>
     5	#include <meow/value.h>
     6	#include <meow/bytecode/chunk.h>
     7	
     8	namespace meow {
     9	
    10	static constexpr size_t CALL_IC_SIZE = 16;
    11	
    12	constexpr std::string_view get_opcode_name(OpCode op) {
    13	    switch (op) {
    14	#define OP(name) case OpCode::name: return #name;
    15	        OP(LOAD_CONST) OP(LOAD_NULL) OP(LOAD_TRUE) OP(LOAD_FALSE)
    16	        OP(LOAD_INT) OP(LOAD_FLOAT) OP(MOVE)
    17	        OP(INC) OP(DEC)
    18	        OP(ADD) OP(SUB) OP(MUL) OP(DIV) OP(MOD) OP(POW)
    19	        OP(EQ) OP(NEQ) OP(GT) OP(GE) OP(LT) OP(LE)
    20	        OP(NEG) OP(NOT)
    21	        OP(BIT_AND) OP(BIT_OR) OP(BIT_XOR) OP(BIT_NOT)
    22	        OP(LSHIFT) OP(RSHIFT)
    23	        OP(GET_GLOBAL) OP(SET_GLOBAL)
    24	        OP(GET_UPVALUE) OP(SET_UPVALUE)
    25	        OP(CLOSURE) OP(CLOSE_UPVALUES)
    26	        OP(JUMP) OP(JUMP_IF_FALSE) OP(JUMP_IF_TRUE)
    27	        OP(CALL) OP(CALL_VOID) OP(RETURN) OP(HALT)
    28	        OP(NEW_ARRAY) OP(NEW_HASH)
    29	        OP(GET_INDEX) OP(SET_INDEX)
    30	        OP(GET_KEYS) OP(GET_VALUES)
    31	        OP(NEW_CLASS) OP(NEW_INSTANCE)
    32	        OP(GET_PROP) OP(SET_PROP) OP(SET_METHOD)
    33	        OP(INHERIT) OP(GET_SUPER)
    34	        OP(THROW) OP(SETUP_TRY) OP(POP_TRY)
    35	        OP(IMPORT_MODULE) OP(EXPORT) OP(GET_EXPORT) OP(IMPORT_ALL)
    36	        OP(TAIL_CALL)
    37	        
    38	        OP(ADD_B) OP(SUB_B) OP(MUL_B) OP(DIV_B) OP(MOD_B)
    39	        OP(EQ_B) OP(NEQ_B) OP(GT_B) OP(GE_B) OP(LT_B) OP(LE_B)
    40	        OP(JUMP_IF_TRUE_B) OP(JUMP_IF_FALSE_B)
    41	        OP(MOVE_B) OP(LOAD_INT_B)
    42	#undef OP
    43	        default: return "UNKNOWN_OP";
    44	    }
    45	}
    46	
    47	template <typename T>
    48	[[gnu::always_inline]] 
    49	static inline T read_as(const uint8_t* code, size_t& ip) {
    50	    T val;
    51	    std::memcpy(&val, code + ip, sizeof(T));
    52	    ip += sizeof(T);
    53	    return val;
    54	}
    55	
    56	static inline uint8_t read_u8(const uint8_t* code, size_t& ip) { return code[ip++]; }
    57	static inline uint16_t read_u16(const uint8_t* code, size_t& ip) { return read_as<uint16_t>(code, ip); }
    58	static inline uint64_t read_u64(const uint8_t* code, size_t& ip) { return read_as<uint64_t>(code, ip); }
    59	static inline double read_f64(const uint8_t* code, size_t& ip) { return read_as<double>(code, ip); }
    60	
    61	static std::string value_to_string(const Value& value) {
    62	    if (value.is_null()) return "null";
    63	    if (value.is_bool()) return value.as_bool() ? "true" : "false";
    64	    if (value.is_int()) return std::to_string(value.as_int());
    65	    if (value.is_float()) return std::format("{:.6g}", value.as_float());
    66	    if (value.is_string()) return std::format("\"{}\"", value.as_string()->c_str());
    67	    if (value.is_function()) {
    68	        auto name = value.as_function()->get_proto()->get_name();
    69	        return std::format("<fn {}>", name ? name->c_str() : "script");
    70	    }
    71	    return "<obj>";
    72	}
    73	
    74	std::pair<std::string, size_t> disassemble_instruction(const Chunk& chunk, size_t offset) noexcept {
    75	    const uint8_t* code = chunk.get_code();
    76	    size_t code_size = chunk.get_code_size();
    77	    
    78	    if (offset >= code_size) return {"<end>", offset};
    79	
    80	    size_t ip = offset;
    81	    uint8_t raw_op = read_u8(code, ip);
    82	    OpCode op = static_cast<OpCode>(raw_op);
    83	    
    84	    std::string line;
    85	    line.reserve(64); 
    86	
    87	    std::format_to(std::back_inserter(line), "{:<16}", get_opcode_name(op));
    88	
    89	    try {
    90	        switch (op) {
    91	            // --- CONSTANTS ---
    92	            case OpCode::LOAD_CONST: {
    93	                uint16_t dst = read_u16(code, ip);
    94	                uint16_t idx = read_u16(code, ip);
    95	                std::format_to(std::back_inserter(line), "r{}, [{}]", dst, idx);
    96	                if (idx < chunk.get_pool_size()) {
    97	                    std::format_to(std::back_inserter(line), " ({})", value_to_string(chunk.get_constant(idx)));
    98	                }
    99	                break;
   100	            }
   101	            case OpCode::LOAD_INT: {
   102	                uint16_t dst = read_u16(code, ip);
   103	                int64_t val = std::bit_cast<int64_t>(read_u64(code, ip));
   104	                std::format_to(std::back_inserter(line), "r{}, {}", dst, val);
   105	                break;
   106	            }
   107	            case OpCode::LOAD_FLOAT: {
   108	                uint16_t dst = read_u16(code, ip);
   109	                double val = read_f64(code, ip);
   110	                std::format_to(std::back_inserter(line), "r{}, {:.6g}", dst, val);
   111	                break;
   112	            }
   113	            case OpCode::LOAD_NULL:
   114	            case OpCode::LOAD_TRUE:
   115	            case OpCode::LOAD_FALSE: {
   116	                uint16_t dst = read_u16(code, ip);
   117	                std::format_to(std::back_inserter(line), "r{}", dst);
   118	                break;
   119	            }
   120	            case OpCode::LOAD_INT_B: {
   121	                uint8_t dst = read_u8(code, ip);
   122	                int8_t val = static_cast<int8_t>(read_u8(code, ip));
   123	                std::format_to(std::back_inserter(line), "r{}, {}", dst, val);
   124	                break;
   125	            }
   126	
   127	            // --- MOVES ---
   128	            case OpCode::MOVE: {
   129	                uint16_t dst = read_u16(code, ip);
   130	                uint16_t src = read_u16(code, ip);
   131	                std::format_to(std::back_inserter(line), "r{}, r{}", dst, src);
   132	                break;
   133	            }
   134	            case OpCode::MOVE_B: {
   135	                uint8_t dst = read_u8(code, ip);
   136	                uint8_t src = read_u8(code, ip);
   137	                std::format_to(std::back_inserter(line), "r{}, r{}", dst, src);
   138	                break;
   139	            }
   140	
   141	            // --- MATH / BINARY (STANDARD) ---
   142	            case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::DIV:
   143	            case OpCode::MOD: case OpCode::POW:
   144	            case OpCode::EQ: case OpCode::NEQ: case OpCode::GT: case OpCode::GE:
   145	            case OpCode::LT: case OpCode::LE:
   146	            case OpCode::BIT_AND: case OpCode::BIT_OR: case OpCode::BIT_XOR:
   147	            case OpCode::LSHIFT: case OpCode::RSHIFT: {
   148	                uint16_t dst = read_u16(code, ip);
   149	                uint16_t r1 = read_u16(code, ip);
   150	                uint16_t r2 = read_u16(code, ip);
   151	                std::format_to(std::back_inserter(line), "r{}, r{}, r{}", dst, r1, r2);
   152	                break;
   153	            }
   154	
   155	            // --- MATH / BINARY (BYTE OPTIMIZED) ---
   156	            case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: case OpCode::DIV_B:
   157	            case OpCode::MOD_B:
   158	            case OpCode::EQ_B: case OpCode::NEQ_B: case OpCode::GT_B: case OpCode::GE_B:
   159	            case OpCode::LT_B: case OpCode::LE_B: {
   160	                uint8_t dst = read_u8(code, ip);
   161	                uint8_t r1 = read_u8(code, ip);
   162	                uint8_t r2 = read_u8(code, ip);
   163	                std::format_to(std::back_inserter(line), "r{}, r{}, r{}", dst, r1, r2);
   164	                break;
   165	            }
   166	
   167	            // --- UNARY ---
   168	            case OpCode::NEG: case OpCode::NOT: case OpCode::BIT_NOT: {
   169	                uint16_t dst = read_u16(code, ip);
   170	                uint16_t src = read_u16(code, ip);
   171	                std::format_to(std::back_inserter(line), "r{}, r{}", dst, src);
   172	                break;
   173	            }
   174	            case OpCode::INC: case OpCode::DEC: case OpCode::THROW: {
   175	                uint16_t reg = read_u16(code, ip);
   176	                std::format_to(std::back_inserter(line), "r{}", reg);
   177	                break;
   178	            }
   179	
   180	            // --- GLOBALS / UPVALUES ---
   181	            case OpCode::GET_GLOBAL: {
   182	                uint16_t dst = read_u16(code, ip);
   183	                uint16_t idx = read_u16(code, ip);
   184	                std::format_to(std::back_inserter(line), "r{}, g[{}]", dst, idx);
   185	                if (idx < chunk.get_pool_size()) std::format_to(std::back_inserter(line), " ({})", value_to_string(chunk.get_constant(idx)));
   186	                break;
   187	            }
   188	            case OpCode::SET_GLOBAL: {
   189	                uint16_t idx = read_u16(code, ip);
   190	                uint16_t src = read_u16(code, ip);
   191	                std::format_to(std::back_inserter(line), "g[{}], r{}", idx, src);
   192	                if (idx < chunk.get_pool_size()) std::format_to(std::back_inserter(line), " ({})", value_to_string(chunk.get_constant(idx)));
   193	                break;
   194	            }
   195	            case OpCode::GET_UPVALUE: {
   196	                uint16_t dst = read_u16(code, ip);
   197	                uint16_t idx = read_u16(code, ip);
   198	                std::format_to(std::back_inserter(line), "r{}, uv[{}]", dst, idx);
   199	                break;
   200	            }
   201	            case OpCode::SET_UPVALUE: {
   202	                uint16_t idx = read_u16(code, ip);
   203	                uint16_t src = read_u16(code, ip);
   204	                std::format_to(std::back_inserter(line), "uv[{}], r{}", idx, src);
   205	                break;
   206	            }
   207	            case OpCode::CLOSURE: {
   208	                uint16_t dst = read_u16(code, ip);
   209	                uint16_t idx = read_u16(code, ip);
   210	                std::format_to(std::back_inserter(line), "r{}, <proto {}>", dst, idx);
   211	                break;
   212	            }
   213	            case OpCode::CLOSE_UPVALUES: {
   214	                uint16_t slot = read_u16(code, ip);
   215	                std::format_to(std::back_inserter(line), "stack>={}", slot);
   216	                break;
   217	            }
   218	
   219	            // --- JUMPS ---
   220	            case OpCode::JUMP: {
   221	                uint16_t offset = read_u16(code, ip);
   222	                std::format_to(std::back_inserter(line), "-> {:04d}", offset);
   223	                break;
   224	            }
   225	            case OpCode::SETUP_TRY: {
   226	                uint16_t offset = read_u16(code, ip);
   227	                uint16_t err_reg = read_u16(code, ip);
   228	                std::format_to(std::back_inserter(line), "try -> {:04d}, catch=r{}", offset, err_reg);
   229	                break;
   230	            }
   231	            case OpCode::JUMP_IF_FALSE:
   232	            case OpCode::JUMP_IF_TRUE: {
   233	                uint16_t cond = read_u16(code, ip);
   234	                uint16_t offset = read_u16(code, ip);
   235	                std::format_to(std::back_inserter(line), "r{} ? -> {:04d}", cond, offset);
   236	                break;
   237	            }
   238	            case OpCode::JUMP_IF_FALSE_B:
   239	            case OpCode::JUMP_IF_TRUE_B: {
   240	                uint8_t cond = read_u8(code, ip);
   241	                uint16_t offset = read_u16(code, ip);
   242	                std::format_to(std::back_inserter(line), "r{} ? -> {:04d}", cond, offset);
   243	                break;
   244	            }
   245	
   246	            // --- CALLS  ---
   247	            case OpCode::CALL: {
   248	                uint16_t dst = read_u16(code, ip);
   249	                uint16_t fn = read_u16(code, ip);
   250	                uint16_t arg = read_u16(code, ip);
   251	                uint16_t argc = read_u16(code, ip);
   252	                
   253	                std::format_to(std::back_inserter(line), "r{} = r{}(argc={}, args=r{})", dst, fn, argc, arg);
   254	                
   255	                ip += CALL_IC_SIZE; 
   256	                break;
   257	            }
   258	            case OpCode::CALL_VOID: {
   259	                uint16_t fn = read_u16(code, ip);
   260	                uint16_t arg = read_u16(code, ip);
   261	                uint16_t argc = read_u16(code, ip);
   262	
   263	                std::format_to(std::back_inserter(line), "void r{}(argc={}, args=r{})", fn, argc, arg);
   264	                
   265	                ip += CALL_IC_SIZE;
   266	                break;
   267	            }
   268	            case OpCode::TAIL_CALL: {
   269	                uint16_t dst = read_u16(code, ip); (void)dst;
   270	                uint16_t fn = read_u16(code, ip);
   271	                uint16_t arg = read_u16(code, ip);
   272	                uint16_t argc = read_u16(code, ip);
   273	
   274	                std::format_to(std::back_inserter(line), "tail r{}(argc={}, args=r{})", fn, argc, arg);
   275	                
   276	                ip += CALL_IC_SIZE;
   277	                break;
   278	            }
   279	            case OpCode::RETURN: {
   280	                uint16_t reg = read_u16(code, ip);
   281	                if (reg == 0xFFFF) line += "void";
   282	                else std::format_to(std::back_inserter(line), "r{}", reg);
   283	                break;
   284	            }
   285	
   286	            // --- STRUCTURES & OOP ---
   287	            case OpCode::NEW_ARRAY:
   288	            case OpCode::NEW_HASH: {
   289	                uint16_t dst = read_u16(code, ip);
   290	                uint16_t start = read_u16(code, ip);
   291	                uint16_t count = read_u16(code, ip);
   292	                std::format_to(std::back_inserter(line), "r{}, start=r{}, count={}", dst, start, count);
   293	                break;
   294	            }
   295	            case OpCode::GET_INDEX: {
   296	                uint16_t dst = read_u16(code, ip);
   297	                uint16_t src = read_u16(code, ip);
   298	                uint16_t key = read_u16(code, ip);
   299	                std::format_to(std::back_inserter(line), "r{} = r{}[r{}]", dst, src, key);
   300	                break;
   301	            }
   302	            case OpCode::SET_INDEX: {
   303	                uint16_t src = read_u16(code, ip);
   304	                uint16_t key = read_u16(code, ip);
   305	                uint16_t val = read_u16(code, ip);
   306	                std::format_to(std::back_inserter(line), "r{}[r{}] = r{}", src, key, val);
   307	                break;
   308	            }
   309	            case OpCode::NEW_CLASS: {
   310	                uint16_t dst = read_u16(code, ip);
   311	                uint16_t idx = read_u16(code, ip);
   312	                std::format_to(std::back_inserter(line), "r{}, name=[{}]", dst, idx);
   313	                break;
   314	            }
   315	            case OpCode::NEW_INSTANCE: {
   316	                uint16_t dst = read_u16(code, ip);
   317	                uint16_t cls = read_u16(code, ip);
   318	                std::format_to(std::back_inserter(line), "r{}, class=r{}", dst, cls);
   319	                break;
   320	            }
   321	            case OpCode::GET_PROP: {
   322	                uint16_t dst = read_u16(code, ip);
   323	                uint16_t obj = read_u16(code, ip);
   324	                uint16_t idx = read_u16(code, ip);
   325	                std::format_to(std::back_inserter(line), "r{} = r{}.[{}]", dst, obj, idx);
   326	                
   327	                ip += 48; 
   328	                break;
   329	            }
   330	            case OpCode::SET_PROP: {
   331	                uint16_t obj = read_u16(code, ip);
   332	                uint16_t idx = read_u16(code, ip);
   333	                uint16_t val = read_u16(code, ip);
   334	                std::format_to(std::back_inserter(line), "r{}.[{}] = r{}", obj, idx, val);
   335	                
   336	                ip += 48;
   337	                break;
   338	            }
   339	            case OpCode::SET_METHOD: {
   340	                uint16_t cls = read_u16(code, ip);
   341	                uint16_t idx = read_u16(code, ip);
   342	                uint16_t mth = read_u16(code, ip);
   343	                std::format_to(std::back_inserter(line), "r{}.methods[{}] = r{}", cls, idx, mth);
   344	                break;
   345	            }
   346	            case OpCode::INHERIT: {
   347	                uint16_t sub = read_u16(code, ip);
   348	                uint16_t sup = read_u16(code, ip);
   349	                std::format_to(std::back_inserter(line), "sub=r{}, super=r{}", sub, sup);
   350	                break;
   351	            }
   352	            case OpCode::GET_SUPER: {
   353	                uint16_t dst = read_u16(code, ip);
   354	                uint16_t idx = read_u16(code, ip);
   355	                std::format_to(std::back_inserter(line), "r{}, name=[{}]", dst, idx);
   356	                break;
   357	            }
   358	
   359	            // --- MODULES ---
   360	            case OpCode::IMPORT_MODULE: {
   361	                uint16_t dst = read_u16(code, ip);
   362	                uint16_t idx = read_u16(code, ip);
   363	                std::format_to(std::back_inserter(line), "r{}, path=[{}]", dst, idx);
   364	                break;
   365	            }
   366	            case OpCode::EXPORT: {
   367	                uint16_t idx = read_u16(code, ip);
   368	                uint16_t src = read_u16(code, ip);
   369	                std::format_to(std::back_inserter(line), "exports[{}] = r{}", idx, src);
   370	                break;
   371	            }
   372	            case OpCode::GET_EXPORT: {
   373	                uint16_t dst = read_u16(code, ip);
   374	                uint16_t mod = read_u16(code, ip);
   375	                uint16_t idx = read_u16(code, ip);
   376	                std::format_to(std::back_inserter(line), "r{} = r{}::[{}]", dst, mod, idx);
   377	                break;
   378	            }
   379	            case OpCode::IMPORT_ALL: {
   380	                uint16_t mod = read_u16(code, ip);
   381	                std::format_to(std::back_inserter(line), "module=r{}", mod);
   382	                break;
   383	            }
   384	
   385	            // --- OTHERS ---
   386	            case OpCode::GET_KEYS:
   387	            case OpCode::GET_VALUES: {
   388	                uint16_t dst = read_u16(code, ip);
   389	                uint16_t src = read_u16(code, ip);
   390	                std::format_to(std::back_inserter(line), "r{}, from=r{}", dst, src);
   391	                break;
   392	            }
   393	            case OpCode::HALT:
   394	            case OpCode::POP_TRY:
   395	                break; // No operands
   396	
   397	            default:
   398	                line += "<unknown_operands>";
   399	                break;
   400	        }
   401	    } catch (...) {
   402	        line += " <DECODE_ERROR>";
   403	    }
   404	
   405	    return {line, ip};
   406	}
   407	
   408	std::string disassemble_chunk(const Chunk& chunk, const char* name) noexcept {
   409	    std::string out;
   410	    out.reserve(chunk.get_code_size() * 16); 
   411	
   412	    std::format_to(std::back_inserter(out), "== {} ==\n", name ? name : "Chunk");
   413	    
   414	    size_t ip = 0;
   415	    while (ip < chunk.get_code_size()) {
   416	        auto [str, next_ip] = disassemble_instruction(chunk, ip);
   417	        std::format_to(std::back_inserter(out), "{:04d}: {}\n", ip, str);
   418	        ip = next_ip;
   419	    }
   420	    return out;
   421	}
   422	
   423	std::string disassemble_around(const Chunk& chunk, size_t target_ip, int context_lines) noexcept {
   424	    std::vector<size_t> lines;
   425	    lines.reserve(chunk.get_code_size() / 2); 
   426	
   427	    size_t scan_ip = 0;
   428	    while (scan_ip < chunk.get_code_size()) {
   429	        lines.push_back(scan_ip);
   430	        
   431	        auto [_, next] = disassemble_instruction(chunk, scan_ip);
   432	        if (next <= scan_ip) scan_ip++; 
   433	        else scan_ip = next;
   434	    }
   435	
   436	    std::string out;
   437	    out.reserve(1024);
   438	
   439	    auto it = std::upper_bound(lines.begin(), lines.end(), target_ip);
   440	    
   441	    size_t found_idx = 0;
   442	    bool is_aligned = false;
   443	
   444	    if (it == lines.begin()) {
   445	        std::format_to(std::back_inserter(out), "CRITICAL ERROR: Target IP {} is before start of chunk!\n", target_ip);
   446	        found_idx = 0;
   447	    } else {
   448	        --it;
   449	        size_t start_ip = *it;
   450	        found_idx = std::distance(lines.begin(), it);
   451	        
   452	        if (start_ip == target_ip) {
   453	            is_aligned = true;
   454	        } else {
   455	            std::format_to(std::back_inserter(out), 
   456	                "WARNING: IP Misalignment detected!\n"
   457	                "Runtime IP: {}\n"
   458	                "Scanner thinks instruction starts at: {}\n"
   459	                "Diff: {} bytes (Runtime jumped into middle of instruction?)\n"
   460	                "------------------------------------------------\n",
   461	                target_ip, start_ip, target_ip - start_ip);
   462	        }
   463	    }
   464	
   465	    size_t start_idx = (found_idx > (size_t)context_lines) ? found_idx - context_lines : 0;
   466	    size_t end_idx = std::min(lines.size(), found_idx + context_lines + 1);
   467	
   468	    for (size_t i = start_idx; i < end_idx; ++i) {
   469	        size_t ip = lines[i];
   470	        auto [str, next_ip] = disassemble_instruction(chunk, ip);
   471	        
   472	        if (ip == lines[found_idx]) {
   473	            if (is_aligned) {
   474	                std::format_to(std::back_inserter(out), " -> {:04d}: {}   <--- HERE (Exact)\n", ip, str);
   475	            } else {
   476	                std::format_to(std::back_inserter(out), " -> {:04d}: {}   <--- Scanner sees this\n", ip, str);
   477	                
   478	                long long diff = static_cast<long long>(target_ip) - static_cast<long long>(ip);
   479	                std::format_to(std::back_inserter(out), "    ....: (Runtime is at offset {:+} inside here)\n", diff);
   480	            }
   481	        } else {
   482	            std::format_to(std::back_inserter(out), "    {:04d}: {}\n", ip, str);
   483	        }
   484	    }
   485	
   486	    return out;
   487	}
   488	
   489	} // namespace meow


// =============================================================================
//  FILE PATH: src/bytecode/loader.cpp
// =============================================================================

     1	#include "bytecode/loader.h"
     2	#include <meow/memory/memory_manager.h>
     3	#include <meow/memory/gc_disable_guard.h>
     4	#include <meow/core/string.h>
     5	#include <meow/core/function.h>
     6	#include <meow/core/module.h>
     7	#include <meow/value.h>
     8	#include <meow/bytecode/chunk.h>
     9	#include <meow/bytecode/op_codes.h>
    10	
    11	namespace meow {
    12	
    13	constexpr uint32_t MAGIC_NUMBER = 0x4D454F57; // "MEOW"
    14	constexpr uint32_t FORMAT_VERSION = 1;
    15	
    16	enum class ConstantTag : uint8_t {
    17	    NULL_T,
    18	    INT_T,
    19	    FLOAT_T,
    20	    STRING_T,
    21	    PROTO_REF_T
    22	};
    23	
    24	Loader::Loader(MemoryManager* heap, const std::vector<uint8_t>& data)
    25	    : heap_(heap), data_(data), cursor_(0) {}
    26	
    27	void Loader::check_can_read(size_t bytes) {
    28	    if (cursor_ + bytes > data_.size()) {
    29	        throw LoaderError("Unexpected end of file. File is truncated or corrupt.");
    30	    }
    31	}
    32	
    33	uint8_t Loader::read_u8() {
    34	    check_can_read(1);
    35	    return data_[cursor_++];
    36	}
    37	
    38	uint16_t Loader::read_u16() {
    39	    check_can_read(2);
    40	    // Little-endian load
    41	    uint16_t val = static_cast<uint16_t>(data_[cursor_]) |
    42	                   (static_cast<uint16_t>(data_[cursor_ + 1]) << 8);
    43	    cursor_ += 2;
    44	    return val;
    45	}
    46	
    47	uint32_t Loader::read_u32() {
    48	    check_can_read(4);
    49	    uint32_t val = static_cast<uint32_t>(data_[cursor_]) |
    50	                   (static_cast<uint32_t>(data_[cursor_ + 1]) << 8) |
    51	                   (static_cast<uint32_t>(data_[cursor_ + 2]) << 16) |
    52	                   (static_cast<uint32_t>(data_[cursor_ + 3]) << 24);
    53	    cursor_ += 4;
    54	    return val;
    55	}
    56	
    57	uint64_t Loader::read_u64() {
    58	    check_can_read(8);
    59	    uint64_t val;
    60	    std::memcpy(&val, &data_[cursor_], 8); 
    61	    cursor_ += 8;
    62	    return val; // Giả định little-endian machine (x86/arm64)
    63	}
    64	
    65	double Loader::read_f64() {
    66	    return std::bit_cast<double>(read_u64());
    67	}
    68	
    69	string_t Loader::read_string() {
    70	    uint32_t length = read_u32();
    71	    check_can_read(length);
    72	    std::string str(reinterpret_cast<const char*>(data_.data() + cursor_), length);
    73	    cursor_ += length;
    74	    return heap_->new_string(str);
    75	}
    76	
    77	Value Loader::read_constant(size_t current_proto_idx, size_t current_const_idx) {
    78	    ConstantTag tag = static_cast<ConstantTag>(read_u8());
    79	    switch (tag) {
    80	        case ConstantTag::NULL_T:   return Value(null_t{});
    81	        case ConstantTag::INT_T:    return Value(static_cast<int64_t>(read_u64()));
    82	        case ConstantTag::FLOAT_T:  return Value(read_f64());
    83	        case ConstantTag::STRING_T: return Value(read_string());
    84	        
    85	        case ConstantTag::PROTO_REF_T: {
    86	            uint32_t target_proto_index = read_u32();
    87	            patches_.push_back({current_proto_idx, current_const_idx, target_proto_index});
    88	            return Value(null_t{}); 
    89	        }
    90	        default:
    91	            throw LoaderError("Unknown constant tag in binary file.");
    92	    }
    93	}
    94	
    95	proto_t Loader::read_prototype(size_t current_proto_idx) {
    96	    uint32_t num_registers = read_u32();
    97	    uint32_t num_upvalues = read_u32();
    98	    uint32_t name_idx_in_pool = read_u32();
    99	
   100	    uint32_t constant_pool_size = read_u32();
   101	    std::vector<Value> constants;
   102	    constants.reserve(constant_pool_size);
   103	    
   104	    for (uint32_t i = 0; i < constant_pool_size; ++i) {
   105	        constants.push_back(read_constant(current_proto_idx, i));
   106	    }
   107	    
   108	    string_t name = nullptr;
   109	    if (name_idx_in_pool < constants.size() && constants[name_idx_in_pool].is_string()) {
   110	        name = constants[name_idx_in_pool].as_string();
   111	    }
   112	
   113	    // Upvalues
   114	    uint32_t upvalue_desc_count = read_u32();
   115	    if (upvalue_desc_count != num_upvalues) {
   116	         throw LoaderError("Upvalue count mismatch.");
   117	    }
   118	    std::vector<UpvalueDesc> upvalue_descs;
   119	    upvalue_descs.reserve(upvalue_desc_count);
   120	    for (uint32_t i = 0; i < upvalue_desc_count; ++i) {
   121	        bool is_local = (read_u8() == 1);
   122	        uint32_t index = read_u32();
   123	        upvalue_descs.emplace_back(is_local, index);
   124	    }
   125	
   126	    // Bytecode
   127	    uint32_t bytecode_size = read_u32();
   128	    check_can_read(bytecode_size);
   129	    std::vector<uint8_t> bytecode(data_.data() + cursor_, data_.data() + cursor_ + bytecode_size);
   130	    cursor_ += bytecode_size;
   131	    
   132	    Chunk chunk(std::move(bytecode), std::move(constants));
   133	    return heap_->new_proto(num_registers, num_upvalues, name, std::move(chunk), std::move(upvalue_descs));
   134	}
   135	
   136	void Loader::check_magic() {
   137	    if (read_u32() != MAGIC_NUMBER) {
   138	        throw LoaderError("Not a valid Meow bytecode file (magic number mismatch).");
   139	    }
   140	    uint32_t version = read_u32();
   141	    if (version != FORMAT_VERSION) {
   142	        throw LoaderError(std::format("Bytecode version mismatch. File is v{}, VM supports v{}.", version, FORMAT_VERSION));
   143	    }
   144	}
   145	
   146	void Loader::link_prototypes() {    
   147	    for (const auto& patch : patches_) {
   148	        if (patch.proto_idx >= loaded_protos_.size() || patch.target_idx >= loaded_protos_.size()) {
   149	            throw LoaderError("Invalid prototype reference indices.");
   150	        }
   151	
   152	        proto_t parent_proto = loaded_protos_[patch.proto_idx];
   153	        proto_t child_proto = loaded_protos_[patch.target_idx];
   154	
   155	        // Hack const_cast để sửa constant pool (được phép lúc load)
   156	        Chunk& chunk = const_cast<Chunk&>(parent_proto->get_chunk()); 
   157	        
   158	        if (patch.const_idx >= chunk.get_pool_size()) {
   159	             throw LoaderError("Internal Error: Patch constant index out of bounds.");
   160	        }
   161	
   162	        chunk.get_constant_ref(patch.const_idx) = Value(child_proto);
   163	    }
   164	}
   165	
   166	proto_t Loader::load_module() {
   167	    GCDisableGuard guard(heap_);
   168	    check_magic();
   169	    
   170	    uint32_t main_proto_index = read_u32();
   171	    uint32_t prototype_count = read_u32();
   172	    
   173	    if (prototype_count == 0) throw LoaderError("No prototypes found.");
   174	    
   175	    loaded_protos_.reserve(prototype_count);
   176	    for (uint32_t i = 0; i < prototype_count; ++i) {
   177	        loaded_protos_.push_back(read_prototype(i));
   178	    }
   179	    
   180	    if (main_proto_index >= loaded_protos_.size()) throw LoaderError("Main proto index invalid.");
   181	    
   182	    link_prototypes();
   183	    
   184	    return loaded_protos_[main_proto_index];
   185	}
   186	
   187	static void patch_chunk_globals_recursive(module_t mod, proto_t proto, std::unordered_set<proto_t>& visited) {
   188	    if (!proto || visited.contains(proto)) return;
   189	    visited.insert(proto);
   190	
   191	    Chunk& chunk = const_cast<Chunk&>(proto->get_chunk());
   192	    const uint8_t* code = chunk.get_code();
   193	    size_t size = chunk.get_code_size();
   194	    size_t ip = 0;
   195	
   196	    for (size_t i = 0; i < chunk.get_pool_size(); ++i) {
   197	        if (chunk.get_constant(i).is_proto()) {
   198	            patch_chunk_globals_recursive(mod, chunk.get_constant(i).as_proto(), visited);
   199	        }
   200	    }
   201	
   202	    while (ip < size) {
   203	        OpCode op = static_cast<OpCode>(code[ip]);
   204	        
   205	        if (op == OpCode::GET_GLOBAL || op == OpCode::SET_GLOBAL) {
   206	            size_t operand_offset = (op == OpCode::GET_GLOBAL) ? (ip + 3) : (ip + 1);
   207	            
   208	            if (operand_offset + 2 <= size) {
   209	                uint16_t name_idx = static_cast<uint16_t>(code[operand_offset]) | 
   210	                                    (static_cast<uint16_t>(code[operand_offset + 1]) << 8);
   211	                
   212	                if (name_idx < chunk.get_pool_size()) {
   213	                    Value name_val = chunk.get_constant(name_idx);
   214	                    if (name_val.is_string()) {
   215	                        uint32_t global_idx = mod->intern_global(name_val.as_string());
   216	                        
   217	                        if (global_idx > 0xFFFF) {
   218	                            throw LoaderError("Module has too many globals (> 65535).");
   219	                        }
   220	
   221	                        chunk.patch_u16(operand_offset, static_cast<uint16_t>(global_idx));
   222	                    }
   223	                }
   224	            }
   225	        }
   226	
   227	        ip += 1; // Op
   228	        switch (op) {
   229	            case OpCode::HALT: case OpCode::POP_TRY: 
   230	                break;
   231	
   232	            case OpCode::INC: case OpCode::DEC:
   233	            case OpCode::CLOSE_UPVALUES: case OpCode::IMPORT_ALL: case OpCode::THROW: 
   234	            case OpCode::RETURN: 
   235	                ip += 2; break;
   236	            
   237	            case OpCode::LOAD_NULL: case OpCode::LOAD_TRUE: case OpCode::LOAD_FALSE:
   238	                ip += 2; break;
   239	            
   240	            case OpCode::LOAD_CONST: case OpCode::MOVE: 
   241	            case OpCode::NEG: case OpCode::NOT: case OpCode::BIT_NOT: 
   242	            case OpCode::GET_UPVALUE: case OpCode::SET_UPVALUE: 
   243	            case OpCode::CLOSURE:
   244	            case OpCode::NEW_CLASS: case OpCode::NEW_INSTANCE: 
   245	            case OpCode::IMPORT_MODULE: case OpCode::EXPORT: 
   246	            case OpCode::GET_KEYS: case OpCode::GET_VALUES:
   247	            case OpCode::GET_SUPER: 
   248	            case OpCode::GET_GLOBAL: case OpCode::SET_GLOBAL:
   249	            case OpCode::INHERIT:
   250	                ip += 4; break;
   251	
   252	            case OpCode::GET_EXPORT: 
   253	            case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::DIV:
   254	            case OpCode::MOD: case OpCode::POW: case OpCode::EQ: case OpCode::NEQ:
   255	            case OpCode::GT: case OpCode::GE: case OpCode::LT: case OpCode::LE:
   256	            case OpCode::BIT_AND: case OpCode::BIT_OR: case OpCode::BIT_XOR:
   257	            case OpCode::LSHIFT: case OpCode::RSHIFT: 
   258	            case OpCode::NEW_ARRAY: case OpCode::NEW_HASH: 
   259	            case OpCode::GET_INDEX: case OpCode::SET_INDEX: 
   260	            case OpCode::GET_PROP: case OpCode::SET_PROP:
   261	            case OpCode::SET_METHOD:
   262	            // case OpCode::INHERIT:
   263	                ip += 6; break;
   264	            
   265	            case OpCode::CALL: 
   266	                ip += 8;
   267	                ip += 16;
   268	                break;
   269	            
   270	            case OpCode::TAIL_CALL:
   271	                ip += 8;
   272	                ip += 16;
   273	                break;
   274	                
   275	            case OpCode::CALL_VOID:
   276	                ip += 6;
   277	                ip += 16;
   278	                break;
   279	
   280	            case OpCode::LOAD_INT: case OpCode::LOAD_FLOAT:
   281	                ip += 10; break;
   282	
   283	            case OpCode::JUMP: ip += 2; break;
   284	            case OpCode::SETUP_TRY: ip += 4; break;
   285	            case OpCode::JUMP_IF_FALSE: case OpCode::JUMP_IF_TRUE: ip += 4; break;
   286	
   287	            case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: 
   288	            case OpCode::DIV_B: case OpCode::MOD_B: case OpCode::LT_B:
   289	            case OpCode::JUMP_IF_TRUE_B: case OpCode::JUMP_IF_FALSE_B:
   290	                ip += 3; break;
   291	            case OpCode::INVOKE:
   292	                ip += 79; 
   293	                break;
   294	            default: break;
   295	        }
   296	        
   297	        if (op == OpCode::GET_PROP || op == OpCode::SET_PROP) {
   298	             ip += 48; 
   299	        }
   300	    }
   301	}
   302	
   303	void Loader::link_module(module_t module) {
   304	    if (!module || !module->is_has_main()) return;
   305	    std::unordered_set<proto_t> visited;
   306	    patch_chunk_globals_recursive(module, module->get_main_proto(), visited);
   307	}
   308	
   309	}


// =============================================================================
//  FILE PATH: src/bytecode/loader.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/common.h>
     5	#include <meow/bytecode/chunk.h>
     6	#include <meow/core/function.h>
     7	#include <meow/core/module.h>
     8	
     9	namespace meow {
    10	
    11	class MemoryManager;
    12	
    13	class LoaderError : public std::runtime_error {
    14	public:
    15	    explicit LoaderError(const std::string& msg) : std::runtime_error(msg) {}
    16	};
    17	
    18	class Loader {
    19	public:
    20	    Loader(MemoryManager* heap, const std::vector<uint8_t>& data);
    21	
    22	    proto_t load_module();
    23	    static void link_module(module_t module);
    24	
    25	private:
    26	    struct Patch {
    27	        size_t proto_idx;
    28	        size_t const_idx;
    29	        uint32_t target_idx;
    30	    };
    31	
    32	    MemoryManager* heap_;
    33	    const std::vector<uint8_t>& data_;
    34	    size_t cursor_ = 0;
    35	
    36	    std::vector<proto_t> loaded_protos_;
    37	    std::vector<Patch> patches_;
    38	
    39	    void check_can_read(size_t bytes);
    40	    uint8_t  read_u8();
    41	    uint16_t read_u16();
    42	    uint32_t read_u32();
    43	    uint64_t read_u64();
    44	    double   read_f64();
    45	    string_t read_string();
    46	    
    47	    value_t read_constant(size_t current_proto_idx, size_t current_const_idx);
    48	    proto_t read_prototype(size_t current_proto_idx);
    49	    
    50	    void check_magic();
    51	    void link_prototypes();
    52	};
    53	
    54	}


// =============================================================================
//  FILE PATH: src/cli/main.cpp
// =============================================================================

     1	#include <iostream>
     2	#include <filesystem>
     3	#include <print>
     4	#include <vector>
     5	#include <cstring>
     6	#include <string>
     7	#include <fstream>
     8	
     9	#include <meow/machine.h>
    10	#include <meow/config.h>
    11	#include <meow/masm/assembler.h>
    12	#include <meow/masm/lexer.h>
    13	
    14	namespace fs = std::filesystem;
    15	using namespace meow;
    16	
    17	void print_usage() {
    18	    std::println(stderr, "Usage: meow-vm [options] <file>");
    19	    std::println(stderr, "Options:");
    20	    std::println(stderr, "  -b, --bytecode    Run pre-compiled bytecode (.meowc) [Default]");
    21	    std::println(stderr, "  -c, --compile     Compile and run source assembly (.meowb/.asm)");
    22	    std::println(stderr, "  -v, --version     Show version info");
    23	    std::println(stderr, "  -h, --help        Show this help message");
    24	}
    25	
    26	enum class Mode {
    27	    Bytecode,
    28	    SourceAsm,
    29	    Unknown
    30	};
    31	
    32	int main(int argc, char* argv[]) {
    33	    if (argc < 2) {
    34	        print_usage();
    35	        return 1;
    36	    }
    37	
    38	    std::vector<std::string> args;
    39	    for(int i=1; i<argc; ++i) args.push_back(argv[i]);
    40	
    41	    if (args[0] == "--version" || args[0] == "-v") {
    42	        std::println("MeowVM v{} (Built with ❤️ by LazyPaws)", MEOW_VERSION_STR);
    43	        return 0;
    44	    }
    45	    if (args[0] == "--help" || args[0] == "-h") {
    46	        print_usage();
    47	        return 0;
    48	    }
    49	
    50	    Mode mode = Mode::Bytecode;
    51	    std::string input_file;
    52	
    53	    if (args[0] == "-c" || args[0] == "--compile") {
    54	        mode = Mode::SourceAsm;
    55	        if (args.size() < 2) {
    56	            std::println(stderr, "Error: Missing input file for compile mode.");
    57	            return 1;
    58	        }
    59	        input_file = args[1];
    60	    } else if (args[0] == "-b" || args[0] == "--bytecode") {
    61	        mode = Mode::Bytecode;
    62	        if (args.size() < 2) {
    63	            std::println(stderr, "Error: Missing input file.");
    64	            return 1;
    65	        }
    66	        input_file = args[1];
    67	    } else {
    68	        input_file = args[0];
    69	        if (input_file.ends_with(".meowb") || input_file.ends_with(".asm")) {
    70	            mode = Mode::SourceAsm;
    71	        }
    72	    }
    73	
    74	    if (!fs::exists(input_file)) {
    75	        std::println(stderr, "Error: File '{}' not found.", input_file);
    76	        return 1;
    77	    }
    78	
    79	    if (mode == Mode::SourceAsm) {
    80	        std::ifstream f(input_file, std::ios::ate);
    81	        std::streamsize size = f.tellg();
    82	        f.seekg(0, std::ios::beg);
    83	        std::string source(size, '\0');
    84	        if (!f.read(&source[0], size)) {
    85	             std::println(stderr, "Read error: {}", input_file);
    86	             return 1;
    87	        }
    88	
    89	        try {
    90	            masm::init_op_map();
    91	            masm::Lexer lexer(source);
    92	            auto tokens = lexer.tokenize(); 
    93	            masm::Assembler assembler(tokens);
    94	            std::vector<uint8_t> bytecode = assembler.assemble();
    95	            fs::path src_path(input_file);
    96	            fs::path bin_path = src_path;
    97	            bin_path.replace_extension(".meowc");
    98	            
    99	            std::ofstream out(bin_path, std::ios::binary);
   100	            out.write(reinterpret_cast<const char*>(bytecode.data()), bytecode.size());
   101	            out.close();
   102	            
   103	            input_file = bin_path.string();
   104	            
   105	        } catch (const std::exception& e) {
   106	            std::println(stderr, "[Assembly Error] {}", e.what());
   107	            return 1;
   108	        }
   109	    }
   110	
   111	    try {
   112	        std::vector<char*> clean_argv;
   113	        clean_argv.push_back(argv[0]);
   114	
   115	        for (int i = 1; i < argc; ++i) {
   116	            std::string arg = argv[i];
   117	            if (arg == "-b" || arg == "--bytecode" || arg == "-c" || arg == "--compile") {
   118	                continue; 
   119	            }
   120	            clean_argv.push_back(argv[i]);
   121	        }
   122	
   123	        fs::path abs_path = fs::absolute(input_file);
   124	        std::string root_dir = abs_path.parent_path().string();
   125	        std::string entry_file = abs_path.filename().string();
   126	        
   127	        Machine vm(root_dir, entry_file, static_cast<int>(clean_argv.size()), clean_argv.data()); 
   128	        vm.interpret();
   129	        
   130	    } catch (const std::exception& e) {
   131	        std::println(stderr, "VM Runtime Error: {}", e.what());
   132	        return 1;
   133	    }
   134	
   135	    return 0;
   136	}


// =============================================================================
//  FILE PATH: src/core/objects.cpp
// =============================================================================

     1	#include <meow/core/objects.h>
     2	#include <meow/memory/gc_visitor.h>
     3	#include <meow/memory/memory_manager.h>
     4	
     5	namespace meow {
     6	
     7	void ObjArray::trace(GCVisitor& visitor) const noexcept {
     8	    for (const auto& element : elements_) {
     9	        visitor.visit_value(element);
    10	    }
    11	}
    12	
    13	void ObjClass::trace(GCVisitor& visitor) const noexcept {
    14	    visitor.visit_object(name_);
    15	    visitor.visit_object(superclass_);
    16	    
    17	    const auto& keys = methods_.keys();
    18	    const auto& vals = methods_.values();
    19	    const size_t size = keys.size();
    20	    for (size_t i = 0; i < size; ++i) {
    21	        visitor.visit_object(keys[i]);
    22	        visitor.visit_value(vals[i]);
    23	    }
    24	}
    25	
    26	void ObjUpvalue::trace(GCVisitor& visitor) const noexcept {
    27	    visitor.visit_value(closed_);
    28	}
    29	
    30	void ObjFunctionProto::trace(GCVisitor& visitor) const noexcept {
    31	    visitor.visit_object(name_);
    32	    visitor.visit_object(module_);
    33	    for (size_t i = 0; i < chunk_.get_pool_size(); ++i) {
    34	        visitor.visit_value(chunk_.get_constant(i));
    35	    }
    36	}
    37	
    38	void ObjClosure::trace(GCVisitor& visitor) const noexcept {
    39	    visitor.visit_object(proto_);
    40	    for (const auto& upvalue : upvalues_) {
    41	        visitor.visit_object(upvalue);
    42	    }
    43	}
    44	
    45	void ObjModule::trace(GCVisitor& visitor) const noexcept {
    46	    visitor.visit_object(file_name_);
    47	    visitor.visit_object(file_path_);
    48	    visitor.visit_object(main_proto_);
    49	
    50	    for (const auto& val : globals_store_) {
    51	        visitor.visit_value(val);
    52	    }
    53	    
    54	    const auto& g_keys = global_names_.keys();
    55	    for (auto key : g_keys) {
    56	        visitor.visit_object(key);
    57	    }
    58	
    59	    const auto& e_keys = exports_.keys();
    60	    const auto& e_vals = exports_.values();
    61	    
    62	    const size_t e_size = e_keys.size();
    63	    for (size_t i = 0; i < e_size; ++i) {
    64	        visitor.visit_object(e_keys[i]);
    65	        visitor.visit_value(e_vals[i]);
    66	    }
    67	}
    68	
    69	}


// =============================================================================
//  FILE PATH: src/core/shape.cpp
// =============================================================================

     1	#include <meow/core/shape.h>
     2	#include <meow/memory/memory_manager.h>
     3	
     4	namespace meow {
     5	
     6	int Shape::get_offset(string_t name) const {
     7	    if (const uint32_t* ptr = property_offsets_.find(name)) {
     8	        return static_cast<int>(*ptr);
     9	    }
    10	    return -1;
    11	}
    12	
    13	Shape* Shape::get_transition(string_t name) const {
    14	    if (Shape* const* ptr = transitions_.find(name)) {
    15	        return *ptr;
    16	    }
    17	    return nullptr;
    18	}
    19	
    20	Shape* Shape::add_transition(string_t name, MemoryManager* heap) {
    21	    heap->disable_gc();
    22	    Shape* new_shape = heap->new_shape();
    23	    
    24	    new_shape->copy_from(this);
    25	    new_shape->add_property(name);
    26	
    27	    transitions_.try_emplace(name, new_shape);
    28	    
    29	    heap->write_barrier(this, Value(reinterpret_cast<object_t>(new_shape))); 
    30	    heap->enable_gc();
    31	    return new_shape;
    32	}
    33	
    34	void Shape::trace(GCVisitor& visitor) const noexcept {
    35	    const auto& prop_keys = property_offsets_.keys();
    36	    for (auto key : prop_keys) {
    37	        visitor.visit_object(key);
    38	    }
    39	
    40	    const auto& trans_keys = transitions_.keys();
    41	    const auto& trans_vals = transitions_.values();
    42	    
    43	    for (size_t i = 0; i < trans_keys.size(); ++i) {
    44	        visitor.visit_object(trans_keys[i]);
    45	        visitor.visit_object(trans_vals[i]);
    46	    }
    47	}
    48	
    49	}


// =============================================================================
//  FILE PATH: src/jit/analysis/bytecode_analysis.cpp
// =============================================================================

     1	#include <vector>
     2	#include <cstddef>
     3	#include <cstdint>
     4	#include "meow/bytecode/op_codes.h"
     5	
     6	namespace meow::jit::analysis {
     7	
     8	    struct LoopInfo {
     9	        size_t start_ip;
    10	        size_t end_ip;
    11	    };
    12	
    13	    // Hàm quét bytecode để tìm loops
    14	    std::vector<LoopInfo> find_loops(const uint8_t* bytecode, size_t len) {
    15	        std::vector<LoopInfo> loops;
    16	        size_t ip = 0;
    17	        
    18	        while (ip < len) {
    19	            OpCode op = static_cast<OpCode>(bytecode[ip]);
    20	            ip++; 
    21	
    22	            // Logic skip operands (tương tự disassembler)
    23	            // ... (Cần implement logic đọc độ dài từng opcode)
    24	            // Tạm thời bỏ qua vì cần bảng OpCodeLength.
    25	            
    26	            // Nếu gặp JUMP ngược -> Phát hiện loop
    27	        }
    28	        return loops;
    29	    }
    30	
    31	} // namespace meow::jit::analysis


// =============================================================================
//  FILE PATH: src/jit/jit_compiler.cpp
// =============================================================================

     1	#include "jit_compiler.h"
     2	#include "jit_config.h"
     3	#include "x64/code_generator.h"
     4	
     5	#include <iostream>
     6	#include <cstring>
     7	
     8	// Platform specific headers for memory management
     9	#if defined(_WIN32)
    10	    #include <windows.h>
    11	#else
    12	    #include <sys/mman.h>
    13	    #include <unistd.h>
    14	#endif
    15	
    16	namespace meow::jit {
    17	
    18	JitCompiler& JitCompiler::instance() {
    19	    static JitCompiler instance;
    20	    return instance;
    21	}
    22	
    23	// Bộ nhớ JIT: Cần quyền Read + Write (lúc ghi code) và Execute (lúc chạy)
    24	// Vì lý do bảo mật (W^X), thường ta không nên để cả Write và Execute cùng lúc.
    25	// Nhưng để đơn giản cho project này, ta sẽ set RWX (Read-Write-Execute).
    26	// (Production grade thì nên: W -> mprotect -> X)
    27	
    28	struct JitMemory {
    29	    uint8_t* ptr = nullptr;
    30	    size_t size = 0;
    31	};
    32	
    33	static JitMemory g_jit_mem;
    34	
    35	void JitCompiler::initialize() {
    36	    if (g_jit_mem.ptr) return; // Đã init
    37	
    38	    size_t capacity = JIT_CACHE_SIZE;
    39	
    40	#if defined(_WIN32)
    41	    void* ptr = VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    42	    if (!ptr) {
    43	        std::cerr << "[JIT] Failed to allocate executable memory (Windows)!" << std::endl;
    44	        return;
    45	    }
    46	    g_jit_mem.ptr = static_cast<uint8_t*>(ptr);
    47	#else
    48	    void* ptr = mmap(nullptr, capacity, 
    49	                     PROT_READ | PROT_WRITE | PROT_EXEC, 
    50	                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    51	    if (ptr == MAP_FAILED) {
    52	        std::cerr << "[JIT] Failed to allocate executable memory (mmap)!" << std::endl;
    53	        return;
    54	    }
    55	    g_jit_mem.ptr = static_cast<uint8_t*>(ptr);
    56	#endif
    57	
    58	    g_jit_mem.size = capacity;
    59	    if (JIT_DEBUG_LOG) {
    60	        std::cout << "[JIT] Initialized " << (capacity / 1024) << "KB executable memory at " 
    61	                  << (void*)g_jit_mem.ptr << std::endl;
    62	    }
    63	}
    64	
    65	void JitCompiler::shutdown() {
    66	    if (!g_jit_mem.ptr) return;
    67	
    68	#if defined(_WIN32)
    69	    VirtualFree(g_jit_mem.ptr, 0, MEM_RELEASE);
    70	#else
    71	    munmap(g_jit_mem.ptr, g_jit_mem.size);
    72	#endif
    73	    g_jit_mem.ptr = nullptr;
    74	    g_jit_mem.size = 0;
    75	}
    76	
    77	JitFunc JitCompiler::compile(const uint8_t* bytecode, size_t length) {
    78	    if (!g_jit_mem.ptr) {
    79	        initialize();
    80	        if (!g_jit_mem.ptr) return nullptr;
    81	    }
    82	
    83	    // TODO: Quản lý bộ nhớ tốt hơn (Bump pointer allocator)
    84	    // Hiện tại: Reset bộ đệm mỗi lần compile (Chỉ chạy được 1 hàm JIT tại 1 thời điểm)
    85	    // Để chạy nhiều hàm, bạn cần một con trỏ `current_offset` toàn cục.
    86	    static size_t current_offset = 0;
    87	
    88	    // Align 16 bytes
    89	    while (current_offset % 16 != 0) current_offset++;
    90	
    91	    if (current_offset + length * 10 > g_jit_mem.size) { // Ước lượng size * 10
    92	        std::cerr << "[JIT] Cache full! Resetting..." << std::endl;
    93	        current_offset = 0; // Reset "ngây thơ"
    94	    }
    95	
    96	    uint8_t* buffer_start = g_jit_mem.ptr + current_offset;
    97	    size_t remaining_size = g_jit_mem.size - current_offset;
    98	
    99	    // Gọi Backend để sinh mã
   100	    x64::CodeGenerator codegen(buffer_start, remaining_size);
   101	    JitFunc fn = codegen.compile(bytecode, length);
   102	
   103	    // Cập nhật offset (CodeGenerator đã ghi bao nhiêu bytes?)
   104	    // Ta cần API lấy size từ CodeGenerator, nhưng hiện tại lấy qua con trỏ hàm trả về thì hơi khó.
   105	    // Tốt nhất là CodeGenerator trả về struct { func_ptr, code_size }.
   106	    // Tạm thời fix cứng: lấy current cursor từ codegen (cần sửa CodeGenerator public method).
   107	    // Vì CodeGenerator::compile trả về void*, ta giả định nó ghi tiếp nối.
   108	    // Hack tạm: truy cập vào asm bên trong (cần friend class hoặc getter).
   109	    
   110	    // Giả sử ta sửa CodeGenerator để trả về size hoặc tự tính:
   111	    // CodeGenerator instance sẽ bị hủy, ta không query được size.
   112	    // ==> Giải pháp: CodeGenerator::compile nên update 1 biến size tham chiếu.
   113	    
   114	    // (Để code chạy được ngay, ta cứ tăng offset một lượng an toàn hoặc để reset mỗi lần test)
   115	    current_offset += (length * 10); // Hacky estimation
   116	
   117	    if (JIT_DEBUG_LOG) {
   118	        std::cout << "[JIT] Compiled bytecode len=" << length << " to addr=" << (void*)fn << std::endl;
   119	    }
   120	
   121	    return fn;
   122	}
   123	
   124	} // namespace meow::jit


// =============================================================================
//  FILE PATH: src/jit/jit_compiler.h
// =============================================================================

     1	/**
     2	 * @file jit_compiler.h
     3	 * @brief Public API - The main entry point for JIT Compilation
     4	 */
     5	
     6	#pragma once
     7	
     8	#include "meow/value.h"
     9	#include <cstddef>
    10	#include <cstdint>
    11	
    12	// Forward declarations
    13	namespace meow { struct VMState; }
    14	
    15	namespace meow::jit {
    16	
    17	    // Signature của hàm sau khi đã được JIT
    18	    using JitFunc = void (*)(meow::VMState*);
    19	
    20	    class JitCompiler {
    21	    public:
    22	        // Singleton: Chỉ cần 1 trình biên dịch trong suốt vòng đời VM
    23	        static JitCompiler& instance();
    24	
    25	        // Chuẩn bị bộ nhớ (mmap, quyền execute...)
    26	        void initialize();
    27	
    28	        // Dọn dẹp bộ nhớ khi tắt VM
    29	        void shutdown();
    30	
    31	        /**
    32	         * @brief Compile bytecode thành mã máy
    33	         * * @param bytecode Pointer đến mảng bytecode gốc
    34	         * @param length Độ dài
    35	         * @return JitFunc Con trỏ hàm mã máy (hoặc nullptr nếu lỗi/từ chối compile)
    36	         */
    37	        JitFunc compile(const uint8_t* bytecode, size_t length);
    38	
    39	    private:
    40	        JitCompiler() = default;
    41	        ~JitCompiler() = default;
    42	
    43	        JitCompiler(const JitCompiler&) = delete;
    44	        JitCompiler& operator=(const JitCompiler&) = delete;
    45	    };
    46	
    47	} // namespace meow::jit


// =============================================================================
//  FILE PATH: src/jit/jit_config.h
// =============================================================================

     1	/**
     2	 * @file jit_config.h
     3	 * @brief Configuration constants for MeowVM JIT Compiler
     4	 */
     5	
     6	#pragma once
     7	
     8	#include <cstddef>
     9	
    10	namespace meow::jit {
    11	
    12	    // --- Tuning Parameters ---
    13	
    14	    // Số lần một hàm/loop được thực thi trước khi kích hoạt JIT (Hot Threshold)
    15	    // Để 0 hoặc 1 nếu muốn JIT luôn chạy (Eager JIT) để test.
    16	    static constexpr size_t JIT_THRESHOLD = 100;
    17	
    18	    // Kích thước bộ đệm chứa mã máy (Executable Memory)
    19	    // 1MB là đủ cho rất nhiều code meow nhỏ.
    20	    static constexpr size_t JIT_CACHE_SIZE = 1024 * 1024; 
    21	
    22	    // --- Optimization Flags ---
    23	
    24	    // Bật tính năng Inline Caching (Tăng tốc truy cập thuộc tính)
    25	    static constexpr bool ENABLE_INLINE_CACHE = true;
    26	
    27	    // Bật tính năng Guarded Arithmetic (Cộng trừ nhanh trên số nguyên)
    28	    static constexpr bool ENABLE_INT_FAST_PATH = true;
    29	
    30	    // --- Debugging ---
    31	
    32	    // In ra mã Assembly (Hex) sau khi compile
    33	    static constexpr bool JIT_DEBUG_LOG = true;
    34	
    35	    // In ra thông tin khi Deoptimization xảy ra (fallback về Interpreter)
    36	    static constexpr bool LOG_DEOPT = true;
    37	
    38	} // namespace meow::jit


// =============================================================================
//  FILE PATH: src/jit/runtime/runtime_stubs.cpp
// =============================================================================

     1	#include "meow/value.h"
     2	#include "meow/bytecode/op_codes.h"
     3	#include <iostream>
     4	#include <cmath>
     5	
     6	// Sử dụng namespace của VM để truy cập Value, OpCode
     7	using namespace meow;
     8	
     9	namespace meow::jit::runtime {
    10	
    11	// Helper: Convert raw bits -> Value, thực hiện phép toán, ghi lại kết quả
    12	extern "C" void binary_op_generic(int op, uint64_t v1_bits, uint64_t v2_bits, uint64_t* dst) {
    13	    // 1. Reconstruct Values từ Raw Bits (Nanboxing)
    14	    Value v1 = Value::from_raw(v1_bits);
    15	    Value v2 = Value::from_raw(v2_bits);
    16	    Value result;
    17	
    18	    OpCode opcode = static_cast<OpCode>(op);
    19	
    20	    // 2. Thực hiện logic (giống Interpreter nhưng viết gọn)
    21	    // Lưu ý: Ở đây ta handle cả số thực (Double) và các case phức tạp khác
    22	    try {
    23	        if (opcode == OpCode::ADD || opcode == OpCode::ADD_B) {
    24	            if (v1.is_int() && v2.is_int()) {
    25	                result = Value(v1.as_int() + v2.as_int());
    26	            } else if (v1.is_float() || v2.is_float()) {
    27	                // Ép kiểu sang float nếu 1 trong 2 là float
    28	                double d1 = v1.is_int() ? (double)v1.as_int() : v1.as_float();
    29	                double d2 = v2.is_int() ? (double)v2.as_int() : v2.as_float();
    30	                result = Value(d1 + d2);
    31	            } else {
    32	                // TODO: Handle String concat hoặc báo lỗi
    33	                // Tạm thời trả về Null nếu lỗi type
    34	                result = Value(); 
    35	            }
    36	        } 
    37	        else if (opcode == OpCode::SUB || opcode == OpCode::SUB_B) {
    38	            if (v1.is_int() && v2.is_int()) {
    39	                result = Value(v1.as_int() - v2.as_int());
    40	            } else {
    41	                double d1 = v1.is_int() ? (double)v1.as_int() : v1.as_float();
    42	                double d2 = v2.is_int() ? (double)v2.as_int() : v2.as_float();
    43	                result = Value(d1 - d2);
    44	            }
    45	        }
    46	        else if (opcode == OpCode::MUL || opcode == OpCode::MUL_B) {
    47	            if (v1.is_int() && v2.is_int()) {
    48	                result = Value(v1.as_int() * v2.as_int());
    49	            } else {
    50	                double d1 = v1.is_int() ? (double)v1.as_int() : v1.as_float();
    51	                double d2 = v2.is_int() ? (double)v2.as_int() : v2.as_float();
    52	                result = Value(d1 * d2);
    53	            }
    54	        }
    55	    } catch (...) {
    56	        // Chống crash nếu có exception
    57	        result = Value();
    58	    }
    59	
    60	    // 3. Ghi kết quả (Raw bits) vào địa chỉ đích
    61	    *dst = result.raw();
    62	}
    63	
    64	extern "C" void compare_generic(int op, uint64_t v1_bits, uint64_t v2_bits, uint64_t* dst) {
    65	    Value v1 = Value::from_raw(v1_bits);
    66	    Value v2 = Value::from_raw(v2_bits);
    67	    bool res = false;
    68	    OpCode opcode = static_cast<OpCode>(op);
    69	
    70	    // Logic so sánh tổng quát
    71	    // Lưu ý: Cần implement operator<, operator== chuẩn cho class Value
    72	    // Ở đây demo logic cơ bản cho số:
    73	
    74	    double d1 = v1.is_int() ? (double)v1.as_int() : (v1.is_float() ? v1.as_float() : 0.0);
    75	    double d2 = v2.is_int() ? (double)v2.as_int() : (v2.is_float() ? v2.as_float() : 0.0);
    76	    
    77	    // Nếu không phải số, so sánh raw bits (đối với pointer/bool/null)
    78	    bool is_numeric = (v1.is_int() || v1.is_float()) && (v2.is_int() || v2.is_float());
    79	
    80	    switch (opcode) {
    81	        case OpCode::EQ: case OpCode::EQ_B:
    82	            res = (v1 == v2); // Value::operator==
    83	            break;
    84	        case OpCode::NEQ: case OpCode::NEQ_B:
    85	            res = (v1 != v2);
    86	            break;
    87	        case OpCode::LT: case OpCode::LT_B:
    88	            if (is_numeric) res = d1 < d2;
    89	            else res = false; // TODO: String comparison
    90	            break;
    91	        case OpCode::LE: case OpCode::LE_B:
    92	            if (is_numeric) res = d1 <= d2;
    93	            else res = false;
    94	            break;
    95	        case OpCode::GT: case OpCode::GT_B:
    96	            if (is_numeric) res = d1 > d2;
    97	            else res = false;
    98	            break;
    99	        case OpCode::GE: case OpCode::GE_B:
   100	            if (is_numeric) res = d1 >= d2;
   101	            else res = false;
   102	            break;
   103	        default: break;
   104	    }
   105	
   106	    // Kết quả trả về là Value(bool)
   107	    Value result_val(res);
   108	    *dst = result_val.raw();
   109	}
   110	
   111	} // namespace meow::jit::runtime


// =============================================================================
//  FILE PATH: src/jit/x64/assembler.cpp
// =============================================================================

     1	#include "x64/assembler.h"
     2	#include <cstring>
     3	#include <stdexcept>
     4	
     5	namespace meow::jit::x64 {
     6	
     7	Assembler::Assembler(uint8_t* buffer, size_t capacity) 
     8	    : buffer_(buffer), capacity_(capacity), size_(0) {}
     9	
    10	void Assembler::emit(uint8_t b) {
    11	    if (size_ >= capacity_) throw std::runtime_error("JIT Buffer Overflow");
    12	    buffer_[size_++] = b;
    13	}
    14	
    15	void Assembler::emit_u32(uint32_t v) {
    16	    if (size_ + 4 > capacity_) throw std::runtime_error("JIT Buffer Overflow");
    17	    std::memcpy(buffer_ + size_, &v, 4);
    18	    size_ += 4;
    19	}
    20	
    21	void Assembler::emit_u64(uint64_t v) {
    22	    if (size_ + 8 > capacity_) throw std::runtime_error("JIT Buffer Overflow");
    23	    std::memcpy(buffer_ + size_, &v, 8);
    24	    size_ += 8;
    25	}
    26	
    27	void Assembler::patch_u32(size_t offset, uint32_t value) {
    28	    std::memcpy(buffer_ + offset, &value, 4);
    29	}
    30	
    31	// REX Prefix: 0100 WRXB
    32	void Assembler::emit_rex(bool w, bool r, bool x, bool b) {
    33	    uint8_t rex = 0x40;
    34	    if (w) rex |= 0x08; // 64-bit operand size
    35	    if (r) rex |= 0x04; // Extension of ModR/M reg field
    36	    if (x) rex |= 0x02; // Extension of SIB index field
    37	    if (b) rex |= 0x01; // Extension of ModR/M r/m field
    38	    
    39	    // Luôn emit nếu có bất kỳ bit nào hoặc operand là register cao (SPL/BPL/SIL/DIL)
    40	    // Nhưng đơn giản hóa: Nếu operand >= R8 thì R/B sẽ true.
    41	    if (rex != 0x40 || w) emit(rex); // MeowVM chủ yếu dùng 64-bit (W=1)
    42	}
    43	
    44	void Assembler::emit_modrm(int mode, int reg, int rm) {
    45	    emit((mode << 6) | ((reg & 7) << 3) | (rm & 7));
    46	}
    47	
    48	// MOV dst, src
    49	void Assembler::mov(Reg dst, Reg src) {
    50	    if (dst == src) return; // NOP
    51	    emit_rex(true, dst >= 8, false, src >= 8);
    52	    emit(0x8B);
    53	    emit_modrm(3, dst, src);
    54	}
    55	
    56	// MOV dst, imm64
    57	void Assembler::mov(Reg dst, int64_t imm) {
    58	    // Optim: Nếu số dương và vừa 32-bit, dùng MOV r32, imm32 (tự zero-extend lên 64)
    59	    if (imm >= 0 && imm <= 0xFFFFFFFF) {
    60	        if (dst >= 8) emit(0x41); // REX.B nếu dst >= R8
    61	        emit(0xB8 | (dst & 7));
    62	        emit_u32((uint32_t)imm);
    63	    } 
    64	    // Optim: Nếu số âm nhưng vừa 32-bit sign-extended (ví dụ -1, -100) -> MOV r64, imm32 (C7 /0)
    65	    else if (imm >= -2147483648LL && imm <= 2147483647LL) {
    66	        emit_rex(true, false, false, dst >= 8);
    67	        emit(0xC7);
    68	        emit_modrm(3, 0, dst);
    69	        emit_u32((uint32_t)imm);
    70	    } 
    71	    // Full 64-bit load
    72	    else {
    73	        emit_rex(true, false, false, dst >= 8);
    74	        emit(0xB8 | (dst & 7));
    75	        emit_u64((uint64_t)imm);
    76	    }
    77	}
    78	
    79	// MOV dst, [base + disp]
    80	void Assembler::mov(Reg dst, Reg base, int32_t disp) {
    81	    emit_rex(true, dst >= 8, false, base >= 8);
    82	    emit(0x8B);
    83	    
    84	    if (disp == 0 && (base & 7) != 5) { // Mode 0: [reg]
    85	        emit_modrm(0, dst, base);
    86	    } else if (disp >= -128 && disp <= 127) { // Mode 1: [reg + disp8]
    87	        emit_modrm(1, dst, base);
    88	        emit((uint8_t)disp);
    89	    } else { // Mode 2: [reg + disp32]
    90	        emit_modrm(2, dst, base);
    91	        emit_u32(disp);
    92	    }
    93	    if ((base & 7) == 4) emit(0x24); // SIB byte cho RSP (0x24 = [RSP])
    94	}
    95	
    96	// MOV [base + disp], src
    97	void Assembler::mov(Reg base, int32_t disp, Reg src) {
    98	    emit_rex(true, src >= 8, false, base >= 8);
    99	    emit(0x89); // Store
   100	    
   101	    if (disp == 0 && (base & 7) != 5) {
   102	        emit_modrm(0, src, base);
   103	    } else if (disp >= -128 && disp <= 127) {
   104	        emit_modrm(1, src, base);
   105	        emit((uint8_t)disp);
   106	    } else {
   107	        emit_modrm(2, src, base);
   108	        emit_u32(disp);
   109	    }
   110	    if ((base & 7) == 4) emit(0x24); // SIB RSP
   111	}
   112	
   113	void Assembler::emit_alu(uint8_t opcode, Reg dst, Reg src) {
   114	    emit_rex(true, src >= 8, false, dst >= 8);
   115	    emit(opcode);
   116	    emit_modrm(3, src, dst);
   117	}
   118	
   119	void Assembler::add(Reg dst, Reg src) { emit_alu(0x01, dst, src); }
   120	void Assembler::sub(Reg dst, Reg src) { emit_alu(0x29, dst, src); }
   121	void Assembler::and_(Reg dst, Reg src) { emit_alu(0x21, dst, src); }
   122	void Assembler::or_(Reg dst, Reg src) { emit_alu(0x09, dst, src); }
   123	void Assembler::xor_(Reg dst, Reg src) { emit_alu(0x31, dst, src); }
   124	void Assembler::cmp(Reg dst, Reg src) { emit_alu(0x39, dst, src); }
   125	void Assembler::test(Reg dst, Reg src) { emit_rex(true, src >= 8, false, dst >= 8); emit(0x85); emit_modrm(3, src, dst); }
   126	
   127	void Assembler::imul(Reg dst, Reg src) {
   128	    emit_rex(true, dst >= 8, false, src >= 8);
   129	    emit(0x0F); emit(0xAF);
   130	    emit_modrm(3, dst, src);
   131	}
   132	
   133	void Assembler::shl(Reg dst, uint8_t imm) {
   134	    emit_rex(true, false, false, dst >= 8);
   135	    if (imm == 1) { emit(0xD1); emit_modrm(3, 4, dst); }
   136	    else { emit(0xC1); emit_modrm(3, 4, dst); emit(imm); }
   137	}
   138	
   139	void Assembler::sar(Reg dst, uint8_t imm) {
   140	    emit_rex(true, false, false, dst >= 8);
   141	    if (imm == 1) { emit(0xD1); emit_modrm(3, 7, dst); }
   142	    else { emit(0xC1); emit_modrm(3, 7, dst); emit(imm); }
   143	}
   144	
   145	void Assembler::jmp(int32_t rel_offset) { emit(0xE9); emit_u32(rel_offset); }
   146	void Assembler::jmp_short(int8_t rel_offset) { emit(0xEB); emit((uint8_t)rel_offset); }
   147	
   148	void Assembler::jcc(Condition cond, int32_t rel_offset) {
   149	    emit(0x0F); emit(0x80 | (cond & 0xF)); emit_u32(rel_offset);
   150	}
   151	
   152	void Assembler::jcc_short(Condition cond, int8_t rel_offset) {
   153	    emit(0x70 | (cond & 0xF)); emit((uint8_t)rel_offset);
   154	}
   155	
   156	void Assembler::call(Reg target) {
   157	    // Indirect call: FF /2
   158	    if (target >= 8) emit(0x41); 
   159	    emit(0xFF);
   160	    emit_modrm(3, 2, target);
   161	}
   162	
   163	void Assembler::ret() { emit(0xC3); }
   164	
   165	void Assembler::push(Reg r) {
   166	    if (r >= 8) emit(0x41);
   167	    emit(0x50 | (r & 7));
   168	}
   169	
   170	void Assembler::pop(Reg r) {
   171	    if (r >= 8) emit(0x41);
   172	    emit(0x58 | (r & 7));
   173	}
   174	
   175	void Assembler::setcc(Condition cond, Reg dst) {
   176	    if (dst >= 4) emit_rex(false, false, false, dst >= 8);
   177	    emit(0x0F); emit(0x90 | (cond & 0xF));
   178	    emit_modrm(3, 0, dst);
   179	}
   180	
   181	void Assembler::movzx_b(Reg dst, Reg src) {
   182	    emit_rex(true, dst >= 8, false, src >= 8);
   183	    emit(0x0F); emit(0xB6);
   184	    emit_modrm(3, dst, src);
   185	}
   186	
   187	void Assembler::align(size_t boundary) {
   188	    while ((size_ % boundary) != 0) emit(0x90);
   189	}
   190	
   191	} // namespace meow::jit::x64


// =============================================================================
//  FILE PATH: src/jit/x64/assembler.h
// =============================================================================

     1	/**
     2	 * @file assembler.h
     3	 * @brief Low-level x64 Machine Code Emitter
     4	 */
     5	
     6	#pragma once
     7	
     8	#include "x64/common.h"
     9	#include <vector>
    10	#include <cstddef>
    11	#include <cstdint>
    12	
    13	namespace meow::jit::x64 {
    14	
    15	    class Assembler {
    16	    public:
    17	        Assembler(uint8_t* buffer, size_t capacity);
    18	
    19	        // --- Buffer Management ---
    20	        size_t cursor() const { return size_; }
    21	        uint8_t* start_ptr() const { return buffer_; }
    22	        void patch_u32(size_t offset, uint32_t value);
    23	        void align(size_t boundary);
    24	
    25	        // --- Data Movement ---
    26	        void mov(Reg dst, Reg src);
    27	        void mov(Reg dst, int64_t imm64);        // MOV r64, imm64
    28	        void mov(Reg dst, Reg base, int32_t disp); // Load: MOV dst, [base + disp]
    29	        void mov(Reg base, int32_t disp, Reg src); // Store: MOV [base + disp], src
    30	        
    31	        // --- Arithmetic (ALU) ---
    32	        void add(Reg dst, Reg src);
    33	        void sub(Reg dst, Reg src);
    34	        void imul(Reg dst, Reg src);
    35	        void and_(Reg dst, Reg src);
    36	        void or_(Reg dst, Reg src);
    37	        void xor_(Reg dst, Reg src);
    38	        void shl(Reg dst, uint8_t imm);
    39	        void sar(Reg dst, uint8_t imm);
    40	
    41	        // --- Control Flow & Comparison ---
    42	        void cmp(Reg r1, Reg r2);
    43	        void test(Reg r1, Reg r2);
    44	        
    45	        void jmp(int32_t rel_offset);      // JMP rel32
    46	        void jmp_short(int8_t rel_offset); // JMP rel8
    47	        
    48	        void jcc(Condition cond, int32_t rel_offset);
    49	        void jcc_short(Condition cond, int8_t rel_offset);
    50	        
    51	        void call(Reg target); // Indirect call: CALL reg
    52	        void ret();
    53	
    54	        // --- Stack ---
    55	        void push(Reg r);
    56	        void pop(Reg r);
    57	
    58	        // --- Helper Instructions ---
    59	        void setcc(Condition cond, Reg dst); // SETcc r8
    60	        void movzx_b(Reg dst, Reg src);      // MOVZX r64, r8
    61	
    62	    private:
    63	        void emit(uint8_t b);
    64	        void emit_u32(uint32_t v);
    65	        void emit_u64(uint64_t v);
    66	        
    67	        // Encoding Helpers
    68	        void emit_rex(bool w, bool r, bool x, bool b);
    69	        void emit_modrm(int mode, int reg, int rm);
    70	        void emit_alu(uint8_t opcode, Reg dst, Reg src);
    71	
    72	        uint8_t* buffer_;
    73	        size_t capacity_;
    74	        size_t size_;
    75	    };
    76	
    77	} // namespace meow::jit::x64


// =============================================================================
//  FILE PATH: src/jit/x64/code_generator.cpp
// =============================================================================

     1	#include "x64/code_generator.h"
     2	#include "jit_config.h"
     3	#include "x64/common.h"
     4	#include "meow/value.h"
     5	#include "meow/bytecode/op_codes.h"
     6	#include <cstring>
     7	#include <iostream>
     8	
     9	// --- Runtime Helper Definitions ---
    10	namespace meow::jit::runtime {
    11	    // Helper cho số học (+, -, *)
    12	    extern "C" void binary_op_generic(int op, uint64_t v1, uint64_t v2, uint64_t* dst);
    13	    // Helper cho so sánh (==, <, >...) - Trả về 1 (True) hoặc 0 (False) vào *dst
    14	    extern "C" void compare_generic(int op, uint64_t v1, uint64_t v2, uint64_t* dst);
    15	}
    16	
    17	namespace meow::jit::x64 {
    18	
    19	// --- Constants derived from MeowVM Layout ---
    20	
    21	using Layout = meow::Value::layout_traits;
    22	using Variant = meow::base_t;
    23	
    24	// Register Mapping
    25	static constexpr Reg REG_VM_REGS_BASE = R14;
    26	static constexpr Reg REG_CONSTS_BASE  = R15;
    27	
    28	#define MEM_REG(idx)   REG_VM_REGS_BASE, (idx) * 8
    29	#define MEM_CONST(idx) REG_CONSTS_BASE,  (idx) * 8
    30	
    31	// --- Nanbox Constants Calculation ---
    32	// Tính toán Tag tại Compile-time dựa trên thứ tự trong meow::variant
    33	
    34	// 1. INT (Index ?)
    35	static constexpr uint64_t INT_INDEX = Variant::index_of<meow::int_t>();
    36	static constexpr uint64_t TAG_INT   = Layout::make_tag(INT_INDEX);
    37	
    38	// 2. BOOL (Index ?)
    39	static constexpr uint64_t BOOL_INDEX = Variant::index_of<meow::bool_t>();
    40	static constexpr uint64_t TAG_BOOL   = Layout::make_tag(BOOL_INDEX);
    41	
    42	// 3. NULL (Index ?)
    43	static constexpr uint64_t NULL_INDEX = Variant::index_of<meow::null_t>();
    44	static constexpr uint64_t TAG_NULL   = Layout::make_tag(NULL_INDEX);
    45	
    46	// Mask & Check Constants
    47	static constexpr uint64_t TAG_SHIFT     = Layout::TAG_SHIFT;
    48	static constexpr uint64_t TAG_CHECK_VAL = TAG_INT >> TAG_SHIFT; 
    49	
    50	// Giá trị thực tế của FALSE trong Nanboxing (Tag Bool | 0)
    51	// Lưu ý: True là (TAG_BOOL | 1)
    52	static constexpr uint64_t VALUE_FALSE   = TAG_BOOL | 0;
    53	
    54	CodeGenerator::CodeGenerator(uint8_t* buffer, size_t capacity) 
    55	    : asm_(buffer, capacity) {}
    56	
    57	// --- Register Allocation ---
    58	
    59	Reg CodeGenerator::map_vm_reg(int vm_reg) const {
    60	    switch (vm_reg) {
    61	        case 0: return RBX;
    62	        case 1: return R12;
    63	        case 2: return R13;
    64	        default: return INVALID_REG;
    65	    }
    66	}
    67	
    68	void CodeGenerator::load_vm_reg(Reg cpu_dst, int vm_src) {
    69	    Reg mapped = map_vm_reg(vm_src);
    70	    if (mapped != INVALID_REG) {
    71	        if (mapped != cpu_dst) asm_.mov(cpu_dst, mapped);
    72	    } else {
    73	        asm_.mov(cpu_dst, MEM_REG(vm_src));
    74	    }
    75	}
    76	
    77	void CodeGenerator::store_vm_reg(int vm_dst, Reg cpu_src) {
    78	    Reg mapped = map_vm_reg(vm_dst);
    79	    if (mapped != INVALID_REG) {
    80	        if (mapped != cpu_src) asm_.mov(mapped, cpu_src);
    81	    } else {
    82	        asm_.mov(MEM_REG(vm_dst), cpu_src);
    83	    }
    84	}
    85	
    86	// --- Frame Management ---
    87	
    88	void CodeGenerator::emit_prologue() {
    89	    asm_.push(RBP); 
    90	    asm_.mov(RBP, RSP);
    91	    
    92	    // Save Callee-saved regs
    93	    // Total pushes: 5. Stack alignment check:
    94	    // Entry (le 8) -> push RBP (chan 16) -> push 5 regs (le 8).
    95	    // => Hiện tại RSP đang LẺ (misaligned).
    96	    asm_.push(RBX); asm_.push(R12); asm_.push(R13); asm_.push(R14); asm_.push(R15);
    97	
    98	    asm_.mov(REG_VM_REGS_BASE, RDI, 32); // R14 = state->registers
    99	    asm_.mov(REG_CONSTS_BASE,  RDI, 40); // R15 = state->constants
   100	
   101	    // Load Cached Registers from RAM
   102	    for (int i = 0; i <= 2; ++i) {
   103	        Reg r = map_vm_reg(i);
   104	        asm_.mov(r, MEM_REG(i));
   105	    }
   106	}
   107	
   108	void CodeGenerator::emit_epilogue() {
   109	    // Write-back Cached Registers to RAM
   110	    for (int i = 0; i <= 2; ++i) {
   111	        Reg r = map_vm_reg(i);
   112	        asm_.mov(MEM_REG(i), r);
   113	    }
   114	
   115	    // Restore Callee-saved
   116	    asm_.pop(R15); asm_.pop(R14); asm_.pop(R13); asm_.pop(R12); asm_.pop(RBX);
   117	    asm_.pop(RBP);
   118	    asm_.ret();
   119	}
   120	
   121	// --- Helpers for Code Gen ---
   122	
   123	// Flush các thanh ghi vật lý về RAM trước khi gọi hàm C++
   124	// Để đảm bảo GC nhìn thấy dữ liệu mới nhất.
   125	void CodeGenerator::flush_cached_regs() {
   126	    for (int i = 0; i <= 2; ++i) {
   127	        Reg r = map_vm_reg(i);
   128	        asm_.mov(MEM_REG(i), r);
   129	    }
   130	}
   131	
   132	// Load lại thanh ghi từ RAM sau khi gọi hàm C++
   133	// Để đảm bảo nếu GC di chuyển object, ta có địa chỉ mới.
   134	void CodeGenerator::reload_cached_regs() {
   135	    for (int i = 0; i <= 2; ++i) {
   136	        Reg r = map_vm_reg(i);
   137	        asm_.mov(r, MEM_REG(i));
   138	    }
   139	}
   140	
   141	// --- Main Compile Loop ---
   142	
   143	JitFunc CodeGenerator::compile(const uint8_t* bytecode, size_t len) {
   144	    bc_to_native_.clear();
   145	    fixups_.clear();
   146	    slow_paths_.clear();
   147	
   148	    emit_prologue();
   149	
   150	    size_t ip = 0;
   151	    while (ip < len) {
   152	        bc_to_native_[ip] = asm_.cursor();
   153	        
   154	        OpCode op = static_cast<OpCode>(bytecode[ip++]);
   155	
   156	        auto read_u8  = [&]() { return bytecode[ip++]; };
   157	        auto read_u16 = [&]() { 
   158	            uint16_t v; std::memcpy(&v, bytecode + ip, 2); ip += 2; return v; 
   159	        };
   160	
   161	        // --- Guarded Arithmetic Implementation ---
   162	        auto emit_binary_op = [&](uint8_t opcode_alu, bool is_byte_op) {
   163	            uint16_t dst = is_byte_op ? read_u8() : read_u16();
   164	            uint16_t r1  = is_byte_op ? read_u8() : read_u16();
   165	            uint16_t r2  = is_byte_op ? read_u8() : read_u16();
   166	
   167	            Reg r1_reg = map_vm_reg(r1);
   168	            if (r1_reg == INVALID_REG) { load_vm_reg(RAX, r1); r1_reg = RAX; }
   169	            
   170	            Reg r2_reg = map_vm_reg(r2);
   171	            if (r2_reg == INVALID_REG) { load_vm_reg(RCX, r2); r2_reg = RCX; }
   172	
   173	            // 1. Tag Check (Chỉ tối ưu cho INT)
   174	            // Check R1 is INT
   175	            asm_.mov(R8, r1_reg);
   176	            asm_.sar(R8, TAG_SHIFT);
   177	            asm_.mov(R9, TAG_CHECK_VAL);
   178	            asm_.cmp(R8, R9);
   179	            
   180	            Condition not_int1 = NE;
   181	            size_t jump_slow1 = asm_.cursor();
   182	            asm_.jcc(not_int1, 0); // Patch sau
   183	
   184	            // Check R2 is INT
   185	            asm_.mov(R8, r2_reg);
   186	            asm_.sar(R8, TAG_SHIFT);
   187	            asm_.cmp(R8, R9);
   188	            
   189	            Condition not_int2 = NE;
   190	            size_t jump_slow2 = asm_.cursor();
   191	            asm_.jcc(not_int2, 0); // Patch sau
   192	
   193	            // 2. Fast Path (Unbox -> ALU -> Rebox)
   194	            asm_.mov(R8, r1_reg);
   195	            asm_.shl(R8, 16); asm_.sar(R8, 16); // Unbox R1
   196	            
   197	            asm_.mov(R9, r2_reg);
   198	            asm_.shl(R9, 16); asm_.sar(R9, 16); // Unbox R2
   199	
   200	            switch(opcode_alu) {
   201	                case 0: asm_.add(R8, R9); break; // ADD
   202	                case 1: asm_.sub(R8, R9); break; // SUB
   203	                case 2: asm_.imul(R8, R9); break; // MUL
   204	            }
   205	
   206	            // Rebox
   207	            asm_.mov(R9, Layout::PAYLOAD_MASK);
   208	            asm_.and_(R8, R9);
   209	            asm_.mov(R9, TAG_INT);
   210	            asm_.or_(R8, R9);
   211	
   212	            store_vm_reg(dst, R8);
   213	
   214	            // Jump Over Slow Path
   215	            size_t jump_over = asm_.cursor();
   216	            asm_.jmp(0); 
   217	
   218	            // Register Slow Path
   219	            SlowPath sp;
   220	            sp.jumps_to_here.push_back(jump_slow1);
   221	            sp.jumps_to_here.push_back(jump_slow2);
   222	            sp.op = static_cast<int>(op);
   223	            sp.dst_reg_idx = dst;
   224	            sp.src1_reg_idx = r1;
   225	            sp.src2_reg_idx = r2;
   226	            sp.patch_jump_over = jump_over;
   227	            slow_paths_.push_back(sp);
   228	        };
   229	
   230	        // --- Comparison Logic (Chỉ Fast Path cho INT) ---
   231	        auto emit_cmp_op = [&](Condition cond_code, bool is_byte_op) {
   232	            uint16_t dst = is_byte_op ? read_u8() : read_u16();
   233	            uint16_t r1  = is_byte_op ? read_u8() : read_u16();
   234	            uint16_t r2  = is_byte_op ? read_u8() : read_u16();
   235	
   236	            Reg r1_reg = map_vm_reg(r1);
   237	            if (r1_reg == INVALID_REG) { load_vm_reg(RAX, r1); r1_reg = RAX; }
   238	            Reg r2_reg = map_vm_reg(r2);
   239	            if (r2_reg == INVALID_REG) { load_vm_reg(RCX, r2); r2_reg = RCX; }
   240	
   241	            // Tag Check (R1 & R2 must be INT)
   242	            asm_.mov(R8, r1_reg); asm_.sar(R8, TAG_SHIFT);
   243	            asm_.mov(R9, TAG_CHECK_VAL);
   244	            asm_.cmp(R8, R9);
   245	            size_t j1 = asm_.cursor(); asm_.jcc(NE, 0);
   246	
   247	            asm_.mov(R8, r2_reg); asm_.sar(R8, TAG_SHIFT);
   248	            asm_.cmp(R8, R9);
   249	            size_t j2 = asm_.cursor(); asm_.jcc(NE, 0);
   250	
   251	            // Fast CMP
   252	            asm_.cmp(r1_reg, r2_reg);
   253	            asm_.setcc(cond_code, RAX); // Set AL based on flags
   254	            asm_.movzx_b(RAX, RAX);     // Zero extend AL to RAX
   255	
   256	            // Convert result (0/1) to Bool Value
   257	            // Result = (RAX == 1) ? (TAG_BOOL | 1) : (TAG_BOOL | 0)
   258	            // Trick: Bool Value = TAG_BOOL | RAX (vì RAX là 0 hoặc 1)
   259	            asm_.mov(R9, TAG_BOOL);
   260	            asm_.or_(RAX, R9);
   261	            store_vm_reg(dst, RAX);
   262	
   263	            size_t j_over = asm_.cursor(); asm_.jmp(0);
   264	
   265	            // Slow Path (Nếu không phải Int)
   266	            SlowPath sp;
   267	            sp.jumps_to_here.push_back(j1);
   268	            sp.jumps_to_here.push_back(j2);
   269	            sp.op = static_cast<int>(op);
   270	            sp.dst_reg_idx = dst;
   271	            sp.src1_reg_idx = r1;
   272	            sp.src2_reg_idx = r2;
   273	            sp.patch_jump_over = j_over;
   274	            slow_paths_.push_back(sp);
   275	        };
   276	
   277	        switch (op) {
   278	            case OpCode::LOAD_CONST: {
   279	                uint16_t dst = read_u16();
   280	                uint16_t idx = read_u16();
   281	                asm_.mov(RAX, MEM_CONST(idx));
   282	                store_vm_reg(dst, RAX);
   283	                break;
   284	            }
   285	            case OpCode::LOAD_INT: {
   286	                uint16_t dst = read_u16();
   287	                int64_t val; std::memcpy(&val, bytecode + ip, 8); ip += 8;
   288	                // Box Int
   289	                asm_.mov(RAX, val);
   290	                asm_.mov(RCX, Layout::PAYLOAD_MASK);
   291	                asm_.and_(RAX, RCX);
   292	                asm_.mov(RCX, TAG_INT);
   293	                asm_.or_(RAX, RCX);
   294	                store_vm_reg(dst, RAX);
   295	                break;
   296	            }
   297	            case OpCode::LOAD_TRUE: {
   298	                 uint16_t dst = read_u16();
   299	                 asm_.mov(RAX, TAG_BOOL | 1);
   300	                 store_vm_reg(dst, RAX);
   301	                 break;
   302	            }
   303	            case OpCode::LOAD_FALSE: {
   304	                 uint16_t dst = read_u16();
   305	                 asm_.mov(RAX, TAG_BOOL); 
   306	                 store_vm_reg(dst, RAX);
   307	                 break;
   308	            }
   309	            case OpCode::MOVE: {
   310	                uint16_t dst = read_u16();
   311	                uint16_t src = read_u16();
   312	                Reg src_reg = map_vm_reg(src);
   313	                if (src_reg != INVALID_REG) {
   314	                    store_vm_reg(dst, src_reg);
   315	                } else {
   316	                    asm_.mov(RAX, MEM_REG(src));
   317	                    store_vm_reg(dst, RAX);
   318	                }
   319	                break;
   320	            }
   321	
   322	            // --- Arithmetic ---
   323	            case OpCode::ADD: case OpCode::ADD_B: emit_binary_op(0, op == OpCode::ADD_B); break;
   324	            case OpCode::SUB: case OpCode::SUB_B: emit_binary_op(1, op == OpCode::SUB_B); break;
   325	            case OpCode::MUL: case OpCode::MUL_B: emit_binary_op(2, op == OpCode::MUL_B); break;
   326	
   327	            // --- Comparisons ---
   328	            case OpCode::EQ:  case OpCode::EQ_B:  emit_cmp_op(E,  op == OpCode::EQ_B); break;
   329	            case OpCode::NEQ: case OpCode::NEQ_B: emit_cmp_op(NE, op == OpCode::NEQ_B); break;
   330	            case OpCode::LT:  case OpCode::LT_B:  emit_cmp_op(L,  op == OpCode::LT_B); break;
   331	            case OpCode::LE:  case OpCode::LE_B:  emit_cmp_op(LE, op == OpCode::LE_B); break;
   332	            case OpCode::GT:  case OpCode::GT_B:  emit_cmp_op(G,  op == OpCode::GT_B); break;
   333	            case OpCode::GE:  case OpCode::GE_B:  emit_cmp_op(GE, op == OpCode::GE_B); break;
   334	
   335	            // --- Control Flow ---
   336	            case OpCode::JUMP: {
   337	                uint16_t off = read_u16();
   338	                size_t target = off;
   339	                if (bc_to_native_.count(target)) {
   340	                    // Backward Jump
   341	                    size_t target_native = bc_to_native_[target];
   342	                    size_t current = asm_.cursor();
   343	                    int32_t diff = (int32_t)(target_native - (current + 5));
   344	                    asm_.jmp(diff);
   345	                } else {
   346	                    // Forward Jump
   347	                    fixups_.push_back({asm_.cursor(), target, false});
   348	                    asm_.jmp(0); 
   349	                }
   350	                break;
   351	            }
   352	            case OpCode::JUMP_IF_FALSE: case OpCode::JUMP_IF_FALSE_B: {
   353	                bool is_b = (op == OpCode::JUMP_IF_FALSE_B);
   354	                uint16_t reg = is_b ? read_u8() : read_u16();
   355	                uint16_t off = read_u16();
   356	
   357	                Reg r = map_vm_reg(reg);
   358	                if (r == INVALID_REG) { load_vm_reg(RAX, reg); r = RAX; }
   359	
   360	                // Check FALSE: (Val == VALUE_FALSE)
   361	                asm_.mov(RCX, VALUE_FALSE);
   362	                asm_.cmp(r, RCX);
   363	                
   364	                // Nếu Equal (là False) -> Jump
   365	                fixups_.push_back({asm_.cursor(), (size_t)off, true});
   366	                asm_.jcc(E, 0); 
   367	
   368	                // Check NULL: (Val == TAG_NULL)
   369	                asm_.mov(RCX, TAG_NULL);
   370	                asm_.cmp(r, RCX);
   371	                
   372	                // Nếu Equal (là Null) -> Jump
   373	                fixups_.push_back({asm_.cursor(), (size_t)off, true});
   374	                asm_.jcc(E, 0); 
   375	
   376	                break;
   377	            }
   378	
   379	            case OpCode::JUMP_IF_TRUE: case OpCode::JUMP_IF_TRUE_B: {
   380	                bool is_b = (op == OpCode::JUMP_IF_TRUE_B);
   381	                uint16_t reg = is_b ? read_u8() : read_u16();
   382	                uint16_t off = read_u16();
   383	
   384	                Reg r = map_vm_reg(reg);
   385	                if (r == INVALID_REG) { load_vm_reg(RAX, reg); r = RAX; }
   386	
   387	                // Logic: Nhảy nếu (Val != FALSE) AND (Val != NULL)
   388	                
   389	                // 1. Kiểm tra FALSE. Nếu bằng False -> KHÔNG nhảy (bỏ qua lệnh nhảy)
   390	                asm_.mov(RCX, VALUE_FALSE);
   391	                asm_.cmp(r, RCX);
   392	                size_t skip_jump1 = asm_.cursor();
   393	                asm_.jcc(E, 0); // Nhảy đến 'next_instruction' (sẽ patch sau)
   394	
   395	                // 2. Kiểm tra NULL. Nếu bằng Null -> KHÔNG nhảy
   396	                asm_.mov(RCX, TAG_NULL);
   397	                asm_.cmp(r, RCX);
   398	                size_t skip_jump2 = asm_.cursor();
   399	                asm_.jcc(E, 0); // Nhảy đến 'next_instruction'
   400	
   401	                // 3. Thực hiện nhảy (Vì không phải False cũng không phải Null)
   402	                // Đây là nhảy không điều kiện về mặt CPU (vì ta đã lọc ở trên)
   403	                fixups_.push_back({asm_.cursor(), (size_t)off, false}); // false = unconditional jump (5 bytes)
   404	                asm_.jmp(0); 
   405	
   406	                // 4. Patch các lệnh kiểm tra ở trên để trỏ xuống đây (bỏ qua lệnh nhảy)
   407	                size_t next_instruction = asm_.cursor();
   408	                
   409	                // Patch skip_jump1
   410	                int32_t dist1 = (int32_t)(next_instruction - (skip_jump1 + 6)); // JCC near là 6 bytes
   411	                asm_.patch_u32(skip_jump1 + 2, dist1);
   412	
   413	                // Patch skip_jump2
   414	                int32_t dist2 = (int32_t)(next_instruction - (skip_jump2 + 6));
   415	                asm_.patch_u32(skip_jump2 + 2, dist2);
   416	
   417	                break;
   418	            }
   419	
   420	            case OpCode::RETURN: case OpCode::HALT:
   421	                emit_epilogue();
   422	                break;
   423	
   424	            default: break;
   425	        }
   426	    }
   427	
   428	    // --- Generate Out-of-Line Slow Paths ---
   429	    for (auto& sp : slow_paths_) {
   430	        size_t slow_start = asm_.cursor();
   431	        sp.label_start = slow_start;
   432	        
   433	        // 1. Patch jumps to here
   434	        for (size_t jump_src : sp.jumps_to_here) {
   435	            int32_t off = (int32_t)(slow_start - (jump_src + 6));
   436	            asm_.patch_u32(jump_src + 2, off);
   437	        }
   438	
   439	        // 2. State Flushing (Cực kỳ quan trọng!)
   440	        flush_cached_regs();
   441	
   442	        // 3. Align Stack (Linux/System V requirement)
   443	        // Hiện tại RSP đang lệch 8 (do Push 5 regs + RBP).
   444	        asm_.mov(RAX, 8);
   445	        asm_.sub(RSP, RAX);
   446	
   447	        // 4. Setup Runtime Call (System V ABI: RDI, RSI, RDX, RCX, ...)
   448	        asm_.mov(RDI, (uint64_t)sp.op);    // Arg1: Opcode
   449	        load_vm_reg(RSI, sp.src1_reg_idx); // Arg2: Value 1
   450	        load_vm_reg(RDX, sp.src2_reg_idx); // Arg3: Value 2
   451	
   452	        // Arg4: Dst pointer (&regs[dst])
   453	        asm_.mov(RCX, REG_VM_REGS_BASE);
   454	        asm_.mov(RAX, sp.dst_reg_idx * 8);
   455	        asm_.add(RCX, RAX); 
   456	
   457	        // 5. Call helper
   458	        if (sp.op >= static_cast<int>(OpCode::EQ)) {
   459	             asm_.mov(RAX, (uint64_t)&runtime::compare_generic);
   460	        } else {
   461	             asm_.mov(RAX, (uint64_t)&runtime::binary_op_generic);
   462	        }
   463	        asm_.call(RAX);
   464	
   465	        // 6. Restore Stack
   466	        asm_.mov(RAX, 8);
   467	        asm_.add(RSP, RAX);
   468	
   469	        // 7. Reload State (vì GC có thể đã chạy)
   470	        reload_cached_regs();
   471	
   472	        // Reload kết quả từ RAM lên Register (nếu dst đang được cache)
   473	        Reg mapped_dst = map_vm_reg(sp.dst_reg_idx);
   474	        if (mapped_dst != INVALID_REG) {
   475	            asm_.mov(mapped_dst, MEM_REG(sp.dst_reg_idx));
   476	        }
   477	
   478	        // 8. Jump back
   479	        size_t jump_back_target = sp.patch_jump_over + 5; 
   480	        int32_t back_off = (int32_t)(jump_back_target - (asm_.cursor() + 5));
   481	        asm_.jmp(back_off);
   482	    }
   483	
   484	    // --- Patch Forward Jumps ---
   485	    for (const auto& fix : fixups_) {
   486	        if (bc_to_native_.count(fix.target_bc)) {
   487	            size_t target_native = bc_to_native_[fix.target_bc];
   488	            size_t jump_len = fix.is_cond ? 6 : 5;
   489	            int32_t rel = (int32_t)(target_native - (fix.jump_op_pos + jump_len));
   490	            asm_.patch_u32(fix.jump_op_pos + (fix.is_cond ? 2 : 1), rel);
   491	        }
   492	    }
   493	
   494	    return reinterpret_cast<JitFunc>(asm_.start_ptr());
   495	}
   496	
   497	} // namespace meow::jit::x64


// =============================================================================
//  FILE PATH: src/jit/x64/code_generator.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "jit_compiler.h"
     4	#include "x64/assembler.h"
     5	#include "x64/common.h"
     6	#include <vector>
     7	#include <unordered_map>
     8	
     9	namespace meow::jit::x64 {
    10	
    11	struct Fixup {
    12	    size_t jump_op_pos; // Vị trí ghi opcode nhảy
    13	    size_t target_bc;   // Bytecode đích đến
    14	    bool is_cond;       // Là nhảy có điều kiện? (JCC) hay không (JMP)
    15	};
    16	
    17	struct SlowPath {
    18	    size_t label_start;           // Địa chỉ bắt đầu sinh mã slow path
    19	    std::vector<size_t> jumps_to_here; // Các chỗ cần patch để nhảy vào đây
    20	    size_t patch_jump_over;       // Chỗ cần nhảy về sau khi xong (Fast path end)
    21	    
    22	    int op;           // Opcode gốc
    23	    int dst_reg_idx;  // VM Register index
    24	    int src1_reg_idx;
    25	    int src2_reg_idx;
    26	};
    27	
    28	class CodeGenerator {
    29	public:
    30	    CodeGenerator(uint8_t* buffer, size_t capacity);
    31	    
    32	    // Compile bytecode -> trả về con trỏ hàm JIT
    33	    JitFunc compile(const uint8_t* bytecode, size_t len);
    34	
    35	private:
    36	    Assembler asm_;
    37	    
    38	    // Map từ Bytecode Offset -> Native Code Offset
    39	    std::unordered_map<size_t, size_t> bc_to_native_;
    40	    
    41	    // Danh sách cần fix nhảy (Forward Jumps)
    42	    std::vector<Fixup> fixups_;
    43	    
    44	    // Danh sách Slow Paths (sinh mã ở cuối buffer)
    45	    std::vector<SlowPath> slow_paths_;
    46	
    47	    // --- Helpers ---
    48	    void emit_prologue();
    49	    void emit_epilogue();
    50	
    51	    // Register Allocator đơn giản (Map VM Reg -> CPU Reg)
    52	    Reg map_vm_reg(int vm_reg) const;
    53	    void load_vm_reg(Reg cpu_dst, int vm_src);
    54	    void store_vm_reg(int vm_dst, Reg cpu_src);
    55	
    56	    // [MỚI] Đồng bộ trạng thái VM State (QUAN TRỌNG)
    57	    void flush_cached_regs();
    58	    void reload_cached_regs();
    59	};
    60	
    61	} // namespace meow::jit::x64


// =============================================================================
//  FILE PATH: src/jit/x64/common.h
// =============================================================================

     1	/**
     2	 * @file common.h
     3	 * @brief Shared definitions for x64 Backend (Registers, Conditions)
     4	 */
     5	
     6	#pragma once
     7	
     8	#include <cstdint>
     9	
    10	namespace meow::jit::x64 {
    11	
    12	    // --- x64 Hardware Registers ---
    13	    enum Reg : uint8_t {
    14	        RAX = 0, RCX = 1, RDX = 2, RBX = 3, 
    15	        RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    16	        R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    17	        R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    18	        INVALID_REG = 0xFF
    19	    };
    20	
    21	    // --- CPU Condition Codes (EFLAGS) ---
    22	    enum Condition : uint8_t {
    23	        O  = 0,  NO = 1,  // Overflow
    24	        B  = 2,  AE = 3,  // Below / Above or Equal (Unsigned)
    25	        E  = 4,  NE = 5,  // Equal / Not Equal
    26	        BE = 6,  A  = 7,  // Below or Equal / Above (Unsigned)
    27	        S  = 8,  NS = 9,  // Sign
    28	        P  = 10, NP = 11, // Parity
    29	        L  = 12, GE = 13, // Less / Greater or Equal (Signed)
    30	        LE = 14, G  = 15  // Less or Equal / Greater (Signed)
    31	    };
    32	
    33	    // --- MeowVM Calling Convention ---
    34	    // Quy định thanh ghi nào giữ con trỏ quan trọng của VM
    35	
    36	    // Thanh ghi chứa con trỏ `VMState*` (được truyền vào từ C++)
    37	    // Theo System V AMD64 ABI (Linux/Mac), tham số đầu tiên là RDI.
    38	    // Theo MS x64 (Windows), tham số đầu tiên là RCX.
    39	    #if defined(_WIN32)
    40	        static constexpr Reg REG_STATE = RCX;
    41	        static constexpr Reg REG_TMP1  = RDX; // Scratch register
    42	    #else
    43	        static constexpr Reg REG_STATE = RDI;
    44	        static constexpr Reg REG_TMP1  = RSI; // Lưu ý: RSI thường dùng cho param 2
    45	    #endif
    46	
    47	    // Các thanh ghi Callee-saved (được phép dùng lâu dài, phải restore khi exit)
    48	    // Ta dùng để map các register ảo của VM (r0, r1...)
    49	    static constexpr Reg VM_LOCALS_BASE = RBX; 
    50	    // Chúng ta sẽ định nghĩa map cụ thể trong CodeGen sau.
    51	
    52	    // Tagging constants (cho Nan-boxing)
    53	    // Giá trị này phải khớp với meow::Value::NANBOX_INT_TAG
    54	    // Giả sử layout traits tag = 2 (như trong compiler cũ)
    55	    static constexpr uint64_t NANBOX_INT_TAG = 0xFFFE000000000000ULL; // Ví dụ, cần check lại value.h thực tế
    56	
    57	} // namespace meow::jit::x64


// =============================================================================
//  FILE PATH: src/memory/generational_gc.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "memory/generational_gc.h"
     3	#include <meow/value.h>
     4	#include "runtime/execution_context.h"
     5	#include <meow/core/meow_object.h>
     6	#include <module/module_manager.h>
     7	#include "meow_heap.h"
     8	
     9	namespace meow {
    10	
    11	using namespace gc_flags;
    12	
    13	static void clear_list(heap* h, ObjectMeta* head) {
    14	    while (head) {
    15	        ObjectMeta* next = head->next_gc;
    16	        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(head));
    17	        std::destroy_at(obj);
    18	        h->deallocate_raw(head, sizeof(ObjectMeta) + head->size);
    19	        head = next;
    20	    }
    21	}
    22	
    23	GenerationalGC::~GenerationalGC() noexcept {
    24	    if (heap_) {
    25	        clear_list(heap_, young_head_);
    26	        clear_list(heap_, old_head_);
    27	        clear_list(heap_, perm_head_);
    28	    }
    29	}
    30	
    31	void GenerationalGC::register_object(const MeowObject* object) {
    32	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    33	    
    34	    meta->next_gc = young_head_;
    35	    young_head_ = meta;
    36	    
    37	    meta->flags = GEN_YOUNG;
    38	    
    39	    young_count_++;
    40	}
    41	
    42	void GenerationalGC::register_permanent(const MeowObject* object) {
    43	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    44	    
    45	    meta->next_gc = perm_head_;
    46	    perm_head_ = meta;
    47	    
    48	    meta->flags = GEN_OLD | PERMANENT | MARKED;
    49	}
    50	
    51	void GenerationalGC::write_barrier(MeowObject* owner, Value value) noexcept {
    52	    auto* owner_meta = heap::get_meta(owner);
    53	    
    54	    if (!(owner_meta->flags & GEN_OLD)) return;
    55	
    56	    if (value.is_object()) {
    57	        MeowObject* target = value.as_object();
    58	        if (target) {
    59	            auto* target_meta = heap::get_meta(target);
    60	            if (!(target_meta->flags & GEN_OLD)) {
    61	                remembered_set_.push_back(owner);
    62	            }
    63	        }
    64	    }
    65	}
    66	
    67	size_t GenerationalGC::collect() noexcept {
    68	    context_->trace(*this);
    69	    module_manager_->trace(*this);
    70	    
    71	    ObjectMeta* perm = perm_head_;
    72	    while (perm) {
    73	        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(perm));
    74	        if (obj->get_type() != ObjectType::STRING) {
    75	            mark_object(obj);
    76	        }
    77	        perm = perm->next_gc;
    78	    }
    79	
    80	    for (auto* obj : remembered_set_) {
    81	        mark_object(obj);
    82	    }
    83	
    84	    if (old_count_ > old_gen_threshold_) {
    85	        sweep_full();
    86	        old_gen_threshold_ = std::max((size_t)100, old_count_ * 2);
    87	    } else {
    88	        sweep_young();
    89	    }
    90	
    91	    remembered_set_.clear();
    92	    return young_count_ + old_count_;
    93	}
    94	
    95	void GenerationalGC::destroy_object(ObjectMeta* meta) {
    96	    MeowObject* obj = static_cast<MeowObject*>(heap::get_data(meta));
    97	    std::destroy_at(obj);
    98	    heap_->deallocate_raw(meta, sizeof(ObjectMeta) + meta->size);
    99	}
   100	
   101	void GenerationalGC::sweep_young() {
   102	    ObjectMeta** curr = &young_head_;
   103	    size_t survived = 0;
   104	
   105	    while (*curr) {
   106	        ObjectMeta* meta = *curr;
   107	        
   108	        if (meta->flags & MARKED) {
   109	            *curr = meta->next_gc; 
   110	            meta->next_gc = old_head_;
   111	            old_head_ = meta;
   112	            meta->flags = GEN_OLD; 
   113	            
   114	            old_count_++;
   115	            young_count_--;
   116	        } else {
   117	            ObjectMeta* dead = meta;
   118	            *curr = dead->next_gc;
   119	            
   120	            destroy_object(dead);
   121	            young_count_--;
   122	        }
   123	    }
   124	}
   125	
   126	void GenerationalGC::sweep_full() {
   127	    ObjectMeta** curr_old = &old_head_;
   128	    size_t old_survived = 0;
   129	    while (*curr_old) {
   130	        ObjectMeta* meta = *curr_old;
   131	        if (meta->flags & MARKED) {
   132	            meta->flags &= ~MARKED;
   133	            curr_old = &meta->next_gc;
   134	            old_survived++;
   135	        } else {
   136	            ObjectMeta* dead = meta;
   137	            *curr_old = dead->next_gc;
   138	            destroy_object(dead);
   139	        }
   140	    }
   141	    old_count_ = old_survived;
   142	
   143	    sweep_young(); 
   144	}
   145	
   146	void GenerationalGC::visit_value(param_t value) noexcept {
   147	    if (value.is_object()) mark_object(value.as_object());
   148	}
   149	
   150	void GenerationalGC::visit_object(const MeowObject* object) noexcept {
   151	    mark_object(const_cast<MeowObject*>(object));
   152	}
   153	
   154	void GenerationalGC::mark_object(MeowObject* object) {
   155	    if (object == nullptr) return;
   156	    
   157	    auto* meta = heap::get_meta(object);
   158	    
   159	    if (meta->flags & MARKED) return;
   160	    
   161	    meta->flags |= MARKED;
   162	    
   163	    object->trace(*this);
   164	}
   165	
   166	}


// =============================================================================
//  FILE PATH: src/memory/generational_gc.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/common.h>
     5	#include <meow/memory/garbage_collector.h>
     6	#include <meow/memory/gc_visitor.h>
     7	#include <vector> 
     8	#include "meow_heap.h"
     9	
    10	namespace meow {
    11	struct ExecutionContext;
    12	class ModuleManager;
    13	
    14	class GenerationalGC : public GarbageCollector, public GCVisitor {
    15	public:
    16	    explicit GenerationalGC(ExecutionContext* context) noexcept : context_(context) {}
    17	    ~GenerationalGC() noexcept override;
    18	
    19	    void register_object(const MeowObject* object) override;
    20	    void register_permanent(const MeowObject* object) override;
    21	    size_t collect() noexcept override;
    22	
    23	    void write_barrier(MeowObject* owner, Value value) noexcept override;
    24	
    25	    void visit_value(param_t value) noexcept override;
    26	    void visit_object(const MeowObject* object) noexcept override;
    27	
    28	    void set_module_manager(ModuleManager* mm) { module_manager_ = mm; }
    29	
    30	private:
    31	    ExecutionContext* context_ = nullptr;
    32	    ModuleManager* module_manager_ = nullptr;
    33	
    34	    ObjectMeta* young_head_ = nullptr;
    35	    ObjectMeta* old_head_   = nullptr;
    36	    ObjectMeta* perm_head_  = nullptr;
    37	    
    38	    std::vector<MeowObject*> remembered_set_;
    39	
    40	    size_t young_count_ = 0;
    41	    size_t old_count_ = 0;
    42	    size_t old_gen_threshold_ = 100;
    43	
    44	    void mark_object(MeowObject* object);
    45	    
    46	    void sweep_young(); 
    47	    void sweep_full();
    48	    
    49	    void destroy_object(ObjectMeta* meta);
    50	};
    51	}


// =============================================================================
//  FILE PATH: src/memory/mark_sweep_gc.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "memory/mark_sweep_gc.h"
     3	#include <meow/value.h>
     4	#include <module/module_manager.h>
     5	#include "runtime/execution_context.h"
     6	#include "meow_heap.h"
     7	
     8	namespace meow {
     9	    
    10	using namespace gc_flags;
    11	
    12	MarkSweepGC::~MarkSweepGC() noexcept {
    13	    while (head_) {
    14	        ObjectMeta* next = head_->next_gc;
    15	        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(head_));
    16	        std::destroy_at(obj);
    17	        if (heap_) heap_->deallocate_raw(head_, sizeof(ObjectMeta) + head_->size);
    18	        head_ = next;
    19	    }
    20	}
    21	
    22	void MarkSweepGC::register_object(const MeowObject* object) {
    23	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    24	    meta->next_gc = head_;
    25	    head_ = meta;
    26	    meta->flags = 0;
    27	    object_count_++;
    28	}
    29	
    30	void MarkSweepGC::register_permanent(const MeowObject* object) {
    31	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    32	    meta->flags = MARKED | PERMANENT; 
    33	}
    34	
    35	size_t MarkSweepGC::collect() noexcept {
    36	    context_->trace(*this);
    37	    module_manager_->trace(*this);
    38	    ObjectMeta** curr = &head_;
    39	    size_t survived = 0;
    40	
    41	    while (*curr) {
    42	        ObjectMeta* meta = *curr;
    43	        
    44	        if (meta->flags & PERMANENT) {
    45	            curr = &meta->next_gc;
    46	        }
    47	        else if (meta->flags & MARKED) {
    48	            meta->flags &= ~MARKED;
    49	            curr = &meta->next_gc;
    50	            survived++;
    51	        } else {
    52	            ObjectMeta* dead = meta;
    53	            *curr = dead->next_gc;
    54	            
    55	            MeowObject* obj = static_cast<MeowObject*>(heap::get_data(dead));
    56	            std::destroy_at(obj);
    57	            heap_->deallocate_raw(dead, sizeof(ObjectMeta) + dead->size);
    58	            
    59	            object_count_--;
    60	        }
    61	    }
    62	
    63	    return survived;
    64	}
    65	
    66	void MarkSweepGC::visit_value(param_t value) noexcept {
    67	    if (value.is_object()) mark(value.as_object());
    68	}
    69	
    70	void MarkSweepGC::visit_object(const MeowObject* object) noexcept {
    71	    mark(const_cast<MeowObject*>(object));
    72	}
    73	
    74	void MarkSweepGC::mark(MeowObject* object) {
    75	    if (object == nullptr) return;
    76	    auto* meta = heap::get_meta(object);
    77	    
    78	    if (meta->flags & MARKED) return;
    79	    
    80	    meta->flags |= MARKED;
    81	    object->trace(*this);
    82	}
    83	
    84	}


// =============================================================================
//  FILE PATH: src/memory/mark_sweep_gc.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/common.h>
     5	#include <meow/memory/garbage_collector.h>
     6	#include <meow/memory/gc_visitor.h>
     7	#include "meow_heap.h"
     8	
     9	namespace meow {
    10	struct ExecutionContext;
    11	class ModuleManager;
    12	
    13	class MarkSweepGC : public GarbageCollector, public GCVisitor {
    14	public:
    15	    explicit MarkSweepGC(ExecutionContext* context) noexcept : context_(context) {}
    16	    ~MarkSweepGC() noexcept override;
    17	
    18	    void register_object(const MeowObject* object) override;
    19	    void register_permanent(const MeowObject* object) override;
    20	    size_t collect() noexcept override;
    21	
    22	    void visit_value(param_t value) noexcept override;
    23	    void visit_object(const MeowObject* object) noexcept override;
    24	
    25	    void set_module_manager(ModuleManager* mm) { module_manager_ = mm; }
    26	private:
    27	    ExecutionContext* context_ = nullptr;
    28	    ModuleManager* module_manager_ = nullptr;
    29	    
    30	    ObjectMeta* head_ = nullptr;
    31	    size_t object_count_ = 0;
    32	
    33	    void mark(MeowObject* object);
    34	};
    35	}


// =============================================================================
//  FILE PATH: src/memory/memory_manager.cpp
// =============================================================================

     1	#include <meow/memory/memory_manager.h>
     2	#include <meow/core/objects.h>
     3	
     4	namespace meow {
     5	
     6	thread_local MemoryManager* MemoryManager::current_ = nullptr;
     7	
     8	MemoryManager::MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept 
     9	    : arena_(64 * 1024), 
    10	      heap_(arena_),
    11	      gc_(std::move(gc)), 
    12	      gc_threshold_(1e7), 
    13	      object_allocated_(0) 
    14	{ 
    15	    if (gc_) {
    16	        gc_->set_heap(&heap_);
    17	    }
    18	}
    19	MemoryManager::~MemoryManager() noexcept {}
    20	
    21	string_t MemoryManager::new_string(std::string_view str_view) {
    22	    if (auto it = string_pool_.find(str_view); it != string_pool_.end()) {
    23	        return *it;
    24	    }
    25	    
    26	    size_t length = str_view.size();
    27	    size_t hash = std::hash<std::string_view>{}(str_view);
    28	    
    29	    string_t new_obj = heap_.create_varsize<ObjString>(length, str_view.data(), length, hash);
    30	    
    31	    gc_->register_permanent(new_obj);
    32	    
    33	    object_allocated_++;
    34	    string_pool_.insert(new_obj);
    35	    return new_obj;
    36	}
    37	
    38	string_t MemoryManager::new_string(const char* chars, size_t length) {
    39	    return new_string(std::string(chars, length));
    40	}
    41	
    42	array_t MemoryManager::new_array(const std::vector<Value>& elements) {
    43	    // meow::allocator<Value> alloc(arena_);
    44	    
    45	    // return new_object<ObjArray>(elements, alloc);
    46	    return new_object<ObjArray>(elements);
    47	}
    48	
    49	hash_table_t MemoryManager::new_hash(uint32_t capacity) {
    50	    auto alloc = heap_.get_allocator<Entry>();
    51	    return heap_.create<ObjHashTable>(alloc, capacity);
    52	}
    53	
    54	upvalue_t MemoryManager::new_upvalue(size_t index) {
    55	    return new_object<ObjUpvalue>(index);
    56	}
    57	
    58	proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk) {
    59	    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk));
    60	}
    61	
    62	proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<UpvalueDesc>&& descs) {
    63	    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk), std::move(descs));
    64	}
    65	
    66	function_t MemoryManager::new_function(proto_t proto) {
    67	    return new_object<ObjClosure>(proto);
    68	}
    69	
    70	module_t MemoryManager::new_module(string_t file_name, string_t file_path, proto_t main_proto) {
    71	    return new_object<ObjModule>(file_name, file_path, main_proto);
    72	}
    73	
    74	class_t MemoryManager::new_class(string_t name) {
    75	    return new_object<ObjClass>(name);
    76	}
    77	
    78	instance_t MemoryManager::new_instance(class_t klass, Shape* shape) {
    79	    return new_object<ObjInstance>(klass, shape);
    80	}
    81	
    82	bound_method_t MemoryManager::new_bound_method(Value instance, Value function) {
    83	    return new_object<ObjBoundMethod>(instance, function);
    84	}
    85	
    86	Shape* MemoryManager::new_shape() {
    87	    return new_object<Shape>();
    88	}
    89	
    90	Shape* MemoryManager::get_empty_shape() noexcept {
    91	    if (empty_shape_ == nullptr) {
    92	        empty_shape_ = heap_.create<Shape>(); 
    93	        
    94	        if (gc_) gc_->register_permanent(empty_shape_);
    95	        
    96	        object_allocated_++;
    97	    }
    98	    return empty_shape_;
    99	}
   100	
   101	}


// =============================================================================
//  FILE PATH: src/module/module_manager.cpp
// =============================================================================

     1	#include "module/module_manager.h"
     2	
     3	#include "pch.h"
     4	#include <meow/core/module.h>
     5	#include <meow/core/string.h>
     6	#include <meow/memory/memory_manager.h>
     7	#include <meow/memory/gc_disable_guard.h>
     8	#include <meow/memory/gc_visitor.h>
     9	#include "module/module_utils.h"
    10	#include "bytecode/loader.h"
    11	
    12	namespace meow {
    13	
    14	static void link_module_to_proto(module_t mod, proto_t proto, std::unordered_set<proto_t>& visited) {
    15	    if (!proto || visited.contains(proto)) return;
    16	    visited.insert(proto);
    17	
    18	    proto->set_module(mod);
    19	
    20	    Chunk& chunk = const_cast<Chunk&>(proto->get_chunk());
    21	    for (size_t i = 0; i < chunk.get_pool_size(); ++i) {
    22	        if (chunk.get_constant(i).is_proto()) {
    23	            link_module_to_proto(mod, chunk.get_constant(i).as_proto(), visited);
    24	        }
    25	    }
    26	}
    27	
    28	ModuleManager::ModuleManager(MemoryManager* heap, Machine* vm) noexcept
    29	    : heap_(heap), vm_(vm), entry_path_(nullptr) {}
    30	
    31	module_t ModuleManager::load_module(string_t module_path_obj, string_t importer_path_obj) {
    32	    if (!module_path_obj) {
    33	        throw std::runtime_error("ModuleManager::load_module: Đường dẫn module là null.");
    34	    }
    35	
    36	    std::string module_path = module_path_obj->c_str();
    37	    std::string importer_path = importer_path_obj ? importer_path_obj->c_str() : "";
    38	
    39	    if (auto it = module_cache_.find(module_path_obj); it != module_cache_.end()) {
    40	        return it->second;
    41	    }
    42	    const std::vector<std::string> forbidden_extensions = {".meowb", ".meowc"};
    43	    
    44	    const std::vector<std::string> candidate_extensions = {get_platform_library_extension()};
    45	    std::filesystem::path root_dir =
    46	        detect_root_cached("meow-root", "$ORIGIN", true); //
    47	    std::vector<std::filesystem::path> search_roots = make_default_search_roots(root_dir);
    48	    std::string resolved_native_path = resolve_library_path_generic( //
    49	        module_path, importer_path, entry_path_ ? entry_path_->c_str() : "", forbidden_extensions,
    50	        candidate_extensions, search_roots, true);
    51	
    52	    if (!resolved_native_path.empty()) {
    53	        string_t resolved_native_path_obj = heap_->new_string(resolved_native_path);
    54	        if (auto it = module_cache_.find(resolved_native_path_obj); it != module_cache_.end()) {
    55	            module_cache_[module_path_obj] = it->second;
    56	            return it->second;
    57	        }
    58	
    59	        void* handle = open_native_library(resolved_native_path);
    60	        if (!handle) {
    61	            std::string err_detail = platform_last_error();
    62	            throw std::runtime_error("Không thể tải thư viện native '" + resolved_native_path +
    63	                                     "': " + err_detail);
    64	        }
    65	
    66	        const char* factory_symbol_name = "CreateMeowModule";
    67	        void* symbol = get_native_symbol(handle, factory_symbol_name);
    68	
    69	        if (!symbol) {
    70	            std::string err_detail = platform_last_error();
    71	            close_native_library(handle);
    72	            throw std::runtime_error("Không tìm thấy biểu tượng (symbol) '" + std::string(factory_symbol_name) +
    73	                                     "' trong thư viện native '" + resolved_native_path +
    74	                                     "': " + err_detail);
    75	        }
    76	
    77	        using NativeModuleFactory = module_t (*)(Machine*, MemoryManager*);
    78	        NativeModuleFactory factory = reinterpret_cast<NativeModuleFactory>(symbol);
    79	
    80	        module_t native_module = nullptr;
    81	        try {
    82	            native_module = factory(vm_, heap_);
    83	            if (!native_module) {
    84	                throw std::runtime_error("Hàm factory của module native '" + resolved_native_path +
    85	                                         "' trả về null.");
    86	            }
    87	            native_module->set_executed();
    88	        } catch (const std::exception& e) {
    89	            close_native_library(handle);
    90	            throw std::runtime_error(
    91	                "Ngoại lệ C++ khi gọi hàm factory của module native '" +
    92	                resolved_native_path + "': " + e.what());
    93	        } catch (...) {
    94	            close_native_library(handle);
    95	            throw std::runtime_error(
    96	                "Ngoại lệ không xác định khi gọi hàm factory của module native '" +
    97	                resolved_native_path + "'.");
    98	        }
    99	
   100	        module_cache_[module_path_obj] = native_module;
   101	        module_cache_[resolved_native_path_obj] = native_module;
   102	        return native_module;
   103	    }
   104	    
   105	    std::filesystem::path base_dir;
   106	    if (importer_path_obj == entry_path_) { 
   107	        base_dir = std::filesystem::path(entry_path_ ? entry_path_->c_str() : "").parent_path();
   108	    } else {
   109	        base_dir = std::filesystem::path(importer_path).parent_path();
   110	    }
   111	
   112	    std::filesystem::path binary_file_path_fs = normalize_path(base_dir / module_path);
   113	
   114	    if (binary_file_path_fs.extension() != ".meowc") {
   115	        binary_file_path_fs.replace_extension(".meowc");
   116	    }
   117	
   118	    std::string binary_file_path = binary_file_path_fs.string();
   119	    string_t binary_file_path_obj = heap_->new_string(binary_file_path);
   120	
   121	    if (auto it = module_cache_.find(binary_file_path_obj); it != module_cache_.end()) {
   122	        module_cache_[module_path_obj] = it->second;
   123	        return it->second;
   124	    }
   125	
   126	    std::ifstream file(binary_file_path, std::ios::binary | std::ios::ate);
   127	    if (!file.is_open()) {
   128	        throw std::runtime_error("Không thể mở tệp module (đã thử native và bytecode '" + 
   129	                                 binary_file_path + "')");
   130	    }
   131	    
   132	    std::streamsize size = file.tellg();
   133	    file.seekg(0, std::ios::beg);
   134	    
   135	    std::vector<uint8_t> buffer(static_cast<size_t>(size));
   136	    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
   137	         throw std::runtime_error("Không thể đọc tệp bytecode: " + binary_file_path);
   138	    }
   139	    
   140	    file.close();
   141	
   142	    GCDisableGuard guard(heap_);
   143	
   144	    proto_t main_proto = nullptr;
   145	    try {
   146	        Loader loader(heap_, buffer);
   147	        main_proto = loader.load_module();
   148	    } catch (const LoaderError& e) {
   149	        throw std::runtime_error("Tệp bytecode bị hỏng: " + binary_file_path + " - " + e.what());
   150	    }
   151	
   152	    if (!main_proto) {
   153	        throw std::runtime_error("Loader trả về proto null");
   154	    }
   155	
   156	    string_t filename_obj = heap_->new_string(binary_file_path_fs.filename().string());
   157	    module_t meow_module = heap_->new_module(filename_obj, binary_file_path_obj, main_proto);
   158	
   159	    Loader::link_module(meow_module);
   160	
   161	    std::unordered_set<proto_t> visited;
   162	    link_module_to_proto(meow_module, main_proto, visited);
   163	
   164	    module_cache_[module_path_obj] = meow_module;
   165	    module_cache_[binary_file_path_obj] = meow_module;
   166	    
   167	    // [FIX QUAN TRỌNG] Tự động Inject 'native' vào mọi module mới load
   168	    // Điều này đảm bảo các hàm như print, len, int... luôn có sẵn trong các file được import
   169	    if (std::string(filename_obj->c_str()) != "native") {
   170	        string_t native_name = heap_->new_string("native");
   171	        // Gọi đệ quy để lấy module native (đã được cache từ lúc khởi động Machine)
   172	        module_t native_mod = load_module(native_name, nullptr);
   173	        if (native_mod) {
   174	            meow_module->import_all_global(native_mod);
   175	        }
   176	    }
   177	    
   178	    return meow_module;
   179	}
   180	
   181	void ModuleManager::trace(GCVisitor& visitor) const noexcept {
   182	    // Duyệt qua tất cả module trong cache và báo cáo cho GC
   183	    for (const auto& [key, mod] : module_cache_) {
   184	        visitor.visit_object(key); // Mark cái tên module (String)
   185	        visitor.visit_object(mod); // Mark cái module object
   186	    }
   187	}
   188	
   189	}


// =============================================================================
//  FILE PATH: src/module/module_manager.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/core/module.h>
     5	#include <meow/common.h>
     6	
     7	namespace meow {
     8	    class Machine;
     9	    class MemoryManager;
    10	    struct GCVisitor;
    11	}
    12	
    13	namespace meow {
    14	class ModuleManager {
    15	public:
    16	    explicit ModuleManager(MemoryManager* heap, Machine* vm) noexcept;
    17	    ModuleManager(const ModuleManager&) = delete;
    18	    ModuleManager(ModuleManager&&) = default;
    19	    ModuleManager& operator=(const ModuleManager&) = delete;
    20	    ModuleManager& operator=(ModuleManager&&) = default;
    21	    ModuleManager() = default;
    22	
    23	    module_t load_module(string_t module_path, string_t importer_path);
    24	
    25	    inline void reset_cache() noexcept {
    26	        module_cache_.clear();
    27	    }
    28	
    29	    inline void add_cache(string_t name, module_t mod) {
    30	        module_cache_[name] = mod;
    31	    }
    32	
    33	    void trace(GCVisitor& visitor) const noexcept;
    34	private:
    35	    std::unordered_map<string_t, module_t> module_cache_;
    36	    MemoryManager* heap_;
    37	    Machine* vm_;
    38	    string_t entry_path_;
    39	};
    40	}


// =============================================================================
//  FILE PATH: src/module/module_utils.cpp
// =============================================================================

     1	#include "module/module_utils.h"
     2	#include "pch.h"
     3	#include <mutex>
     4	
     5	#if defined(_WIN32)
     6	#define WIN32_LEAN_AND_MEAN
     7	#include <windows.h>
     8	#else
     9	#include <dlfcn.h>
    10	#if defined(__APPLE__)
    11	#include <mach-o/dyld.h>
    12	#else
    13	#include <limits.h>
    14	#include <unistd.h>
    15	#endif
    16	#endif
    17	
    18	namespace meow {
    19	
    20	static inline std::string to_lower_copy(std::string s) noexcept {
    21	    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    22	    return s;
    23	}
    24	
    25	std::filesystem::path get_executable_dir() noexcept {
    26	    try {
    27	#if defined(_WIN32)
    28	        char buf[MAX_PATH];
    29	        DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    30	        if (len == 0) return std::filesystem::current_path();
    31	        return std::filesystem::path(std::string(buf, static_cast<size_t>(len))).parent_path();
    32	#elif defined(__APPLE__)
    33	        uint32_t size = 0;
    34	        if (_NSGetExecutablePath(nullptr, &size) != 0 && size == 0) return std::filesystem::current_path();
    35	        std::vector<char> buf(size ? size : 1);
    36	        if (_NSGetExecutablePath(buf.data(), &size) != 0) return std::filesystem::current_path();
    37	        return std::filesystem::absolute(std::filesystem::path(buf.data())).parent_path();
    38	#else
    39	        char buf[PATH_MAX];
    40	        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    41	        if (len == -1) return std::filesystem::current_path();
    42	        buf[len] = '\0';
    43	        return std::filesystem::path(std::string(buf, static_cast<size_t>(len))).parent_path();
    44	#endif
    45	    } catch (...) {
    46	        return std::filesystem::current_path();
    47	    }
    48	}
    49	
    50	std::filesystem::path normalize_path(const std::filesystem::path& p) noexcept {
    51	    try {
    52	        if (p.empty()) return p;
    53	        return std::filesystem::absolute(p).lexically_normal();
    54	    } catch (...) {
    55	        return p;
    56	    }
    57	}
    58	
    59	bool file_exists(const std::filesystem::path& p) noexcept {
    60	    try {
    61	        return std::filesystem::exists(p);
    62	    } catch (...) {
    63	        return false;
    64	    }
    65	}
    66	
    67	std::string read_first_non_empty_line_trimmed(const std::filesystem::path& path) noexcept {
    68	    try {
    69	        std::ifstream in(path);
    70	        if (!in) return std::string();
    71	        std::string line;
    72	        while (std::getline(in, line)) {
    73	            // trim both ends
    74	            auto ltrim = [](std::string& s) { s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); })); };
    75	            auto rtrim = [](std::string& s) { s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end()); };
    76	            rtrim(line);
    77	            ltrim(line);
    78	            if (!line.empty()) return line;
    79	        }
    80	    } catch (...) {
    81	    }
    82	    return std::string();
    83	}
    84	
    85	std::string expand_token(const std::string& raw, const std::string& token, const std::filesystem::path& replacement) noexcept {
    86	    if (token.empty() || !raw.contains(token)) return raw;
    87	    std::string out;
    88	    out.reserve(raw.size() + replacement.string().size());
    89	    size_t pos = 0;
    90	    while (true) {
    91	        size_t p = raw.find(token, pos);
    92	        if (p == std::string::npos) {
    93	            out.append(raw.substr(pos));
    94	            break;
    95	        }
    96	        out.append(raw.substr(pos, p - pos));
    97	        out.append(replacement.string());
    98	        pos = p + token.size();
    99	    }
   100	    return out;
   101	}
   102	
   103	// -------------------- root detection with caching (thread-safe, keyed)
   104	// --------------------
   105	struct cache_key {
   106	    std::string config_filename;
   107	    std::string token;
   108	    bool treat_bin_as_parent;
   109	    bool operator==(const cache_key& o) const noexcept {
   110	        return config_filename == o.config_filename && token == o.token && treat_bin_as_parent == o.treat_bin_as_parent;
   111	    }
   112	};
   113	namespace {
   114	struct key_hash {
   115	    size_t operator()(cache_key const& k) const noexcept {
   116	        std::hash<std::string> h;
   117	        size_t r = h(k.config_filename);
   118	        r = r * 1315423911u + h(k.token);
   119	        r ^= static_cast<size_t>(k.treat_bin_as_parent) + 0x9e3779b97f4a7c15ULL + (r << 6) + (r >> 2);
   120	        return r;
   121	    }
   122	};
   123	static std::mutex s_cache_mutex;
   124	static std::unordered_map<cache_key, std::filesystem::path, key_hash> s_root_cache;
   125	}
   126	
   127	std::filesystem::path detect_root_cached(const std::string& config_filename, const std::string& token, bool treat_bin_as_parent, std::function<std::filesystem::path()> exe_dir_provider) noexcept {
   128	    try {
   129	        cache_key k{config_filename, token, treat_bin_as_parent};
   130	        {
   131	            std::lock_guard<std::mutex> lk(s_cache_mutex);
   132	            auto it = s_root_cache.find(k);
   133	            if (it != s_root_cache.end()) return it->second;
   134	        }
   135	
   136	        std::filesystem::path exe_dir = exe_dir_provider();
   137	        if (!config_filename.empty()) {
   138	            std::filesystem::path config_path = exe_dir / config_filename;
   139	            if (file_exists(config_path)) {
   140	                std::string line = read_first_non_empty_line_trimmed(config_path);
   141	                if (!line.empty()) {
   142	                    std::string expanded = token.empty() ? line : expand_token(line, token, exe_dir);
   143	                    std::filesystem::path result = normalize_path(std::filesystem::path(expanded));
   144	                    {
   145	                        std::lock_guard<std::mutex> lk(s_cache_mutex);
   146	                        s_root_cache.emplace(k, result);
   147	                    }
   148	                    return result;
   149	                }
   150	            }
   151	        }
   152	
   153	        std::filesystem::path fallback = exe_dir;
   154	        if (treat_bin_as_parent && exe_dir.filename() == "bin") fallback = exe_dir.parent_path();
   155	        std::filesystem::path result = normalize_path(fallback);
   156	        {
   157	            std::lock_guard<std::mutex> lk(s_cache_mutex);
   158	            s_root_cache.emplace(k, result);
   159	        }
   160	        return result;
   161	    } catch (...) {
   162	        return std::filesystem::current_path();
   163	    }
   164	}
   165	
   166	// -------------------- default search roots helper --------------------
   167	std::vector<std::filesystem::path> make_default_search_roots(const std::filesystem::path& root) noexcept {
   168	    std::vector<std::filesystem::path> v;
   169	    try {
   170	        v.reserve(5);
   171	        v.push_back(normalize_path(root));
   172	        v.push_back(normalize_path(root / "lib"));
   173	        v.push_back(normalize_path(root / "stdlib"));
   174	        v.push_back(normalize_path(root / "bin" / "stdlib"));
   175	        v.push_back(normalize_path(root / "bin"));
   176	    } catch (...) {
   177	    }
   178	    return v;
   179	}
   180	
   181	std::string resolve_library_path_generic(const std::string& module_path, const std::string& importer, const std::string& entry_path, const std::vector<std::string>& forbidden_extensions,
   182	                                         const std::vector<std::string>& candidate_extensions, const std::vector<std::filesystem::path>& search_roots, bool extra_relative_search) noexcept {
   183	    try {
   184	        std::filesystem::path candidate(module_path);
   185	        std::string ext = candidate.extension().string();
   186	        if (!ext.empty()) {
   187	            std::string ext_l = to_lower_copy(ext);
   188	            for (const auto& f : forbidden_extensions) {
   189	                if (ext_l == to_lower_copy(f)) return "";
   190	            }
   191	            if (candidate.is_absolute() && file_exists(candidate)) return normalize_path(candidate).string();
   192	        }
   193	
   194	        std::vector<std::filesystem::path> to_try;
   195	        to_try.reserve(8);
   196	
   197	        if (candidate.extension().empty() && !candidate_extensions.empty()) {
   198	            for (const auto& ce : candidate_extensions) {
   199	                std::filesystem::path p = candidate;
   200	                p.replace_extension(ce);
   201	                to_try.push_back(p);
   202	            }
   203	        } else {
   204	            to_try.push_back(candidate);
   205	        }
   206	
   207	        for (const auto& root : search_roots) {
   208	            for (const auto& t : to_try) {
   209	                std::filesystem::path p = root / t;
   210	                if (file_exists(p)) return normalize_path(p).string();
   211	            }
   212	        }
   213	
   214	        for (const auto& t : to_try) {
   215	            if (file_exists(t)) return normalize_path(t).string();
   216	        }
   217	
   218	        if (extra_relative_search) {
   219	            std::filesystem::path base_dir;
   220	            if (importer == entry_path)
   221	                base_dir = std::filesystem::path(entry_path);
   222	            else
   223	                base_dir = std::filesystem::path(importer).parent_path();
   224	
   225	            for (const auto& t : to_try) {
   226	                std::filesystem::path p = normalize_path(base_dir / t);
   227	                if (file_exists(p)) return p.string();
   228	            }
   229	        }
   230	
   231	        return "";
   232	    } catch (...) {
   233	        return "";
   234	    }
   235	}
   236	
   237	std::string get_platform_library_extension() noexcept {
   238	#if defined(_WIN32)
   239	    return ".dll";
   240	#elif defined(__APPLE__)
   241	    return ".dylib";
   242	#else
   243	    return ".so";
   244	#endif
   245	}
   246	
   247	std::string platform_last_error() noexcept {
   248	#if defined(_WIN32)
   249	    DWORD err = GetLastError();
   250	    if (err == 0) return std::string();
   251	    LPSTR buf = nullptr;
   252	    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, nullptr);
   253	    std::string s = buf ? std::string(buf) : std::string();
   254	    if (buf) LocalFree(buf);
   255	    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
   256	    return s;
   257	#else
   258	    const char* e = dlerror();
   259	    return e ? std::string(e) : std::string();
   260	#endif
   261	}
   262	
   263	void* open_native_library(const std::string& path) noexcept {
   264	#if defined(_WIN32)
   265	    HMODULE h = LoadLibraryA(path.c_str());
   266	    return reinterpret_cast<void*>(h);
   267	#else
   268	    // clear previous errors
   269	    dlerror();
   270	    void* h = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
   271	    return h;
   272	#endif
   273	}
   274	
   275	void* get_native_symbol(void* handle, const char* symbol_name) noexcept {
   276	    if (!handle || !symbol_name) return nullptr;
   277	#if defined(_WIN32)
   278	    FARPROC p = GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol_name);
   279	    return reinterpret_cast<void*>(p);
   280	#else
   281	    dlerror();
   282	    void* p = dlsym(handle, symbol_name);
   283	    const char* err = dlerror();
   284	    (void)err;
   285	    return p;
   286	#endif
   287	}
   288	
   289	void close_native_library(void* handle) noexcept {
   290	    if (!handle) return;
   291	#if defined(_WIN32)
   292	    FreeLibrary(reinterpret_cast<HMODULE>(handle));
   293	#else
   294	    dlclose(handle);
   295	#endif
   296	}
   297	
   298	}



// =============================================================================
//  FILE PATH: src/module/module_utils.h
// =============================================================================

     1	#pragma once
     2	// module_loader_utils.h
     3	// Cross-platform, generic loader helpers (functions only, no classes).
     4	// Style: snake_case, caller-provided module-related strings (no hardcoding).
     5	// C++17
     6	
     7	#include "pch.h"
     8	
     9	namespace meow {
    10	
    11	// --- filesystem / string helpers ---
    12	std::filesystem::path get_executable_dir() noexcept;  // absolute dir of current executable
    13	                                                      // (fallback: current_path)
    14	std::filesystem::path normalize_path(const std::filesystem::path& p) noexcept;
    15	bool file_exists(const std::filesystem::path& p) noexcept;
    16	std::string read_first_non_empty_line_trimmed(const std::filesystem::path& path) noexcept;
    17	std::string expand_token(const std::string& raw, const std::string& token, const std::filesystem::path& replacement) noexcept;
    18	
    19	// --- root detection (generic) ---
    20	// config_filename: filename to look for next to exe_dir (caller decides, e.g.
    21	// "meow-root" or "") token: expansion token (caller decides, e.g. "$ORIGIN" or
    22	// "") treat_bin_as_parent: if exe_dir filename == "bin" treat
    23	// exe_dir.parent_path() as fallback root exe_dir_provider: optionally injected
    24	// (useful for tests) Returns absolute normalized path. Thread-safe (internally
    25	// caches results keyed by inputs).
    26	std::filesystem::path detect_root_cached(const std::string& config_filename, const std::string& token, bool treat_bin_as_parent = true,
    27	                                         std::function<std::filesystem::path()> exe_dir_provider = get_executable_dir) noexcept;
    28	
    29	// Build a set of common search roots from a detected root (caller can still
    30	// pass any roots). This is only a helper to produce typical patterns (root,
    31	// root/lib, root/stdlib, root/bin/stdlib, root/bin).
    32	std::vector<std::filesystem::path> make_default_search_roots(const std::filesystem::path& root) noexcept;
    33	
    34	// --- library resolution (fully generic) ---
    35	// module_path: import string (may be relative or absolute, with or without ext)
    36	// importer: the importer file path (full path) used to resolve relative imports
    37	// entry_path: entry path string used as special-case base (caller-defined
    38	// meaning) forbidden_extensions: extensions which indicate "language files" (do
    39	// not try native), lowercase expected but function is case-insensitive
    40	// candidate_extensions: list of extensions to try if module_path has no
    41	// extension (order matters) search_roots: additional absolute directories to
    42	// search (in order) extra_relative_search: attempt base_dir(importer)/module
    43	// candidates as last resort Returns absolute normalized existing path, or empty
    44	// string if not found.
    45	std::string resolve_library_path_generic(const std::string& module_path, const std::string& importer, const std::string& entry_path, const std::vector<std::string>& forbidden_extensions,
    46	                                         const std::vector<std::string>& candidate_extensions, const std::vector<std::filesystem::path>& search_roots, bool extra_relative_search = true) noexcept;
    47	
    48	// Return platform typical single library extension (helper only)
    49	std::string get_platform_library_extension() noexcept;
    50	
    51	// --- platform errors ---
    52	std::string platform_last_error() noexcept;  // human-friendly last error from
    53	                                             // OS loader (may be empty)
    54	
    55	// --- native library open/get_symbol/close (C-style functions, no classes) ---
    56	// open_native_library returns a platform handle as void* (NULL on failure).
    57	// - On POSIX: returns result of dlopen()
    58	// - On Windows: returns HMODULE cast to void*
    59	// get_native_symbol returns pointer to symbol or nullptr if not found
    60	// close_native_library closes/frees the handle
    61	void* open_native_library(const std::string& path) noexcept;
    62	void* get_native_symbol(void* handle, const char* symbol_name) noexcept;
    63	void close_native_library(void* handle) noexcept;
    64	
    65	}



// =============================================================================
//  FILE PATH: src/pch.h
// =============================================================================

     1	#pragma once
     2	
     3	// Containers
     4	#include <array>
     5	#include <optional>
     6	#include <string>
     7	#include <string_view>
     8	#include <unordered_map>
     9	#include <unordered_set>
    10	#include <vector>
    11	
    12	// Utilities
    13	#include <algorithm>
    14	#include <bit>
    15	#include <cctype>
    16	#include <cmath>
    17	#include <concepts>
    18	#include <cstdint>
    19	#include <format>
    20	#include <functional>
    21	#include <limits>
    22	#include <memory>
    23	#include <utility>
    24	
    25	// IO & Filesystem
    26	#include <filesystem>
    27	#include <fstream>
    28	#include <iomanip>
    29	#include <iostream>
    30	#include <sstream>
    31	#include <system_error>
    32	#include <print>
    33	
    34	// Error handling
    35	#include <cerrno>
    36	#include <cstdlib>
    37	#include <stdexcept>


// =============================================================================
//  FILE PATH: src/runtime/call_frame.h
// =============================================================================

     1	#pragma once
     2	#include <meow/common.h>
     3	
     4	namespace meow {
     5	
     6	struct CallFrame {
     7	    function_t function_ = nullptr;
     8	    Value* regs_base_ = nullptr; 
     9	    Value* ret_dest_ = nullptr;
    10	    const uint8_t* ip_ = nullptr;
    11	    CallFrame() = default;
    12	
    13	    CallFrame(function_t func, Value* regs, Value* ret, const uint8_t* ip)
    14	        : function_(func), regs_base_(regs), ret_dest_(ret), ip_(ip) {
    15	    }
    16	};
    17	
    18	}


// =============================================================================
//  FILE PATH: src/runtime/exception_handler.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	namespace meow {
     5	struct ExceptionHandler {
     6	    size_t catch_ip_;
     7	    size_t frame_depth_;
     8	    size_t stack_depth_;
     9	    size_t error_reg_;
    10	
    11	    ExceptionHandler(size_t catch_ip = 0, size_t frame_depth = 0, size_t stack_depth = 0, size_t error_reg = static_cast<size_t>(-1)) 
    12	        : catch_ip_(catch_ip), frame_depth_(frame_depth), stack_depth_(stack_depth), error_reg_(error_reg) {
    13	    }
    14	};
    15	}


// =============================================================================
//  FILE PATH: src/runtime/execution_context.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/core/objects.h>
     5	#include <meow/value.h>
     6	#include "runtime/call_frame.h"
     7	#include "runtime/exception_handler.h"
     8	
     9	namespace meow {
    10	
    11	struct ExecutionContext {
    12	    // --- CẤU HÌNH TĨNH ---
    13	    static constexpr size_t STACK_SIZE = 65536; 
    14	    static constexpr size_t FRAMES_MAX = 2048;
    15	
    16	    // --- BỘ NHỚ VẬT LÝ ---
    17	    Value stack_[STACK_SIZE]; 
    18	    CallFrame call_stack_[FRAMES_MAX];
    19	
    20	    // --- CON TRỎ QUẢN LÝ ---
    21	    Value* stack_top_ = nullptr; 
    22	    CallFrame* frame_ptr_ = nullptr;
    23	    Value* current_regs_ = nullptr;
    24	
    25	    // --- PHỤ TRỢ ---
    26	    std::vector<upvalue_t> open_upvalues_;
    27	    std::vector<ExceptionHandler> exception_handlers_;
    28	    CallFrame* current_frame_ = nullptr;
    29	
    30	    ExecutionContext() {
    31	        reset();
    32	    }
    33	
    34	    inline void reset() noexcept {
    35	        stack_top_ = stack_;
    36	        current_regs_ = stack_;
    37	        frame_ptr_ = call_stack_;
    38	        current_frame_ = frame_ptr_;
    39	        open_upvalues_.clear();
    40	        exception_handlers_.clear();
    41	    }
    42	
    43	    [[gnu::always_inline]]
    44	    inline bool check_overflow(size_t needed_slots) const noexcept {
    45	        return (stack_top_ + needed_slots) <= (stack_ + STACK_SIZE);
    46	    }
    47	    
    48	    [[gnu::always_inline]]
    49	    inline bool check_frame_overflow() const noexcept {
    50	        return (frame_ptr_ + 1) < (call_stack_ + FRAMES_MAX);
    51	    }
    52	
    53	    inline void trace(GCVisitor& visitor) const noexcept {
    54	        // 1. Trace Operand Stack
    55	        for (const Value* slot = stack_; slot < stack_top_; ++slot) {
    56	            visitor.visit_value(*slot);
    57	        }
    58	        
    59	        // 2. [FIX] Trace Call Stack
    60	        // Iterate from the first frame up to the current frame_ptr_
    61	        for (const CallFrame* frame = call_stack_; frame <= frame_ptr_; ++frame) {
    62	            if (frame->function_) {
    63	                visitor.visit_object(frame->function_);
    64	            }
    65	        }
    66	
    67	        // 3. Trace Open Upvalues
    68	        for (const auto& upvalue : open_upvalues_) {
    69	            visitor.visit_object(upvalue);
    70	        }
    71	    }
    72	};
    73	}


// =============================================================================
//  FILE PATH: src/runtime/operator_dispatcher.cpp
// =============================================================================

     1	#include "runtime/operator_dispatcher.h"
     2	#include <meow/memory/memory_manager.h>
     3	#include <meow/cast.h>
     4	#include <iostream>
     5	#include <cmath>
     6	#include <limits>
     7	
     8	namespace meow {
     9	
    10	static return_t trap_binary(MemoryManager*, param_t, param_t) {
    11	    return Value(null_t{});
    12	}
    13	
    14	static return_t trap_unary(MemoryManager*, param_t) {
    15	    return Value(null_t{});
    16	}
    17	
    18	static constexpr int64_t bool_to_int(bool b) { return b ? 1 : 0; }
    19	static constexpr double bool_to_double(bool b) { return b ? 1.0 : 0.0; }
    20	
    21	static Value string_concat(MemoryManager* heap, std::string_view s1, std::string_view s2) {
    22	    std::string res;
    23	    res.reserve(s1.size() + s2.size());
    24	    res.append(s1);
    25	    res.append(s2);
    26	    return Value(heap->new_string(res));
    27	}
    28	
    29	static Value string_repeat(MemoryManager* heap, std::string_view s, int64_t times) {
    30	    if (times <= 0) return Value(heap->new_string(""));
    31	    std::string res;
    32	    res.reserve(s.size() * static_cast<size_t>(times));
    33	    for (int64_t i = 0; i < times; ++i) res.append(s);
    34	    return Value(heap->new_string(res));
    35	}
    36	
    37	static Value safe_div(double a, double b) {
    38	    if (b == 0.0) {
    39	        if (a > 0.0) return Value(std::numeric_limits<double>::infinity());
    40	        if (a < 0.0) return Value(-std::numeric_limits<double>::infinity());
    41	        return Value(std::numeric_limits<double>::quiet_NaN());
    42	    }
    43	    return Value(a / b);
    44	}
    45	
    46	static Value pow_op(double a, double b) {
    47	    return Value(std::pow(a, b));
    48	}
    49	
    50	static bool loose_eq(param_t a, param_t b) noexcept {
    51	    if (a.is_int() && b.is_int()) return a.as_int() == b.as_int();
    52	    if (a.is_float() && b.is_float()) return std::abs(a.as_float() - b.as_float()) < std::numeric_limits<double>::epsilon();
    53	    if (a.is_int() && b.is_float()) return std::abs(static_cast<double>(a.as_int()) - b.as_float()) < std::numeric_limits<double>::epsilon();
    54	    if (a.is_float() && b.is_int()) return std::abs(a.as_float() - static_cast<double>(b.as_int())) < std::numeric_limits<double>::epsilon();
    55	    if (a.is_bool() && b.is_bool()) return a.as_bool() == b.as_bool();
    56	    if (a.is_string() && b.is_string()) return a.as_string() == b.as_string(); 
    57	    if (a.is_null() && b.is_null()) return true;
    58	    if (a.is_bool() && b.is_int()) return bool_to_int(a.as_bool()) == b.as_int();
    59	    if (a.is_int() && b.is_bool()) return a.as_int() == bool_to_int(b.as_bool());
    60	    if (a.is_object() && b.is_object()) {
    61	        return a.as_object() == b.as_object();
    62	    }
    63	
    64	    return false;
    65	}
    66	
    67	// --- Index Calculation ---
    68	consteval size_t calc_bin_idx(OpCode op, ValueType lhs, ValueType rhs) {
    69	    const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    70	    return (op_idx << (TYPE_BITS * 2)) | (std::to_underlying(lhs) << TYPE_BITS) | std::to_underlying(rhs);
    71	}
    72	
    73	consteval size_t calc_un_idx(OpCode op, ValueType rhs) {
    74	    const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    75	    return (op_idx << TYPE_BITS) | std::to_underlying(rhs);
    76	}
    77	
    78	consteval auto make_binary_table() {
    79	    std::array<binary_function_t, BINARY_TABLE_SIZE> table;
    80	    table.fill(trap_binary);
    81	
    82	    auto reg = [&](OpCode op, ValueType t1, ValueType t2, binary_function_t f) {
    83	        table[calc_bin_idx(op, t1, t2)] = f;
    84	    };
    85	
    86	    using enum OpCode;
    87	    using enum ValueType;
    88	
    89	    // ADD
    90	    reg(ADD, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() + b.as_int()); });
    91	    reg(ADD, Float, Float, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() + b.as_float()); });
    92	    reg(ADD, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<double>(a.as_int()) + b.as_float()); });
    93	    reg(ADD, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() + static_cast<double>(b.as_int())); });
    94	    
    95	    reg(ADD, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() + bool_to_int(b.as_bool())); });
    96	    reg(ADD, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) + b.as_int()); });
    97	    reg(ADD, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() + bool_to_double(b.as_bool())); });
    98	    reg(ADD, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_double(a.as_bool()) + b.as_float()); });
    99	    reg(ADD, Bool, Bool,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<int64_t>(bool_to_int(a.as_bool()) + bool_to_int(b.as_bool()))); });
   100	
   101	    reg(ADD, String, String, [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), b.as_string()->c_str()); });
   102	    
   103	    // Stateless wrappers for string concat with any
   104	    reg(ADD, String, Int,    [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });
   105	    reg(ADD, String, Float,  [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });
   106	    reg(ADD, String, Bool,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });
   107	    reg(ADD, String, Null,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, a.as_string()->c_str(), to_string(b)); });
   108	
   109	    reg(ADD, Int, String,    [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });
   110	    reg(ADD, Float, String,  [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });
   111	    reg(ADD, Bool, String,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });
   112	    reg(ADD, Null, String,   [](MemoryManager* h, param_t a, param_t b) { return string_concat(h, to_string(a), b.as_string()->c_str()); });
   113	
   114	    // SUB
   115	    reg(SUB, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() - b.as_int()); });
   116	    reg(SUB, Float, Float, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() - b.as_float()); });
   117	    reg(SUB, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<double>(a.as_int()) - b.as_float()); });
   118	    reg(SUB, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() - static_cast<double>(b.as_int())); });
   119	    reg(SUB, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() - bool_to_int(b.as_bool())); });
   120	    reg(SUB, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) - b.as_int()); });
   121	    reg(SUB, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() - bool_to_double(b.as_bool())); });
   122	    reg(SUB, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_double(a.as_bool()) - b.as_float()); });
   123	    reg(SUB, Bool, Bool,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<int64_t>(bool_to_int(a.as_bool()) - bool_to_int(b.as_bool()))); });
   124	
   125	    // MUL
   126	    reg(MUL, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() * b.as_int()); });
   127	    reg(MUL, Float, Float, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() * b.as_float()); });
   128	    reg(MUL, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<double>(a.as_int()) * b.as_float()); });
   129	    reg(MUL, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() * static_cast<double>(b.as_int())); });
   130	    reg(MUL, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() * bool_to_int(b.as_bool())); });
   131	    reg(MUL, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) * b.as_int()); });
   132	    reg(MUL, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_float() * bool_to_double(b.as_bool())); });
   133	    reg(MUL, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_double(a.as_bool()) * b.as_float()); });
   134	    
   135	    reg(MUL, String, Int,  [](MemoryManager* h, param_t a, param_t b) { return string_repeat(h, a.as_string()->c_str(), b.as_int()); });
   136	    reg(MUL, String, Bool, [](MemoryManager* h, param_t a, param_t b) { return string_repeat(h, a.as_string()->c_str(), bool_to_int(b.as_bool())); });
   137	
   138	    // DIV
   139	    reg(DIV, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return safe_div(static_cast<double>(a.as_int()), static_cast<double>(b.as_int())); });
   140	    reg(DIV, Float, Float, [](MemoryManager*, param_t a, param_t b) { return safe_div(a.as_float(), b.as_float()); });
   141	    reg(DIV, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return safe_div(static_cast<double>(a.as_int()), b.as_float()); });
   142	    reg(DIV, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return safe_div(a.as_float(), static_cast<double>(b.as_int())); });
   143	    reg(DIV, Int, Bool,    [](MemoryManager*, param_t a, param_t b) { return safe_div(static_cast<double>(a.as_int()), bool_to_double(b.as_bool())); });
   144	    reg(DIV, Bool, Int,    [](MemoryManager*, param_t a, param_t b) { return safe_div(bool_to_double(a.as_bool()), static_cast<double>(b.as_int())); });
   145	    reg(DIV, Bool, Float,  [](MemoryManager*, param_t a, param_t b) { return safe_div(bool_to_double(a.as_bool()), b.as_float()); });
   146	    reg(DIV, Float, Bool,  [](MemoryManager*, param_t a, param_t b) { return safe_div(a.as_float(), bool_to_double(b.as_bool())); });
   147	
   148	    // MOD
   149	    reg(MOD, Int, Int, [](MemoryManager*, param_t a, param_t b) {
   150	        if (b.as_int() == 0) return Value(std::numeric_limits<double>::quiet_NaN());
   151	        return Value(a.as_int() % b.as_int());
   152	    });
   153	    reg(MOD, Int, Bool, [](MemoryManager*, param_t a, param_t b) {
   154	        int64_t div = bool_to_int(b.as_bool());
   155	        if (div == 0) return Value(std::numeric_limits<double>::quiet_NaN());
   156	        return Value(a.as_int() % div);
   157	    });
   158	
   159	    // POW
   160	    reg(POW, Int, Int,     [](MemoryManager*, param_t a, param_t b) { return pow_op(static_cast<double>(a.as_int()), static_cast<double>(b.as_int())); });
   161	    reg(POW, Float, Float, [](MemoryManager*, param_t a, param_t b) { return pow_op(a.as_float(), b.as_float()); });
   162	    reg(POW, Int, Float,   [](MemoryManager*, param_t a, param_t b) { return pow_op(static_cast<double>(a.as_int()), b.as_float()); });
   163	    reg(POW, Float, Int,   [](MemoryManager*, param_t a, param_t b) { return pow_op(a.as_float(), static_cast<double>(b.as_int())); });
   164	
   165	    // BITWISE
   166	    reg(BIT_AND, Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() & b.as_int()); });
   167	    reg(BIT_OR,  Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() | b.as_int()); });
   168	    reg(BIT_XOR, Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() ^ b.as_int()); });
   169	    reg(LSHIFT,  Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() << b.as_int()); });
   170	    reg(RSHIFT,  Int, Int, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() >> b.as_int()); });
   171	    
   172	    // Explicit casts to bool or int to resolve ambiguity
   173	    reg(BIT_AND, Bool, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<bool>(static_cast<int>(a.as_bool()) & static_cast<int>(b.as_bool()))); });
   174	    reg(BIT_OR,  Bool, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<bool>(static_cast<int>(a.as_bool()) | static_cast<int>(b.as_bool()))); });
   175	    reg(BIT_XOR, Bool, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(static_cast<bool>(static_cast<int>(a.as_bool()) ^ static_cast<int>(b.as_bool()))); });
   176	
   177	    reg(BIT_AND, Int, Bool, [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() & bool_to_int(b.as_bool())); });
   178	    reg(BIT_AND, Bool, Int, [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) & b.as_int()); });
   179	    reg(BIT_OR, Int, Bool,  [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() | bool_to_int(b.as_bool())); });
   180	    reg(BIT_OR, Bool, Int,  [](MemoryManager*, param_t a, param_t b) { return Value(bool_to_int(a.as_bool()) | b.as_int()); });
   181	
   182	    // EQUALITY
   183	    for (size_t i = 0; i < (1 << TYPE_BITS); ++i) {
   184	        for (size_t j = 0; j < (1 << TYPE_BITS); ++j) {
   185	            auto t1 = static_cast<ValueType>(i);
   186	            auto t2 = static_cast<ValueType>(j);
   187	            
   188	            // loose_eq đã tự xử lý việc check type bên trong, nên an toàn tuyệt đối
   189	            reg(EQ, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(loose_eq(a, b)); });
   190	            reg(NEQ, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(!loose_eq(a, b)); });
   191	        }
   192	    }
   193	
   194	    // COMPARISON
   195	    reg(LT, Int, Int,       [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() < b.as_int()); });
   196	    reg(GT, Int, Int,       [](MemoryManager*, param_t a, param_t b) { return Value(a.as_int() > b.as_int()); });
   197	    
   198	    auto reg_cmp = [&](ValueType t1, ValueType t2) {
   199	        reg(LT, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) < to_float(b)); });
   200	        reg(GT, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) > to_float(b)); });
   201	        reg(LE, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) <= to_float(b)); });
   202	        reg(GE, t1, t2, [](MemoryManager*, param_t a, param_t b) { return Value(to_float(a) >= to_float(b)); });
   203	    };
   204	
   205	    reg_cmp(Float, Float); reg_cmp(Int, Float); reg_cmp(Float, Int);
   206	    reg_cmp(Int, Bool); reg_cmp(Bool, Int); reg_cmp(Bool, Bool);
   207	
   208	    reg(LT, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) < std::string_view(b.as_string()->c_str())); });
   209	    reg(GT, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) > std::string_view(b.as_string()->c_str())); });
   210	    reg(LE, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) <= std::string_view(b.as_string()->c_str())); });
   211	    reg(GE, String, String, [](MemoryManager*, param_t a, param_t b) { return Value(std::string_view(a.as_string()->c_str()) >= std::string_view(b.as_string()->c_str())); });
   212	
   213	    return table;
   214	}
   215	
   216	consteval auto make_unary_table() {
   217	    std::array<unary_function_t, UNARY_TABLE_SIZE> table;
   218	    table.fill(trap_unary);
   219	
   220	    auto reg = [&](OpCode op, ValueType t, unary_function_t f) {
   221	        table[calc_un_idx(op, t)] = f;
   222	    };
   223	
   224	    using enum OpCode;
   225	    using enum ValueType;
   226	
   227	    reg(NEG, Int,   [](MemoryManager*, param_t v) { return Value(-v.as_int()); });
   228	    reg(NEG, Float, [](MemoryManager*, param_t v) { return Value(-v.as_float()); });
   229	    reg(NEG, Bool,  [](MemoryManager*, param_t v) { return Value(-bool_to_int(v.as_bool())); });
   230	
   231	    reg(BIT_NOT, Int,  [](MemoryManager*, param_t v) { return Value(~v.as_int()); });
   232	    reg(BIT_NOT, Bool, [](MemoryManager*, param_t v) { return Value(~bool_to_int(v.as_bool())); });
   233	
   234	    auto logic_not = [](MemoryManager*, param_t v) { return Value(!to_bool(v)); };
   235	    
   236	    reg(NOT, Null, logic_not);
   237	    reg(NOT, Bool, logic_not);
   238	    reg(NOT, Int, logic_not);
   239	    reg(NOT, Float, logic_not);
   240	    reg(NOT, String, logic_not);
   241	    reg(NOT, Array, logic_not);
   242	    reg(NOT, HashTable, logic_not);
   243	    reg(NOT, Instance, logic_not);
   244	    reg(NOT, Class, logic_not);
   245	    reg(NOT, Function, logic_not);
   246	    reg(NOT, Module, logic_not);
   247	
   248	    return table;
   249	}
   250	
   251	constinit const std::array<binary_function_t, BINARY_TABLE_SIZE> OperatorDispatcher::binary_dispatch_table_ = make_binary_table();
   252	constinit const std::array<unary_function_t, UNARY_TABLE_SIZE> OperatorDispatcher::unary_dispatch_table_  = make_unary_table();
   253	
   254	} // namespace meow


// =============================================================================
//  FILE PATH: src/runtime/operator_dispatcher.h
// =============================================================================

     1	#pragma once
     2	#include <meow/bytecode/op_codes.h>
     3	#include <meow/value.h>
     4	#include <meow/common.h>
     5	#include <meow/cast.h>
     6	
     7	namespace meow {
     8	
     9	class MemoryManager;
    10	constexpr size_t TYPE_BITS = 4;
    11	
    12	constexpr size_t OP_BITS_COMPACT = 5; 
    13	constexpr size_t OP_OFFSET = std::to_underlying(OpCode::__BEGIN_OPERATOR__) + 1;
    14	
    15	constexpr size_t BINARY_TABLE_SIZE = (1 << OP_BITS_COMPACT) * (1 << TYPE_BITS) * (1 << TYPE_BITS);
    16	constexpr size_t UNARY_TABLE_SIZE  = (1 << OP_BITS_COMPACT) * (1 << TYPE_BITS);
    17	
    18	using binary_function_t = return_t (*)(MemoryManager*, param_t, param_t);
    19	using unary_function_t  = return_t (*)(MemoryManager*, param_t);
    20	
    21	class OperatorDispatcher {
    22	public:
    23	    [[gnu::always_inline]] 
    24	    static inline ValueType get_detailed_type(param_t v) noexcept {
    25	        if (v.is_object()) {
    26	            switch (v.as_object()->get_type()) {
    27	                case ObjectType::STRING: return ValueType::String;
    28	                case ObjectType::ARRAY:  return ValueType::Array;
    29	                case ObjectType::HASH_TABLE: return ValueType::HashTable;
    30	                default: return ValueType::Object;
    31	            }
    32	        }
    33	        return static_cast<ValueType>(v.index());
    34	    }
    35	
    36	    [[nodiscard]] 
    37	    [[gnu::always_inline]] static binary_function_t find(OpCode op, param_t lhs, param_t rhs) noexcept {
    38	        const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    39	        
    40	        const size_t t1 = std::to_underlying(get_detailed_type(lhs));
    41	        const size_t t2 = std::to_underlying(get_detailed_type(rhs));
    42	
    43	        const size_t idx = (op_idx << (TYPE_BITS * 2)) | (t1 << TYPE_BITS) | t2;
    44	                           
    45	        return binary_dispatch_table_[idx];
    46	    }
    47	
    48	    [[nodiscard]]
    49	    [[gnu::always_inline]] static unary_function_t find(OpCode op, param_t rhs) noexcept {
    50	        const size_t op_idx = std::to_underlying(op) - OP_OFFSET;
    51	        
    52	        const size_t t1 = std::to_underlying(get_detailed_type(rhs));
    53	        
    54	        const size_t idx = (op_idx << TYPE_BITS) | t1;
    55	        return unary_dispatch_table_[idx];
    56	    }
    57	
    58	private:
    59	    static const std::array<binary_function_t, BINARY_TABLE_SIZE> binary_dispatch_table_;
    60	    static const std::array<unary_function_t, UNARY_TABLE_SIZE> unary_dispatch_table_;
    61	};
    62	
    63	} // namespace meow


// =============================================================================
//  FILE PATH: src/runtime/upvalue.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include "runtime/execution_context.h"
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/common.h>
     7	
     8	namespace meow {
     9	inline upvalue_t capture_upvalue(ExecutionContext* context, MemoryManager* heap, size_t register_index) noexcept {
    10	    for (auto it = context->open_upvalues_.rbegin(); it != context->open_upvalues_.rend(); ++it) {
    11	        upvalue_t uv = *it;
    12	        if (uv->get_index() == register_index) return uv;
    13	        if (uv->get_index() < register_index) break;
    14	    }
    15	
    16	    upvalue_t new_uv = heap->new_upvalue(register_index);
    17	    auto it = std::lower_bound(context->open_upvalues_.begin(), context->open_upvalues_.end(), new_uv, [](auto a, auto b) { return a->get_index() < b->get_index(); });
    18	    context->open_upvalues_.insert(it, new_uv);
    19	    return new_uv;
    20	}
    21	
    22	inline void close_upvalues(ExecutionContext* context, size_t last_index) noexcept {
    23	    while (!context->open_upvalues_.empty() && context->open_upvalues_.back()->get_index() >= last_index) {
    24	        upvalue_t uv = context->open_upvalues_.back();
    25	        // [FIX] Truy cập mảng tĩnh stack_ thay vì vector registers_
    26	        uv->close(context->stack_[uv->get_index()]);
    27	        context->open_upvalues_.pop_back();
    28	    }
    29	}
    30	
    31	inline upvalue_t capture_upvalue(ExecutionContext& context, MemoryManager& heap, size_t register_index) noexcept {
    32	    return capture_upvalue(&context, &heap, register_index);
    33	}
    34	
    35	inline void close_upvalues(ExecutionContext& context, size_t last_index) noexcept {
    36	    return close_upvalues(&context, last_index);
    37	}
    38	}


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


// =============================================================================
//  FILE PATH: src/vm/builtins.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include <meow/machine.h>
     3	#include <meow/core/objects.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include "module/module_manager.h"
     7	#include <meow/cast.h>
     8	#include "vm/stdlib/stdlib.h"
     9	
    10	namespace meow {
    11	
    12	#define CHECK_ARGS(n) \
    13	    if (argc < n) [[unlikely]] vm->error("Native function expects at least " #n " arguments.");
    14	
    15	namespace natives {
    16	
    17	// print(val1, val2, ...)
    18	static Value print([[maybe_unused]] Machine* vm, int argc, Value* argv) {
    19	    for (int i = 0; i < argc; ++i) {
    20	        if (i > 0) std::cout << " ";
    21	        std::print("{}", to_string(argv[i]));
    22	    }
    23	    std::println("");
    24	    return Value(null_t{});
    25	}
    26	
    27	// typeof(value)
    28	static Value type_of(Machine* vm, int argc, Value* argv) {
    29	    CHECK_ARGS(1);
    30	    Value v = argv[0];
    31	    std::string type_str = "unknown";
    32	
    33	    if (v.is_null()) type_str = "null";
    34	    else if (v.is_bool()) type_str = "bool";
    35	    else if (v.is_int()) type_str = "int";
    36	    else if (v.is_float()) type_str = "real";
    37	    else if (v.is_string()) type_str = "string";
    38	    else if (v.is_array()) type_str = "array";
    39	    else if (v.is_hash_table()) type_str = "object";
    40	    else if (v.is_function() || v.is_native() || v.is_bound_method()) type_str = "function";
    41	    else if (v.is_class()) type_str = "class";
    42	    else if (v.is_instance()) type_str = "instance";
    43	    else if (v.is_module()) type_str = "module";
    44	
    45	    return Value(vm->get_heap()->new_string(type_str));
    46	}
    47	
    48	// len(container)
    49	static Value len(Machine* vm, int argc, Value* argv) {
    50	    CHECK_ARGS(1);
    51	    Value v = argv[0];
    52	    int64_t length = -1;
    53	
    54	    if (v.is_string()) length = v.as_string()->size();
    55	    else if (v.is_array()) length = v.as_array()->size();
    56	    else if (v.is_hash_table()) length = v.as_hash_table()->size();
    57	    
    58	    return Value(length);
    59	}
    60	
    61	// assert(condition, message?)
    62	static Value assert_fn(Machine* vm, int argc, Value* argv) {
    63	    CHECK_ARGS(1);
    64	    if (!to_bool(argv[0])) {
    65	        std::string msg = "Assertion failed.";
    66	        if (argc > 1 && argv[1].is_string()) {
    67	            msg = argv[1].as_string()->c_str();
    68	        }
    69	        vm->error(msg);
    70	    }
    71	    return Value(null_t{});
    72	}
    73	
    74	// int(value)
    75	static Value to_int_fn(Machine* vm, int argc, Value* argv) {
    76	    CHECK_ARGS(1);
    77	    return Value(to_int(argv[0]));
    78	}
    79	
    80	// real(value)
    81	static Value to_real_fn(Machine* vm, int argc, Value* argv) {
    82	    CHECK_ARGS(1);
    83	    return Value(to_float(argv[0]));
    84	}
    85	
    86	// bool(value)
    87	static Value to_bool_fn(Machine* vm, int argc, Value* argv) {
    88	    CHECK_ARGS(1);
    89	    return Value(to_bool(argv[0]));
    90	}
    91	
    92	// str(value)
    93	static Value to_str_fn(Machine* vm, int argc, Value* argv) {
    94	    CHECK_ARGS(1);
    95	    return Value(vm->get_heap()->new_string(to_string(argv[0])));
    96	}
    97	
    98	// ord(char_string)
    99	static Value ord(Machine* vm, int argc, Value* argv) {
   100	    CHECK_ARGS(1);
   101	    if (!argv[0].is_string()) vm->error("ord() expects a string.");
   102	    string_t s = argv[0].as_string();
   103	    if (s->size() != 1) vm->error("ord() expects a single character.");
   104	    return Value(static_cast<int64_t>(static_cast<unsigned char>(s->get(0))));
   105	}
   106	
   107	// char(code)
   108	static Value chr(Machine* vm, int argc, Value* argv) {
   109	    CHECK_ARGS(1);
   110	    if (!argv[0].is_int()) vm->error("char() expects an integer.");
   111	    int64_t code = argv[0].as_int();
   112	    if (code < 0 || code > 255) vm->error("char() code out of range [0-255].");
   113	    char c = static_cast<char>(code);
   114	    return Value(vm->get_heap()->new_string(std::string(1, c)));
   115	}
   116	
   117	// range(stop) or range(start, stop) or range(start, stop, step)
   118	static Value range(Machine* vm, int argc, Value* argv) {
   119	    CHECK_ARGS(1);
   120	    int64_t start = 0;
   121	    int64_t stop = 0;
   122	    int64_t step = 1;
   123	
   124	    if (argc == 1) {
   125	        stop = to_int(argv[0]);
   126	    } else if (argc == 2) {
   127	        start = to_int(argv[0]);
   128	        stop = to_int(argv[1]);
   129	    } else {
   130	        start = to_int(argv[0]);
   131	        stop = to_int(argv[1]);
   132	        step = to_int(argv[2]);
   133	    }
   134	
   135	    if (step == 0) vm->error("range() step cannot be 0.");
   136	
   137	    auto arr = vm->get_heap()->new_array();
   138	    
   139	    if (step > 0) {
   140	        for (int64_t i = start; i < stop; i += step) {
   141	            arr->push(Value(i));
   142	        }
   143	    } else {
   144	        for (int64_t i = start; i > stop; i += step) {
   145	            arr->push(Value(i));
   146	        }
   147	    }
   148	
   149	    return Value(arr);
   150	}
   151	
   152	// clock()
   153	static Value clock_fn([[maybe_unused]] Machine* vm, int argc, Value* argv) {
   154	    auto now = std::chrono::steady_clock::now();
   155	    auto duration = now.time_since_epoch();
   156	    double millis = std::chrono::duration<double, std::milli>(duration).count();
   157	    return Value(millis);
   158	}
   159	
   160	} // namespace natives
   161	
   162	void Machine::load_builtins() {
   163	    auto name_native = heap_->new_string("native");
   164	    auto mod = heap_->new_module(name_native, name_native);
   165	
   166	    auto reg = [&](const char* name, native_t fn) {
   167	        mod->set_global(heap_->new_string(name), Value(fn));
   168	    };
   169	
   170	    // Đăng ký danh sách hàm
   171	    reg("print", natives::print);
   172	    reg("typeof", natives::type_of);
   173	    reg("len", natives::len);
   174	    reg("assert", natives::assert_fn);
   175	    reg("int", natives::to_int_fn);
   176	    reg("real", natives::to_real_fn);
   177	    reg("bool", natives::to_bool_fn);
   178	    reg("str", natives::to_str_fn);
   179	    reg("ord", natives::ord);
   180	    reg("char", natives::chr);
   181	    reg("range", natives::range);
   182	    reg("clock", natives::clock_fn);
   183	
   184	    mod_manager_->add_cache(name_native, mod);
   185	    mod_manager_->add_cache(heap_->new_string("io"), stdlib::create_io_module(this, heap_.get()));
   186	    mod_manager_->add_cache(heap_->new_string("system"), stdlib::create_system_module(this, heap_.get()));
   187	    mod_manager_->add_cache(heap_->new_string("array"), stdlib::create_array_module(this, heap_.get()));
   188	    mod_manager_->add_cache(heap_->new_string("string"), stdlib::create_string_module(this, heap_.get()));
   189	    mod_manager_->add_cache(heap_->new_string("object"), stdlib::create_object_module(this, heap_.get()));
   190	    mod_manager_->add_cache(heap_->new_string("json"), stdlib::create_json_module(this, heap_.get()));
   191	    mod_manager_->add_cache(heap_->new_string("memory"), stdlib::create_memory_module(this, heap_.get()));
   192	
   193	}
   194	
   195	} // namespace meow


// =============================================================================
//  FILE PATH: src/vm/handlers/data_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	#include <meow/cast.h>
     5	
     6	namespace meow::handlers {
     7	
     8	
     9	[[gnu::always_inline]] static const uint8_t* impl_LOAD_CONST(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    10	    uint16_t dst = read_u16(ip);
    11	    uint16_t idx = read_u16(ip);
    12	    regs[dst] = constants[idx];
    13	    return ip;
    14	}
    15	
    16	[[gnu::always_inline]] static const uint8_t* impl_LOAD_NULL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    17	    uint16_t dst = read_u16(ip);
    18	    regs[dst] = null_t{};
    19	    return ip;
    20	}
    21	
    22	[[gnu::always_inline]] static const uint8_t* impl_LOAD_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    23	    uint16_t dst = read_u16(ip);
    24	    regs[dst] = true;
    25	    return ip;
    26	}
    27	
    28	[[gnu::always_inline]] static const uint8_t* impl_LOAD_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    29	    uint16_t dst = read_u16(ip);
    30	    regs[dst] = false;
    31	    return ip;
    32	}
    33	
    34	[[gnu::always_inline]] static const uint8_t* impl_LOAD_INT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    35	    uint16_t dst = read_u16(ip);
    36	    regs[dst] = *reinterpret_cast<const int64_t*>(ip);
    37	    ip += 8;
    38	    return ip;
    39	}
    40	
    41	[[gnu::always_inline]] static const uint8_t* impl_LOAD_FLOAT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    42	    uint16_t dst = read_u16(ip);
    43	    regs[dst] = *reinterpret_cast<const double*>(ip);
    44	    ip += 8;
    45	    return ip;
    46	}
    47	
    48	[[gnu::always_inline]] static const uint8_t* impl_MOVE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    49	    uint16_t dst = read_u16(ip);
    50	    uint16_t src = read_u16(ip);
    51	    regs[dst] = regs[src];
    52	    return ip;
    53	}
    54	
    55	[[gnu::always_inline]] static const uint8_t* impl_NEW_ARRAY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    56	    uint16_t dst = read_u16(ip);
    57	    uint16_t start_idx = read_u16(ip);
    58	    uint16_t count = read_u16(ip);
    59	
    60	    auto array = state->heap.new_array();
    61	    regs[dst] = object_t(array);
    62	    array->reserve(count);
    63	    for (size_t i = 0; i < count; ++i) {
    64	        array->push(regs[start_idx + i]);
    65	    }
    66	    return ip;
    67	}
    68	
    69	[[gnu::always_inline]] 
    70	static const uint8_t* impl_NEW_HASH(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    71	    uint16_t dst = read_u16(ip);
    72	    uint16_t start_idx = read_u16(ip);
    73	    uint16_t count = read_u16(ip);
    74	    
    75	    auto hash = state->heap.new_hash(count); 
    76	
    77	    regs[dst] = Value(hash); 
    78	
    79	    for (size_t i = 0; i < count; ++i) {
    80	        Value& key = regs[start_idx + i * 2];
    81	        Value& val = regs[start_idx + i * 2 + 1];
    82	        
    83	        if (key.is_string()) [[likely]] {
    84	            hash->set(key.as_string(), val);
    85	        } else {
    86	            std::string s = to_string(key);
    87	            string_t k = state->heap.new_string(s);
    88	            hash->set(k, val);
    89	        }
    90	    }
    91	    
    92	    return ip;
    93	}
    94	[[gnu::always_inline]] static const uint8_t* impl_GET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    95	    uint16_t dst = read_u16(ip);
    96	    uint16_t src_reg = read_u16(ip);
    97	    uint16_t key_reg = read_u16(ip);
    98	    
    99	    Value& src = regs[src_reg];
   100	    Value& key = regs[key_reg];
   101	
   102	    if (src.is_array()) {
   103	        if (!key.is_int()) {
   104	            state->error("Array index phải là số nguyên.");
   105	            return impl_PANIC(ip, regs, constants, state);
   106	        }
   107	        array_t arr = src.as_array();
   108	        int64_t idx = key.as_int();
   109	        // if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
   110	        //     state->error("Array index out of bounds.");
   111	        //     return impl_PANIC(ip, regs, constants, state);
   112	        // }
   113	        // regs[dst] = arr->get(idx);
   114	
   115	        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
   116	            regs[dst] = null_t{};
   117	        } else {
   118	            regs[dst] = arr->get(idx);
   119	        }
   120	    } 
   121	    else if (src.is_hash_table()) {
   122	        hash_table_t hash = src.as_hash_table();
   123	        string_t k = nullptr;
   124	        
   125	        if (!key.is_string()) {
   126	            std::string s = to_string(key);
   127	            k = state->heap.new_string(s);
   128	        } else {
   129	            k = key.as_string();
   130	        }
   131	
   132	        if (hash->has(k)) {
   133	            regs[dst] = hash->get(k);
   134	        } else {
   135	            regs[dst] = Value(null_t{});
   136	        }
   137	    }
   138	    else if (src.is_string()) {
   139	        if (!key.is_int()) {
   140	            state->error("String index phải là số nguyên.");
   141	            return impl_PANIC(ip, regs, constants, state);
   142	        }
   143	        string_t str = src.as_string();
   144	        int64_t idx = key.as_int();
   145	        if (idx < 0 || static_cast<size_t>(idx) >= str->size()) {
   146	            state->error("String index out of bounds.");
   147	            return impl_PANIC(ip, regs, constants, state);
   148	        }
   149	        char c = str->get(idx);
   150	        regs[dst] = Value(state->heap.new_string(&c, 1));
   151	    }
   152	
   153	    else if (src.is_instance()) {
   154	        if (!key.is_string()) {
   155	            state->error("Instance index key phải là chuỗi (tên thuộc tính/phương thức).");
   156	            return impl_PANIC(ip, regs, constants, state);
   157	        }
   158	        
   159	        string_t name = key.as_string();
   160	        instance_t inst = src.as_instance();
   161	        
   162	        int offset = inst->get_shape()->get_offset(name);
   163	        if (offset != -1) {
   164	            regs[dst] = inst->get_field_at(offset);
   165	        } 
   166	        else {
   167	            class_t k = inst->get_class();
   168	            Value method = null_t{};
   169	            
   170	            while (k) {
   171	                if (k->has_method(name)) {
   172	                    method = k->get_method(name);
   173	                    break;
   174	                }
   175	                k = k->get_super();
   176	            }
   177	            
   178	            if (!method.is_null()) {
   179	                if (method.is_function() || method.is_native()) {
   180	                    auto bound = state->heap.new_bound_method(src, method);
   181	                    regs[dst] = Value(bound);
   182	                } else {
   183	                    regs[dst] = method;
   184	                }
   185	            } else {
   186	                regs[dst] = Value(null_t{});
   187	            }
   188	        }
   189	    }
   190	
   191	    else {
   192	        state->error("Không thể dùng toán tử index [] trên kiểu dữ liệu này.");
   193	        return impl_PANIC(ip, regs, constants, state);
   194	    }
   195	    return ip;
   196	}
   197	
   198	[[gnu::always_inline]] static const uint8_t* impl_SET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   199	    uint16_t src_reg = read_u16(ip);
   200	    uint16_t key_reg = read_u16(ip);
   201	    uint16_t val_reg = read_u16(ip);
   202	
   203	    Value& src = regs[src_reg];
   204	    Value& key = regs[key_reg];
   205	    Value& val = regs[val_reg];
   206	
   207	    if (src.is_array()) {
   208	        if (!key.is_int()) {
   209	            state->error("Array index phải là số nguyên.");
   210	            return impl_PANIC(ip, regs, constants, state);
   211	        }
   212	        array_t arr = src.as_array();
   213	        int64_t idx = key.as_int();
   214	        if (idx < 0) {
   215	            state->error("Array index không được âm.");
   216	            return impl_PANIC(ip, regs, constants, state);
   217	        }
   218	        if (static_cast<size_t>(idx) >= arr->size()) {
   219	            arr->resize(idx + 1);
   220	        }
   221	        
   222	        arr->set(idx, val);
   223	        state->heap.write_barrier(src.as_object(), val);
   224	    }
   225	    else if (src.is_hash_table()) {
   226	        hash_table_t hash = src.as_hash_table();
   227	        string_t k = nullptr;
   228	
   229	        if (!key.is_string()) {
   230	            std::string s = to_string(key);
   231	            k = state->heap.new_string(s);
   232	        } else {
   233	            k = key.as_string();
   234	        }
   235	
   236	        hash->set(k, val);
   237	        
   238	        state->heap.write_barrier(src.as_object(), val);
   239	    } 
   240	        else if (src.is_instance()) {
   241	        if (!key.is_string()) {
   242	            state->error("Instance set index key phải là chuỗi.");
   243	            return impl_PANIC(ip, regs, constants, state);
   244	        }
   245	        
   246	        string_t name = key.as_string();
   247	        instance_t inst = src.as_instance();
   248	        
   249	        int offset = inst->get_shape()->get_offset(name);
   250	        if (offset != -1) {
   251	            inst->set_field_at(offset, val);
   252	            state->heap.write_barrier(inst, val);
   253	        } else {
   254	            Shape* current_shape = inst->get_shape();
   255	            Shape* next_shape = current_shape->get_transition(name);
   256	            if (next_shape == nullptr) {
   257	                next_shape = current_shape->add_transition(name, &state->heap);
   258	            }
   259	            
   260	            inst->set_shape(next_shape);
   261	            
   262	            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape)));
   263	
   264	            inst->add_field(val);
   265	            state->heap.write_barrier(inst, val);
   266	        }
   267	    }
   268	    else {
   269	        state->error("Không thể gán index [] trên kiểu dữ liệu này.");
   270	        return impl_PANIC(ip, regs, constants, state);
   271	    }
   272	    return ip;
   273	}
   274	[[gnu::always_inline]] static const uint8_t* impl_GET_KEYS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   275	    uint16_t dst = read_u16(ip);
   276	    uint16_t src_reg = read_u16(ip);
   277	    Value& src = regs[src_reg];
   278	    
   279	    auto keys_array = state->heap.new_array();
   280	    
   281	    if (src.is_hash_table()) {
   282	        hash_table_t hash = src.as_hash_table();
   283	        keys_array->reserve(hash->size());
   284	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   285	            keys_array->push(Value(it->first));
   286	        }
   287	    } else if (src.is_array()) {
   288	        size_t sz = src.as_array()->size();
   289	        keys_array->reserve(sz);
   290	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   291	    } else if (src.is_string()) {
   292	        size_t sz = src.as_string()->size();
   293	        keys_array->reserve(sz);
   294	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   295	    }
   296	    
   297	    regs[dst] = Value(keys_array);
   298	    return ip;
   299	}
   300	
   301	[[gnu::always_inline]] static const uint8_t* impl_GET_VALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   302	    uint16_t dst = read_u16(ip);
   303	    uint16_t src_reg = read_u16(ip);
   304	    Value& src = regs[src_reg];
   305	    
   306	    auto vals_array = state->heap.new_array();
   307	
   308	    if (src.is_hash_table()) {
   309	        hash_table_t hash = src.as_hash_table();
   310	        vals_array->reserve(hash->size());
   311	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   312	            vals_array->push(it->second);
   313	        }
   314	    } else if (src.is_array()) {
   315	        array_t arr = src.as_array();
   316	        vals_array->reserve(arr->size());
   317	        for (size_t i = 0; i < arr->size(); ++i) vals_array->push(arr->get(i));
   318	    } else if (src.is_string()) {
   319	        string_t str = src.as_string();
   320	        vals_array->reserve(str->size());
   321	        for (size_t i = 0; i < str->size(); ++i) {
   322	            char c = str->get(i);
   323	            vals_array->push(Value(state->heap.new_string(&c, 1)));
   324	        }
   325	    }
   326	
   327	    regs[dst] = Value(vals_array);
   328	    return ip;
   329	}
   330	
   331	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/exception_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	
     5	namespace meow::handlers {
     6	
     7	[[gnu::always_inline]] static const uint8_t* impl_THROW(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
     8	    uint16_t reg = read_u16(ip);
     9	    (void)constants;
    10	    Value& val = regs[reg];
    11	    state->error(to_string(val));
    12	    return impl_PANIC(ip, regs, constants, state);
    13	}
    14	
    15	[[gnu::always_inline]] static const uint8_t* impl_SETUP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    16	    uint16_t offset = read_u16(ip);
    17	    uint16_t err_reg = read_u16(ip);
    18	    (void)regs; (void)constants;
    19	    
    20	    size_t frame_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
    21	    size_t stack_depth = state->ctx.stack_top_ - state->ctx.stack_;
    22	    
    23	    size_t catch_ip_abs = offset; 
    24	    
    25	    state->ctx.exception_handlers_.emplace_back(catch_ip_abs, frame_depth, stack_depth, err_reg);
    26	    return ip;
    27	}
    28	
    29	[[gnu::always_inline]] static const uint8_t* impl_POP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    30	    (void)regs; (void)constants;
    31	    if (!state->ctx.exception_handlers_.empty()) {
    32	        state->ctx.exception_handlers_.pop_back();
    33	    }
    34	    return ip;
    35	}
    36	
    37	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/flow_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include <meow/core/objects.h>
     4	#include <meow/machine.h>
     5	#include <cstring>
     6	
     7	namespace meow::handlers {
     8	
     9	    struct JumpCondArgs { 
    10	        uint16_t cond; 
    11	        uint16_t offset; 
    12	    } __attribute__((packed));
    13	
    14	    struct JumpCondArgsB { 
    15	        uint8_t cond; 
    16	        uint16_t offset; 
    17	    } __attribute__((packed));
    18	
    19	    struct CallIC {
    20	        void* check_tag;
    21	        void* destination;
    22	    } __attribute__((packed)); 
    23	
    24	    [[gnu::always_inline]]
    25	    inline static CallIC* get_call_ic(const uint8_t*& ip) {
    26	        auto* ic = reinterpret_cast<CallIC*>(const_cast<uint8_t*>(ip));
    27	        ip += sizeof(CallIC); 
    28	        return ic;
    29	    }
    30	
    31	    [[gnu::always_inline]]
    32	    inline static const uint8_t* push_call_frame(
    33	        VMState* state, 
    34	        function_t closure, 
    35	        int argc, 
    36	        Value* args_src,       // Nguồn arguments (đối với CALL thường)
    37	        Value* receiver,       // 'this' (Nếu có - dành cho INVOKE)
    38	        Value* ret_dest,       // Nơi lưu kết quả trả về
    39	        const uint8_t* ret_ip, // Địa chỉ return (Sau lệnh call)
    40	        const uint8_t* err_ip  // IP hiện tại để báo lỗi nếu Stack Overflow
    41	    ) {
    42	        proto_t proto = closure->get_proto();
    43	        size_t num_params = proto->get_num_registers();
    44	
    45	        // 1. Check Stack Overflow
    46	        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
    47	            state->ctx.current_frame_->ip_ = err_ip;
    48	            state->error("Stack Overflow!");
    49	            return nullptr; // Báo hiệu lỗi
    50	        }
    51	
    52	        Value* new_base = state->ctx.stack_top_;
    53	        size_t arg_offset = 0;
    54	
    55	        // 2. Setup 'this' (Nếu là method call)
    56	        if (receiver != nullptr && num_params > 0) {
    57	            new_base[0] = *receiver;
    58	            arg_offset = 1; // Các arg sau sẽ lùi lại 1 slot
    59	        }
    60	
    61	        // 3. Copy Arguments
    62	        // INVOKE: args_src trỏ đến arg đầu tiên, receiver đã xử lý riêng
    63	        // CALL: args_src trỏ đến arg đầu tiên, không có receiver
    64	        size_t copy_count = (argc < (num_params - arg_offset)) ? argc : (num_params - arg_offset);
    65	        
    66	        for (size_t i = 0; i < copy_count; ++i) {
    67	            new_base[arg_offset + i] = args_src[i];
    68	        }
    69	
    70	        // 4. Fill Null (Nếu thiếu args)
    71	        size_t filled = arg_offset + argc;
    72	        for (size_t i = filled; i < num_params; ++i) {
    73	            new_base[i] = Value(null_t{});
    74	        }
    75	
    76	        // 5. Push Frame
    77	        state->ctx.frame_ptr_++;
    78	        *state->ctx.frame_ptr_ = CallFrame(
    79	            closure,
    80	            new_base,
    81	            ret_dest, 
    82	            ret_ip 
    83	        );
    84	        
    85	        // 6. Update Pointers
    86	        state->ctx.current_regs_ = new_base;
    87	        state->ctx.stack_top_ += num_params;
    88	        state->ctx.current_frame_ = state->ctx.frame_ptr_;
    89	        state->update_pointers(); 
    90	
    91	        return state->instruction_base; // Nhảy đến đầu hàm callee
    92	    }
    93	
    94	    [[gnu::always_inline]]
    95	    inline static const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    96	        (void)ip; (void)regs; (void)constants;
    97	        
    98	        if (!state->ctx.exception_handlers_.empty()) {
    99	            auto& handler = state->ctx.exception_handlers_.back();
   100	            long current_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
   101	            
   102	            while (current_depth > (long)handler.frame_depth_) {
   103	                size_t reg_idx = state->ctx.frame_ptr_->regs_base_ - state->ctx.stack_;
   104	                meow::close_upvalues(state->ctx, reg_idx);
   105	                state->ctx.frame_ptr_--;
   106	                current_depth--;
   107	            }
   108	            
   109	            state->ctx.stack_top_ = state->ctx.stack_ + handler.stack_depth_;
   110	            state->ctx.current_regs_ = state->ctx.frame_ptr_->regs_base_;
   111	            state->ctx.current_frame_ = state->ctx.frame_ptr_; 
   112	            state->update_pointers();
   113	
   114	            const uint8_t* catch_ip = state->instruction_base + handler.catch_ip_;
   115	            
   116	            if (handler.error_reg_ != static_cast<size_t>(-1)) {
   117	                auto err_str = state->heap.new_string(state->get_error_message());
   118	                regs[handler.error_reg_] = Value(err_str);
   119	            }
   120	            
   121	            state->clear_error();
   122	            state->ctx.exception_handlers_.pop_back();
   123	            return catch_ip;
   124	        } 
   125	        
   126	        std::println("VM Panic: {}", state->get_error_message());
   127	        return nullptr; 
   128	    }
   129	
   130	    [[gnu::always_inline]]
   131	    inline static const uint8_t* impl_UNIMPL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   132	        state->error("Opcode chưa được hỗ trợ (UNIMPL)");
   133	        return impl_PANIC(ip, regs, constants, state);
   134	    }
   135	
   136	    [[gnu::always_inline]]
   137	    inline static const uint8_t* impl_HALT(const uint8_t*, Value*, const Value*, VMState*) {
   138	        return nullptr;
   139	    }
   140	
   141	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   142	        uint16_t offset = *reinterpret_cast<const uint16_t*>(ip);
   143	        return state->instruction_base + offset;
   144	    }
   145	
   146	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   147	        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
   148	        Value& cond = regs[args.cond];
   149	
   150	        bool truthy;
   151	        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
   152	        else if (cond.is_int()) truthy = (cond.as_int() != 0);
   153	        else [[unlikely]] truthy = meow::to_bool(cond);
   154	
   155	        if (truthy) return state->instruction_base + args.offset;
   156	        return ip + sizeof(JumpCondArgs);
   157	    }
   158	
   159	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   160	        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
   161	        Value& cond = regs[args.cond];
   162	
   163	        bool truthy;
   164	        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
   165	        else if (cond.is_int()) truthy = (cond.as_int() != 0);
   166	        else [[unlikely]] truthy = meow::to_bool(cond);
   167	
   168	        if (!truthy) return state->instruction_base + args.offset;
   169	        return ip + sizeof(JumpCondArgs);
   170	    }
   171	
   172	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   173	        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
   174	        Value& cond = regs[args.cond];
   175	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   176	        if (truthy) return state->instruction_base + args.offset;
   177	        return ip + sizeof(JumpCondArgsB);
   178	    }
   179	
   180	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   181	        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
   182	        Value& cond = regs[args.cond];
   183	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   184	        if (!truthy) return state->instruction_base + args.offset;
   185	        return ip + sizeof(JumpCondArgsB);
   186	    }
   187	
   188	    [[gnu::always_inline]]
   189	    inline static const uint8_t* impl_RETURN(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   190	        uint16_t ret_reg_idx = read_u16(ip);
   191	        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : regs[ret_reg_idx];
   192	
   193	        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
   194	        meow::close_upvalues(state->ctx, base_idx);
   195	
   196	        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) [[unlikely]] {
   197	            return nullptr; 
   198	        }
   199	
   200	        CallFrame* popped_frame = state->ctx.frame_ptr_;
   201	        
   202	        if (state->current_module) [[likely]] {
   203	             if (popped_frame->function_->get_proto() == state->current_module->get_main_proto()) [[unlikely]] {
   204	                 state->current_module->set_executed();
   205	             }
   206	        }
   207	
   208	        state->ctx.frame_ptr_--;
   209	        CallFrame* caller = state->ctx.frame_ptr_;
   210	        
   211	        state->ctx.stack_top_ = popped_frame->regs_base_;
   212	        state->ctx.current_regs_ = caller->regs_base_;
   213	        state->ctx.current_frame_ = caller; 
   214	        
   215	        state->update_pointers(); 
   216	
   217	        if (popped_frame->ret_dest_ != nullptr) {
   218	            *popped_frame->ret_dest_ = result;
   219	        }
   220	
   221	        return popped_frame->ip_; 
   222	    }
   223	
   224	    template <bool IsVoid>
   225	    [[gnu::always_inline]] 
   226	    static inline const uint8_t* do_call(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   227	        const uint8_t* start_ip = ip - 1; 
   228	
   229	        uint16_t dst = 0;
   230	        if constexpr (!IsVoid) dst = read_u16(ip);
   231	        uint16_t fn_reg    = read_u16(ip);
   232	        uint16_t arg_start = read_u16(ip);
   233	        uint16_t argc      = read_u16(ip);
   234	
   235	        CallIC* ic = get_call_ic(ip);
   236	        Value& callee = regs[fn_reg];
   237	
   238	        if (callee.is_function()) [[likely]] {
   239	            function_t closure = callee.as_function();
   240	            proto_t proto = closure->get_proto();
   241	
   242	            if (ic->check_tag == proto) [[likely]] {
   243	                size_t num_params = proto->get_num_registers();
   244	                
   245	                if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
   246	                    state->ctx.current_frame_->ip_ = start_ip;
   247	                    state->error("Stack Overflow!");
   248	                    return impl_PANIC(ip, regs, constants, state);
   249	                }
   250	
   251	                Value* new_base = state->ctx.stack_top_;
   252	                
   253	                size_t copy_count = (argc < num_params) ? argc : num_params;
   254	                for (size_t i = 0; i < copy_count; ++i) {
   255	                    new_base[i] = regs[arg_start + i];
   256	                }
   257	
   258	                for (size_t i = copy_count; i < num_params; ++i) {
   259	                    new_base[i] = Value(null_t{});
   260	                }
   261	
   262	                state->ctx.frame_ptr_++;
   263	                *state->ctx.frame_ptr_ = CallFrame(
   264	                    closure,
   265	                    new_base,
   266	                    IsVoid ? nullptr : &regs[dst], 
   267	                    ip 
   268	                );
   269	                
   270	                state->ctx.current_regs_ = new_base;
   271	                state->ctx.stack_top_ += num_params;
   272	                state->ctx.current_frame_ = state->ctx.frame_ptr_;
   273	                state->update_pointers(); 
   274	
   275	                return state->instruction_base;
   276	            }
   277	            
   278	            ic->check_tag = proto;
   279	        } 
   280	        
   281	        if (callee.is_native()) {
   282	            native_t fn = callee.as_native();
   283	            if (ic->check_tag != (void*)fn) {
   284	                ic->check_tag = (void*)fn;
   285	            }
   286	
   287	            Value result = fn(&state->machine, argc, &regs[arg_start]);
   288	            
   289	            if (state->machine.has_error()) [[unlikely]] {
   290	                state->error(std::string(state->machine.get_error_message()));
   291	                state->machine.clear_error();
   292	                return impl_PANIC(ip, regs, constants, state);
   293	            }
   294	
   295	            if constexpr (!IsVoid) {
   296	                if (dst != 0xFFFF) regs[dst] = result;
   297	            }
   298	            return ip;
   299	        }
   300	
   301	        Value* ret_dest_ptr = nullptr;
   302	        if constexpr (!IsVoid) {
   303	            if (dst != 0xFFFF) ret_dest_ptr = &regs[dst];
   304	        }
   305	
   306	        instance_t self = nullptr;
   307	        function_t closure = nullptr;
   308	        bool is_init = false;
   309	
   310	        if (callee.is_function()) {
   311	            closure = callee.as_function();
   312	        } 
   313	        else if (callee.is_bound_method()) {
   314	            bound_method_t bound = callee.as_bound_method();
   315	            Value receiver = bound->get_receiver();
   316	            Value method = bound->get_method();
   317	
   318	            if (method.is_native()) {
   319	                native_t fn = method.as_native();
   320	                
   321	                std::vector<Value> args;
   322	                args.reserve(argc + 1);
   323	                args.push_back(receiver);
   324	                
   325	                for (size_t i = 0; i < argc; ++i) {
   326	                    args.push_back(regs[arg_start + i]);
   327	                }
   328	
   329	                Value result = fn(&state->machine, static_cast<int>(args.size()), args.data());
   330	                
   331	                if (state->machine.has_error()) {
   332	                     return impl_PANIC(ip, regs, constants, state);
   333	                }
   334	
   335	                if constexpr (!IsVoid) regs[dst] = result;
   336	                return ip;
   337	            }
   338	            else if (method.is_function()) {
   339	                closure = method.as_function();
   340	                if (receiver.is_instance()) self = receiver.as_instance();
   341	            }
   342	        }
   343	        else if (callee.is_class()) {
   344	            class_t klass = callee.as_class();
   345	            self = state->heap.new_instance(klass, state->heap.get_empty_shape());
   346	            if (ret_dest_ptr) *ret_dest_ptr = Value(self);
   347	            
   348	            Value init_method = klass->get_method(state->heap.new_string("init"));
   349	            if (init_method.is_function()) {
   350	                closure = init_method.as_function();
   351	                is_init = true;
   352	            } else {
   353	                return ip; 
   354	            }
   355	        } 
   356	        else [[unlikely]] {
   357	            state->ctx.current_frame_->ip_ = start_ip;
   358	            state->error(std::format("Giá trị loại '{}' không thể gọi được (Not callable).", to_string(callee)));
   359	            return impl_PANIC(ip, regs, constants, state);
   360	        }
   361	
   362	        proto_t proto = closure->get_proto();
   363	        size_t num_params = proto->get_num_registers();
   364	
   365	        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
   366	            state->ctx.current_frame_->ip_ = start_ip;
   367	            state->error("Stack Overflow!");
   368	            return impl_PANIC(ip, regs, constants, state);
   369	        }
   370	
   371	        Value* new_base = state->ctx.stack_top_;
   372	        size_t arg_offset = 0;
   373	        
   374	        if (self != nullptr && num_params > 0) {
   375	            new_base[0] = Value(self);
   376	            arg_offset = 1;
   377	        }
   378	
   379	        for (size_t i = 0; i < argc; ++i) {
   380	            if (arg_offset + i < num_params) {
   381	                new_base[arg_offset + i] = regs[arg_start + i];
   382	            }
   383	        }
   384	
   385	        size_t filled_count = arg_offset + argc;
   386	        if (filled_count > num_params) filled_count = num_params;
   387	
   388	        for (size_t i = filled_count; i < num_params; ++i) {
   389	            new_base[i] = Value(null_t{});
   390	        }
   391	
   392	        state->ctx.frame_ptr_++;
   393	        *state->ctx.frame_ptr_ = CallFrame(
   394	            closure,
   395	            new_base,                          
   396	            is_init ? nullptr : ret_dest_ptr,  
   397	            ip                                 
   398	        );
   399	
   400	        state->ctx.current_regs_ = new_base;
   401	        state->ctx.stack_top_ += num_params;
   402	        state->ctx.current_frame_ = state->ctx.frame_ptr_;
   403	        state->update_pointers(); 
   404	
   405	        return state->instruction_base;
   406	    }
   407	
   408	    [[gnu::always_inline]] inline static const uint8_t* impl_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   409	        return do_call<false>(ip, regs, constants, state);
   410	    }
   411	
   412	    [[gnu::always_inline]] inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   413	        return do_call<true>(ip, regs, constants, state);
   414	    }
   415	
   416	[[gnu::always_inline]] 
   417	static const uint8_t* impl_TAIL_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   418	    const uint8_t* start_ip = ip - 1;
   419	
   420	    uint16_t dst = read_u16(ip); (void)dst;
   421	    uint16_t fn_reg = read_u16(ip);
   422	    uint16_t arg_start = read_u16(ip);
   423	    uint16_t argc = read_u16(ip);
   424	    
   425	    ip += 16;
   426	
   427	    Value& callee = regs[fn_reg];
   428	    if (!callee.is_function()) [[unlikely]] {
   429	        state->ctx.current_frame_->ip_ = start_ip;
   430	        state->error("TAIL_CALL: Target không phải là Function.");
   431	        return nullptr;
   432	    }
   433	
   434	    function_t closure = callee.as_function();
   435	    proto_t proto = closure->get_proto();
   436	    size_t num_params = proto->get_num_registers();
   437	
   438	    size_t current_base_idx = regs - state->ctx.stack_;
   439	    meow::close_upvalues(state->ctx, current_base_idx);
   440	
   441	    size_t copy_count = (argc < num_params) ? argc : num_params;
   442	
   443	    for (size_t i = 0; i < copy_count; ++i) {
   444	        regs[i] = regs[arg_start + i];
   445	    }
   446	
   447	    for (size_t i = copy_count; i < num_params; ++i) {
   448	        regs[i] = Value(null_t{});
   449	    }
   450	
   451	    CallFrame* current_frame = state->ctx.frame_ptr_;
   452	    current_frame->function_ = closure;
   453	    
   454	    state->ctx.stack_top_ = regs + num_params;
   455	    state->update_pointers();
   456	
   457	    return proto->get_chunk().get_code();
   458	}
   459	
   460	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/math_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	
     5	namespace meow::handlers {
     6	
     7	struct BinaryArgs { uint16_t dst; uint16_t r1; uint16_t r2; } __attribute__((packed));
     8	struct BinaryArgsB { uint8_t dst; uint8_t r1; uint8_t r2; } __attribute__((packed));
     9	struct UnaryArgs { uint16_t dst; uint16_t src; };
    10	
    11	// --- MACROS ---
    12	#define BINARY_OP_IMPL(NAME, OP_ENUM) \
    13	    HOT_HANDLER impl_##NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    14	        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
    15	        Value& left  = regs[args.r1]; \
    16	        Value& right = regs[args.r2]; \
    17	        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    18	        return ip + sizeof(BinaryArgs); \
    19	    }
    20	
    21	#define BINARY_OP_B_IMPL(NAME, OP_ENUM) \
    22	    HOT_HANDLER impl_##NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    23	        const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip); \
    24	        Value& left  = regs[args.r1]; \
    25	        Value& right = regs[args.r2]; \
    26	        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    27	        return ip + sizeof(BinaryArgsB); \
    28	    }
    29	
    30	HOT_HANDLER impl_ADD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    31	    const auto& args = *reinterpret_cast<const BinaryArgs*>(ip);
    32	    Value& left = regs[args.r1];
    33	    Value& right = regs[args.r2];
    34	    if (left.holds_both<int_t>(right)) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    35	    else if (left.holds_both<float_t>(right)) regs[args.dst] = Value(left.as_float() + right.as_float());
    36	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    37	    return ip + sizeof(BinaryArgs);
    38	}
    39	
    40	HOT_HANDLER impl_ADD_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    41	    const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip);
    42	    Value& left = regs[args.r1];
    43	    Value& right = regs[args.r2];
    44	    if (left.holds_both<int_t>(right)) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    45	    else if (left.holds_both<float_t>(right)) regs[args.dst] = Value(left.as_float() + right.as_float());
    46	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    47	    return ip + sizeof(BinaryArgsB);
    48	}
    49	
    50	BINARY_OP_IMPL(SUB, SUB)
    51	BINARY_OP_IMPL(MUL, MUL)
    52	BINARY_OP_IMPL(DIV, DIV)
    53	BINARY_OP_IMPL(MOD, MOD)
    54	BINARY_OP_IMPL(POW, POW)
    55	
    56	BINARY_OP_B_IMPL(SUB, SUB)
    57	BINARY_OP_B_IMPL(MUL, MUL)
    58	BINARY_OP_B_IMPL(DIV, DIV)
    59	BINARY_OP_B_IMPL(MOD, MOD)
    60	
    61	#define CMP_FAST_IMPL(OP_NAME, OP_ENUM, OPERATOR) \
    62	    HOT_HANDLER impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    63	        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
    64	        Value& left = regs[args.r1]; \
    65	        Value& right = regs[args.r2]; \
    66	        if (left.holds_both<int_t>(right)) [[likely]] { \
    67	            regs[args.dst] = Value(left.as_int() OPERATOR right.as_int()); \
    68	        } else [[unlikely]] { \
    69	            regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    70	        } \
    71	        return ip + sizeof(BinaryArgs); \
    72	    } \
    73	    HOT_HANDLER impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    74	        const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip); \
    75	        Value& left = regs[args.r1]; \
    76	        Value& right = regs[args.r2]; \
    77	        if (left.holds_both<int_t>(right)) [[likely]] { \
    78	            regs[args.dst] = Value(left.as_int() OPERATOR right.as_int()); \
    79	        } else [[unlikely]] { \
    80	            regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    81	        } \
    82	        return ip + sizeof(BinaryArgsB); \
    83	    }
    84	
    85	CMP_FAST_IMPL(EQ, EQ, ==)
    86	CMP_FAST_IMPL(NEQ, NEQ, !=)
    87	CMP_FAST_IMPL(GT, GT, >)
    88	CMP_FAST_IMPL(GE, GE, >=)
    89	CMP_FAST_IMPL(LT, LT, <)
    90	CMP_FAST_IMPL(LE, LE, <=)
    91	BINARY_OP_IMPL(BIT_AND, BIT_AND)
    92	BINARY_OP_IMPL(BIT_OR, BIT_OR)
    93	BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
    94	BINARY_OP_IMPL(LSHIFT, LSHIFT)
    95	BINARY_OP_IMPL(RSHIFT, RSHIFT)
    96	
    97	HOT_HANDLER impl_NEG(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    98	    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    99	    Value& val = regs[args.src];
   100	    if (val.is_int()) [[likely]] regs[args.dst] = Value(-val.as_int());
   101	    else if (val.is_float()) regs[args.dst] = Value(-val.as_float());
   102	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
   103	    return ip + sizeof(UnaryArgs);
   104	}
   105	
   106	HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   107	    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
   108	    Value& val = regs[args.src];
   109	
   110	    if (val.is_int()) [[likely]] {
   111	        regs[args.dst] = Value(~val.as_int());
   112	    } 
   113	    else [[unlikely]] {
   114	        regs[args.dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
   115	    }
   116	
   117	    return ip + sizeof(UnaryArgs);
   118	}
   119	
   120	HOT_HANDLER impl_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   121	    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
   122	    Value& val = regs[args.src];
   123	
   124	    if (val.is_bool()) [[likely]] {
   125	        regs[args.dst] = Value(!val.as_bool());
   126	    } 
   127	    else if (val.is_int()) {
   128	        regs[args.dst] = Value(val.as_int() == 0); 
   129	    }
   130	    else if (val.is_null()) {
   131	        regs[args.dst] = Value(true);
   132	    }
   133	    else [[unlikely]] {
   134	        regs[args.dst] = Value(!to_bool(val)); 
   135	    }
   136	
   137	    return ip + sizeof(UnaryArgs);
   138	}
   139	
   140	[[gnu::always_inline]] static const uint8_t* impl_INC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   141	    uint16_t reg_idx = read_u16(ip);
   142	    Value& val = regs[reg_idx];
   143	
   144	    if (val.is_int()) [[likely]] {
   145	        val = Value(val.as_int() + 1);
   146	    } 
   147	    else if (val.is_float()) {
   148	        val = Value(val.as_float() + 1.0);
   149	    }
   150	    else [[unlikely]] {
   151	        state->error("INC: Toán hạng phải là số (Int/Real).");
   152	        return impl_PANIC(ip, regs, constants, state);
   153	    }
   154	    return ip;
   155	}
   156	
   157	[[gnu::always_inline]] static const uint8_t* impl_DEC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   158	    uint16_t reg_idx = read_u16(ip);
   159	    Value& val = regs[reg_idx];
   160	
   161	    if (val.is_int()) [[likely]] {
   162	        val = Value(val.as_int() - 1);
   163	    } 
   164	    else if (val.is_float()) {
   165	        val = Value(val.as_float() - 1.0);
   166	    } 
   167	    else [[unlikely]] {
   168	        state->error("DEC: Toán hạng phải là số (Int/Real).");
   169	        return impl_PANIC(ip, regs, constants, state);
   170	    }
   171	    return ip;
   172	}
   173	
   174	#undef BINARY_OP_IMPL
   175	#undef BINARY_OP_B_IMPL
   176	#undef CMP_FAST_IMPL
   177	
   178	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/memory_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	#include <meow/memory/gc_disable_guard.h>
     5	
     6	namespace meow::handlers {
     7	
     8	[[gnu::always_inline]] static const uint8_t* impl_GET_GLOBAL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
     9	    uint16_t dst = read_u16(ip);
    10	    uint16_t global_idx = read_u16(ip);
    11	        
    12	    regs[dst] = state->current_module->get_global_by_index(global_idx);
    13	    
    14	    return ip;
    15	}
    16	
    17	[[gnu::always_inline]] static const uint8_t* impl_SET_GLOBAL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    18	    uint16_t global_idx = read_u16(ip);
    19	    uint16_t src = read_u16(ip);
    20	    Value val = regs[src];
    21	        
    22	    state->current_module->set_global_by_index(global_idx, val);
    23	    
    24	    state->heap.write_barrier(state->current_module, val);
    25	    
    26	    return ip;
    27	}
    28	
    29	[[gnu::always_inline]] static const uint8_t* impl_GET_UPVALUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    30	    uint16_t dst = read_u16(ip);
    31	    uint16_t uv_idx = read_u16(ip);
    32	    (void)constants;
    33	    
    34	    upvalue_t uv = state->ctx.frame_ptr_->function_->get_upvalue(uv_idx);
    35	    if (uv->is_closed()) {
    36	        regs[dst] = uv->get_value();
    37	    } else {
    38	        regs[dst] = state->ctx.stack_[uv->get_index()];
    39	    }
    40	    return ip;
    41	}
    42	
    43	[[gnu::always_inline]] static const uint8_t* impl_SET_UPVALUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    44	    uint16_t uv_idx = read_u16(ip);
    45	    uint16_t src = read_u16(ip);
    46	    Value val = regs[src];
    47	
    48	    upvalue_t uv = state->ctx.frame_ptr_->function_->get_upvalue(uv_idx);
    49	    if (uv->is_closed()) {
    50	        uv->close(val);
    51	        state->heap.write_barrier(uv, val);
    52	    } else {
    53	        state->ctx.stack_[uv->get_index()] = val;
    54	    }
    55	    return ip;
    56	}
    57	
    58	[[gnu::always_inline]] static const uint8_t* impl_CLOSURE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    59	    uint16_t dst = read_u16(ip);
    60	    uint16_t proto_idx = read_u16(ip);
    61	    
    62	    Value val = constants[proto_idx];
    63	    if (!val.is_proto()) [[unlikely]] {
    64	        state->ctx.current_frame_->ip_ = ip - 5;
    65	        state->error("CLOSURE: Constant index " + std::to_string(proto_idx) + " is not a Proto");
    66	        return impl_PANIC(ip, regs, constants, state);
    67	    }
    68	    proto_t proto = val.as_proto();
    69	    function_t closure = state->heap.new_function(proto);
    70	    
    71	    regs[dst] = Value(closure); 
    72	
    73	    size_t current_base_idx = regs - state->ctx.stack_;
    74	
    75	    for (size_t i = 0; i < proto->get_num_upvalues(); ++i) {
    76	        const auto& desc = proto->get_desc(i);
    77	        if (desc.is_local_) {
    78	            closure->set_upvalue(i, capture_upvalue(&state->ctx, &state->heap, current_base_idx + desc.index_));
    79	        } else {
    80	            closure->set_upvalue(i, state->ctx.frame_ptr_->function_->get_upvalue(desc.index_));
    81	        }
    82	    }
    83	
    84	    return ip;
    85	}
    86	
    87	[[gnu::always_inline]] static const uint8_t* impl_CLOSE_UPVALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    88	    uint16_t last_reg = read_u16(ip);
    89	    (void)constants;
    90	    
    91	    size_t current_base_idx = regs - state->ctx.stack_;
    92	    close_upvalues(&state->ctx, current_base_idx + last_reg);
    93	    return ip;
    94	}
    95	
    96	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/module_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	#include "module/module_manager.h"
     5	
     6	namespace meow::handlers {
     7	
     8	[[gnu::always_inline]] 
     9	static const uint8_t* impl_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    10	    uint16_t name_idx = read_u16(ip);
    11	    uint16_t src_reg = read_u16(ip);
    12	    Value val = regs[src_reg];
    13	    
    14	    string_t name = constants[name_idx].as_string();
    15	    state->current_module->set_export(name, val);
    16	    
    17	    state->heap.write_barrier(state->current_module, val);
    18	    
    19	    return ip;
    20	}
    21	
    22	[[gnu::always_inline]] 
    23	static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    24	    uint16_t dst = read_u16(ip);
    25	    uint16_t mod_reg = read_u16(ip);
    26	    uint16_t name_idx = read_u16(ip);
    27	    
    28	    Value& mod_val = regs[mod_reg];
    29	    string_t name = constants[name_idx].as_string();
    30	    
    31	    if (!mod_val.is_module()) [[unlikely]] {
    32	        state->error("GET_EXPORT: Toán hạng không phải là Module.");
    33	        return impl_PANIC(ip, regs, constants, state);
    34	    }
    35	    
    36	    module_t mod = mod_val.as_module();
    37	    if (!mod->has_export(name)) [[unlikely]] {
    38	        state->error("Module không export '" + std::string(name->c_str()) + "'.");
    39	        return impl_PANIC(ip, regs, constants, state);
    40	    }
    41	    
    42	    regs[dst] = mod->get_export(name);
    43	    return ip;
    44	}
    45	
    46	[[gnu::always_inline]] 
    47	static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    48	    uint16_t src_idx = read_u16(ip);
    49	    (void)constants;
    50	    
    51	    const Value& mod_val = regs[src_idx];
    52	    
    53	    if (auto src_mod = mod_val.as_if_module()) {
    54	        state->current_module->import_all_export(src_mod);
    55	    } else [[unlikely]] {
    56	        state->error("IMPORT_ALL: Register không chứa Module.");
    57	        return impl_PANIC(ip, regs, constants, state);
    58	    }
    59	    return ip;
    60	}
    61	
    62	[[gnu::always_inline]] 
    63	static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    64	    uint16_t dst = read_u16(ip);
    65	    uint16_t path_idx = read_u16(ip);
    66	    
    67	    string_t path = constants[path_idx].as_string();
    68	    string_t importer_path = state->current_module ? state->current_module->get_file_path() : nullptr;
    69	    
    70	    module_t mod = state->modules.load_module(path, importer_path);
    71	    regs[dst] = Value(mod);
    72	
    73	    if (mod->is_executed() || mod->is_executing()) {
    74	        return ip;
    75	    }
    76	    
    77	    if (!mod->is_has_main()) {
    78	        mod->set_executed();
    79	        return ip;
    80	    }
    81	
    82	    mod->set_execution();
    83	    
    84	    proto_t main_proto = mod->get_main_proto();
    85	    function_t main_closure = state->heap.new_function(main_proto);
    86	    
    87	    size_t num_regs = main_proto->get_num_registers();
    88	
    89	    if (!state->ctx.check_frame_overflow()) [[unlikely]] {
    90	        state->error("Call Stack Overflow (too many imports)!");
    91	        return impl_PANIC(ip, regs, constants, state);
    92	    }
    93	    if (!state->ctx.check_overflow(num_regs)) [[unlikely]] {
    94	        state->error("Register Stack Overflow at import!");
    95	        return impl_PANIC(ip, regs, constants, state);
    96	    }
    97	    
    98	    Value* new_base = state->ctx.stack_top_;
    99	    state->ctx.frame_ptr_++; 
   100	    *state->ctx.frame_ptr_ = CallFrame(
   101	        main_closure,
   102	        new_base,
   103	        nullptr,
   104	        ip
   105	    );
   106	    
   107	    state->ctx.current_regs_ = new_base;
   108	    state->ctx.stack_top_ += num_regs;
   109	    state->ctx.current_frame_ = state->ctx.frame_ptr_;
   110	    
   111	    state->update_pointers();
   112	    
   113	    return main_proto->get_chunk().get_code(); 
   114	}
   115	
   116	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/oop_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	#include <meow/core/shape.h>
     5	#include <meow/core/module.h>
     6	#include <module/module_manager.h>
     7	#include <iostream> 
     8	#include <format>
     9	
    10	namespace meow::handlers {
    11	
    12	static constexpr int IC_CAPACITY = 4;
    13	
    14	struct InlineCacheEntry {
    15	    const Shape* shape;
    16	    uint32_t offset;
    17	} __attribute__((packed));
    18	
    19	struct InlineCache {
    20	    InlineCacheEntry entries[IC_CAPACITY];
    21	} __attribute__((packed));
    22	
    23	[[gnu::always_inline]]
    24	inline static InlineCache* get_inline_cache(const uint8_t*& ip) {
    25	    auto* ic = reinterpret_cast<InlineCache*>(const_cast<uint8_t*>(ip));
    26	    ip += sizeof(InlineCache); 
    27	    return ic;
    28	}
    29	
    30	inline static void update_inline_cache(InlineCache* ic, const Shape* shape, uint32_t offset) {
    31	    for (int i = 0; i < IC_CAPACITY; ++i) {
    32	        if (ic->entries[i].shape == shape) {
    33	            if (i > 0) {
    34	                InlineCacheEntry temp = ic->entries[i];
    35	                std::memmove(&ic->entries[1], &ic->entries[0], i * sizeof(InlineCacheEntry));
    36	                ic->entries[0] = temp;
    37	                ic->entries[0].offset = offset;
    38	            }
    39	            return;
    40	        }
    41	    }
    42	    std::memmove(&ic->entries[1], &ic->entries[0], (IC_CAPACITY - 1) * sizeof(InlineCacheEntry));
    43	    ic->entries[0].shape = shape;
    44	    ic->entries[0].offset = offset;
    45	}
    46	
    47	static inline Value find_primitive_method(VMState* state, const Value& obj, string_t name) {
    48	    const char* mod_name = nullptr;
    49	    
    50	    if (obj.is_array()) mod_name = "array";
    51	    else if (obj.is_string()) mod_name = "string";
    52	    else if (obj.is_hash_table()) mod_name = "object";
    53	    
    54	    if (mod_name) {
    55	        module_t mod = state->modules.load_module(state->heap.new_string(mod_name), nullptr);
    56	        if (mod && mod->has_export(name)) {
    57	            return mod->get_export(name);
    58	        }
    59	    }
    60	    return Value(null_t{});
    61	}
    62	
    63	// --- HANDLERS ---
    64	
    65	[[gnu::always_inline]] 
    66	static const uint8_t* impl_INVOKE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    67	    static bool is_ran = false;
    68	    if (!is_ran) {
    69	        std::println("Đang dùng INVOKE OpCode (chỉ hiện log một lần)");
    70	        is_ran = true;
    71	    }
    72	    // 1. Tính toán địa chỉ return (Nhảy qua 80 bytes fat instruction)
    73	    // ip đang ở byte đầu tiên của tham số (sau opcode)
    74	    // Next Opcode = (ip - 1) + 80 = ip + 79
    75	    const uint8_t* next_ip = ip + 79;
    76	    const uint8_t* start_ip = ip - 1; // Để báo lỗi chính xác
    77	
    78	    // 2. Decode Arguments (10 bytes)
    79	    uint16_t dst = read_u16(ip);
    80	    uint16_t obj_reg = read_u16(ip);
    81	    uint16_t name_idx = read_u16(ip);
    82	    uint16_t arg_start = read_u16(ip);
    83	    uint16_t argc = read_u16(ip);
    84	    
    85	    // 3. Inline Cache (48 bytes)
    86	    InlineCache* ic = get_inline_cache(ip); 
    87	
    88	    Value& receiver = regs[obj_reg];
    89	    string_t name = constants[name_idx].as_string();
    90	
    91	    // 4. Instance Method Call (Fast Path)
    92	    if (receiver.is_instance()) [[likely]] {
    93	        instance_t inst = receiver.as_instance();
    94	        Shape* current_shape = inst->get_shape();
    95	
    96	        // --- OPTIMIZATION: Inline Cache Check ---
    97	        // Nếu shape của object trùng với shape trong cache, ta lấy luôn method offset/index
    98	        // (Lưu ý: Bạn cần mở rộng cấu trúc IC để lưu method ptr hoặc dùng cơ chế lookup nhanh)
    99	        
   100	        // Hiện tại ta vẫn lookup thủ công (nhưng vẫn nhanh hơn tạo BoundMethod):
   101	        class_t k = inst->get_class();
   102	        while (k) {
   103	            if (k->has_method(name)) {
   104	                Value method = k->get_method(name);
   105	                
   106	                // A. Gọi Meow Function (Optimized)
   107	                if (method.is_function()) {
   108	                    const uint8_t* jump_target = push_call_frame(
   109	                        state,
   110	                        method.as_function(),
   111	                        argc,
   112	                        &regs[arg_start], // Arguments source
   113	                        &receiver,        // Receiver ('this')
   114	                        (dst == 0xFFFF) ? nullptr : &regs[dst],
   115	                        next_ip,          // Return address (sau padding)
   116	                        start_ip          // Error address
   117	                    );
   118	                    
   119	                    if (jump_target == nullptr) return impl_PANIC(start_ip, regs, constants, state);
   120	                    return jump_target;
   121	                }
   122	                
   123	                // B. Gọi Native Function
   124	                else if (method.is_native()) {
   125	                    // Native cần mảng liên tục [this, arg1, arg2...]
   126	                    // Vì 'receiver' và 'args' không nằm liền nhau trên stack (regs),
   127	                    // ta phải tạo buffer tạm.
   128	                    
   129	                    // Small optimization: Dùng stack C++ (alloca) hoặc vector nhỏ
   130	                    std::vector<Value> native_args;
   131	                    native_args.reserve(argc + 1);
   132	                    native_args.push_back(receiver); // this
   133	                    for(int i=0; i<argc; ++i) native_args.push_back(regs[arg_start + i]);
   134	
   135	                    Value result = method.as_native()(&state->machine, native_args.size(), native_args.data());
   136	                    
   137	                    if (state->machine.has_error()) return impl_PANIC(start_ip, regs, constants, state);
   138	                    
   139	                    if (dst != 0xFFFF) regs[dst] = result;
   140	                    return next_ip;
   141	                }
   142	                break;
   143	            }
   144	            k = k->get_super();
   145	        }
   146	    }
   147	
   148	    // 5. Fallback (Slow Path)
   149	    // Xử lý trường hợp:
   150	    // - Receiver không phải Instance (vd: String, Array, Module...)
   151	    // - Method không tìm thấy trong Class (có thể là field chứa closure: obj.callback())
   152	    
   153	    // Logic: Tái sử dụng logic của GET_PROP để lấy value, sau đó CALL value đó.
   154	    
   155	    // a. Mô phỏng GET_PROP (lấy value vào regs[dst] tạm thời hoặc temp var)
   156	    // Lưu ý: impl_GET_PROP trong oop_ops.h đã có logic tìm field/method/bound_method.
   157	    // Nhưng ta không gọi impl_GET_PROP được vì nó thao tác IP và Stack khác.
   158	    
   159	    // Solution đơn giản: Gọi hàm helper find_property (bạn cần tách logic từ impl_GET_PROP ra)
   160	    // Hoặc copy logic find primitive method.
   161	    
   162	    // Ví dụ fallback đơn giản cho Primitive (String/Array method):
   163	    Value method_val = find_primitive_method(state, receiver, name); 
   164	    if (!method_val.is_null()) {
   165	         // Primitive method thường là Native, xử lý như case Native ở trên
   166	         // Cần tạo BoundMethod? Không, native call trực tiếp được nếu ta pass receiver.
   167	         // Nhưng find_primitive_method trả về NativeFunction thuần.
   168	         
   169	         std::vector<Value> native_args;
   170	         native_args.reserve(argc + 1);
   171	         native_args.push_back(receiver); 
   172	         for(int i=0; i<argc; ++i) native_args.push_back(regs[arg_start + i]);
   173	         
   174	         Value result;
   175	         if (method_val.is_native()) {
   176	             result = method_val.as_native()(&state->machine, native_args.size(), native_args.data());
   177	         } else {
   178	             // Trường hợp hiếm: Primitive trả về BoundMethod hoặc Closure
   179	             // ... xử lý tương tự ...
   180	             state->error("INVOKE: Primitive method type not supported yet.");
   181	             return impl_PANIC(start_ip, regs, constants, state);
   182	         }
   183	
   184	         if (state->machine.has_error()) return impl_PANIC(start_ip, regs, constants, state);
   185	         if (dst != 0xFFFF) regs[dst] = result;
   186	         return next_ip;
   187	    }
   188	
   189	    // Nếu vẫn không tìm thấy -> Lỗi
   190	    state->error(std::format("INVOKE: Method '{}' not found on object '{}'.", name->c_str(), to_string(receiver)));
   191	    return impl_PANIC(start_ip, regs, constants, state);
   192	}
   193	
   194	[[gnu::always_inline]] 
   195	static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   196	    // static bool is_ran = false;
   197	    // if (!is_ran) {
   198	    //     std::println("Đang dùng NEW_CLASS OpCode (chỉ hiện log một lần)");
   199	    //     is_ran = true;
   200	    // }
   201	    uint16_t dst = read_u16(ip);
   202	    uint16_t name_idx = read_u16(ip);
   203	    string_t name = constants[name_idx].as_string();
   204	    regs[dst] = Value(state->heap.new_class(name));
   205	    return ip;
   206	}
   207	
   208	[[gnu::always_inline]] 
   209	static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   210	    uint16_t dst = read_u16(ip);
   211	    uint16_t class_reg = read_u16(ip);
   212	    Value& class_val = regs[class_reg];
   213	    if (!class_val.is_class()) [[unlikely]] {
   214	        state->error("NEW_INSTANCE: Toán hạng không phải là Class.");
   215	        return impl_PANIC(ip, regs, constants, state);
   216	    }
   217	    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
   218	    return ip;
   219	}
   220	
   221	[[gnu::always_inline]] 
   222	static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   223	    const uint8_t* start_ip = ip - 1;
   224	
   225	    uint16_t dst = read_u16(ip);
   226	    uint16_t obj_reg = read_u16(ip);
   227	    uint16_t name_idx = read_u16(ip);
   228	    
   229	    InlineCache* ic = get_inline_cache(ip); 
   230	    Value& obj = regs[obj_reg];
   231	    string_t name = constants[name_idx].as_string();
   232	
   233	    if (obj.is_null()) [[unlikely]] {
   234	        state->ctx.current_frame_->ip_ = start_ip;
   235	        state->error(std::format("Runtime Error: Cannot read property '{}' of null.", name->c_str()));
   236	        return impl_PANIC(ip, regs, constants, state);
   237	    }
   238	    
   239	    // 1. Instance (Class Object)
   240	    if (obj.is_instance()) [[likely]] {
   241	        instance_t inst = obj.as_instance();
   242	        Shape* current_shape = inst->get_shape();
   243	
   244	        for (int i = 0; i < IC_CAPACITY; ++i) {
   245	            if (ic->entries[i].shape == current_shape) {
   246	                regs[dst] = inst->get_field_at(ic->entries[i].offset);
   247	                return ip;
   248	            }
   249	        }
   250	
   251	        int offset = current_shape->get_offset(name);
   252	        if (offset != -1) {
   253	            update_inline_cache(ic, current_shape, static_cast<uint32_t>(offset));
   254	            regs[dst] = inst->get_field_at(offset);
   255	            return ip;
   256	        }
   257	
   258	        class_t k = inst->get_class();
   259	        while (k) {
   260	            if (k->has_method(name)) {
   261	                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
   262	                return ip;
   263	            }
   264	            k = k->get_super();
   265	        }
   266	    }
   267	    else if (obj.is_hash_table()) {
   268	        hash_table_t hash = obj.as_hash_table();
   269	        
   270	        if (hash->has(name)) {
   271	            regs[dst] = hash->get(name);
   272	            return ip;
   273	        }
   274	        
   275	        Value method = find_primitive_method(state, obj, name);
   276	        if (!method.is_null()) {
   277	            auto bound = state->heap.new_bound_method(obj, method); 
   278	            regs[dst] = Value(bound);
   279	            return ip;
   280	        }
   281	        
   282	        regs[dst] = Value(null_t{}); 
   283	        return ip;
   284	    }
   285	    // 3. Module
   286	    else if (obj.is_module()) {
   287	        module_t mod = obj.as_module();
   288	        if (mod->has_export(name)) {
   289	            regs[dst] = mod->get_export(name);
   290	            return ip;
   291	        }
   292	    }
   293	    // 4. Class (Static Method)
   294	    else if (obj.is_class()) {
   295	        class_t k = obj.as_class();
   296	        if (k->has_method(name)) {
   297	            regs[dst] = k->get_method(name); 
   298	            return ip;
   299	        }
   300	    }
   301	    else if (obj.is_array() && std::strcmp(name->c_str(), "length") == 0) {
   302	        regs[dst] = Value(static_cast<int64_t>(obj.as_array()->size()));
   303	        return ip;
   304	    }
   305	    else if (obj.is_string() && std::strcmp(name->c_str(), "length") == 0) {
   306	        regs[dst] = Value(static_cast<int64_t>(obj.as_string()->size()));
   307	        return ip;
   308	    }
   309	    // 5. Primitive khác (Array, String)
   310	    else {
   311	        Value method = find_primitive_method(state, obj, name);
   312	        if (!method.is_null()) {
   313	            auto bound = state->heap.new_bound_method(obj, method); 
   314	            regs[dst] = Value(bound);
   315	            return ip;
   316	        }
   317	    }
   318	    
   319	    // Not found
   320	    state->ctx.current_frame_->ip_ = start_ip;
   321	    state->error(std::format("Runtime Error: Property '{}' not found on type '{}'.", 
   322	        name->c_str(), to_string(obj)));
   323	    return impl_PANIC(ip, regs, constants, state);
   324	}
   325	
   326	[[gnu::always_inline]] 
   327	static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   328	    const uint8_t* start_ip = ip - 1;
   329	
   330	    uint16_t obj_reg = read_u16(ip);
   331	    uint16_t name_idx = read_u16(ip);
   332	    uint16_t val_reg = read_u16(ip);
   333	    
   334	    InlineCache* ic = get_inline_cache(ip);
   335	    Value& obj = regs[obj_reg];
   336	    Value& val = regs[val_reg];
   337	    string_t name = constants[name_idx].as_string();
   338	    
   339	    if (obj.is_instance()) [[likely]] {
   340	        instance_t inst = obj.as_instance();
   341	        Shape* current_shape = inst->get_shape();
   342	
   343	        for (int i = 0; i < IC_CAPACITY; ++i) {
   344	            if (ic->entries[i].shape == current_shape) {
   345	                inst->set_field_at(ic->entries[i].offset, val);
   346	                state->heap.write_barrier(inst, val);
   347	                return ip;
   348	            }
   349	        }
   350	
   351	        int offset = current_shape->get_offset(name);
   352	
   353	        if (offset != -1) {
   354	            update_inline_cache(ic, current_shape, static_cast<uint32_t>(offset));
   355	            inst->set_field_at(offset, val);
   356	            state->heap.write_barrier(inst, val);
   357	        } 
   358	        else {
   359	            Shape* next_shape = current_shape->get_transition(name);
   360	            if (next_shape == nullptr) {
   361	                next_shape = current_shape->add_transition(name, &state->heap);
   362	            }
   363	            
   364	            inst->set_shape(next_shape);
   365	            
   366	            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape))); 
   367	
   368	            inst->add_field(val);
   369	            state->heap.write_barrier(inst, val);
   370	        }
   371	    }
   372	    else if (obj.is_hash_table()) {
   373	        obj.as_hash_table()->set(name, val);
   374	        state->heap.write_barrier(obj.as_object(), val);
   375	    }
   376	    else {
   377	        state->ctx.current_frame_->ip_ = start_ip;
   378	        state->error(std::format("SET_PROP: Cannot set property '{}' on type '{}'.", name->c_str(), to_string(obj)));
   379	        return impl_PANIC(ip, regs, constants, state);
   380	    }
   381	    return ip;
   382	}
   383	
   384	[[gnu::always_inline]] 
   385	static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   386	    uint16_t class_reg = read_u16(ip);
   387	    uint16_t name_idx = read_u16(ip);
   388	    uint16_t method_reg = read_u16(ip);
   389	    
   390	    Value& class_val = regs[class_reg];
   391	    string_t name = constants[name_idx].as_string();
   392	    Value& method_val = regs[method_reg];
   393	    
   394	    if (!class_val.is_class()) [[unlikely]] {
   395	        state->error("SET_METHOD: Target must be a Class.");
   396	        return impl_PANIC(ip, regs, constants, state);
   397	    }
   398	    if (!method_val.is_function() && !method_val.is_native()) [[unlikely]] {
   399	        state->error("SET_METHOD: Value must be a Function or Native.");
   400	        return impl_PANIC(ip, regs, constants, state);
   401	    }
   402	    class_val.as_class()->set_method(name, method_val);
   403	    state->heap.write_barrier(class_val.as_class(), method_val);
   404	    return ip;
   405	}
   406	
   407	[[gnu::always_inline]] 
   408	static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   409	    uint16_t sub_reg = read_u16(ip);
   410	    uint16_t super_reg = read_u16(ip);
   411	    (void)constants;
   412	    
   413	    Value& sub_val = regs[sub_reg];
   414	    Value& super_val = regs[super_reg];
   415	    
   416	    if (!sub_val.is_class() || !super_val.is_class()) [[unlikely]] {
   417	        state->error("INHERIT: Both operands must be Classes.");
   418	        return impl_PANIC(ip, regs, constants, state);
   419	    }
   420	    
   421	    sub_val.as_class()->set_super(super_val.as_class());
   422	    return ip;
   423	}
   424	
   425	[[gnu::always_inline]] 
   426	static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   427	    const uint8_t* start_ip = ip - 1;
   428	
   429	    uint16_t dst = read_u16(ip);
   430	    uint16_t name_idx = read_u16(ip);
   431	    string_t name = constants[name_idx].as_string();
   432	    
   433	    Value& receiver_val = regs[0]; 
   434	    
   435	    if (!receiver_val.is_instance()) [[unlikely]] {
   436	        state->ctx.current_frame_->ip_ = start_ip;
   437	        state->error("GET_SUPER: 'super' is only valid inside an instance method.");
   438	        return impl_PANIC(ip, regs, constants, state);
   439	    }
   440	    
   441	    instance_t receiver = receiver_val.as_instance();
   442	    class_t klass = receiver->get_class();
   443	    class_t super = klass->get_super();
   444	    
   445	    if (!super) {
   446	        state->ctx.current_frame_->ip_ = start_ip;
   447	        state->error("GET_SUPER: Class has no superclass.");
   448	        return impl_PANIC(ip, regs, constants, state);
   449	    }
   450	    
   451	    class_t k = super;
   452	    while (k) {
   453	        if (k->has_method(name)) {
   454	            regs[dst] = Value(state->heap.new_bound_method(receiver, k->get_method(name).as_function()));
   455	            return ip;
   456	        }
   457	        k = k->get_super();
   458	    }
   459	    
   460	    state->ctx.current_frame_->ip_ = start_ip;
   461	    state->error(std::format("GET_SUPER: Method '{}' not found in superclass.", name->c_str()));
   462	    return impl_PANIC(ip, regs, constants, state);
   463	}
   464	
   465	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/utils.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "vm/vm_state.h"
     4	#include <meow/bytecode/op_codes.h>
     5	#include <meow/value.h>
     6	#include <meow/cast.h>
     7	#include "runtime/operator_dispatcher.h"
     8	#include "runtime/execution_context.h"
     9	#include "runtime/call_frame.h"
    10	#include <meow/core/function.h>
    11	#include <meow/memory/memory_manager.h>
    12	#include "runtime/upvalue.h"
    13	#include <cstring>
    14	
    15	#define HOT_HANDLER [[gnu::always_inline, gnu::hot, gnu::aligned(32)]] static const uint8_t*
    16	
    17	namespace meow {
    18	namespace handlers {
    19	
    20	[[gnu::always_inline]]
    21	inline uint16_t read_u16(const uint8_t*& ip) noexcept {
    22	    uint16_t val = *reinterpret_cast<const uint16_t*>(ip);
    23	    ip += 2;
    24	    return val;
    25	}
    26	
    27	} // namespace handlers
    28	} // namespace meow


// =============================================================================
//  FILE PATH: src/vm/interpreter.cpp
// =============================================================================

     1	#include "vm/interpreter.h"
     2	#include "vm/handlers/data_ops.h"
     3	#include "vm/handlers/math_ops.h"
     4	#include "vm/handlers/flow_ops.h"
     5	#include "vm/handlers/memory_ops.h"
     6	#include "vm/handlers/oop_ops.h"
     7	#include "vm/handlers/module_ops.h"
     8	#include "vm/handlers/exception_ops.h"
     9	
    10	namespace meow {
    11	
    12	namespace {
    13	    using OpHandler = void (*)(const uint8_t*, Value*, const Value*, VMState*);
    14	    using OpImpl    = const uint8_t* (*)(const uint8_t*, Value*, const Value*, VMState*);
    15	
    16	    static OpHandler dispatch_table[256];
    17	
    18	    [[gnu::always_inline, gnu::hot]]
    19	    static void dispatch(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    20	        uint8_t opcode = *ip++;
    21	        [[clang::musttail]] return dispatch_table[opcode](ip, regs, constants, state);
    22	    }
    23	
    24	    template <OpCode Op>
    25	    constexpr bool IsFrameChange = false;
    26	
    27	    template <> constexpr bool IsFrameChange<OpCode::CALL>          = true;
    28	    template <> constexpr bool IsFrameChange<OpCode::CALL_VOID>     = true;
    29	    template <> constexpr bool IsFrameChange<OpCode::TAIL_CALL>     = true;
    30	    template <> constexpr bool IsFrameChange<OpCode::RETURN>        = true;
    31	    template <> constexpr bool IsFrameChange<OpCode::IMPORT_MODULE> = true;
    32	    template <> constexpr bool IsFrameChange<OpCode::THROW>         = true; 
    33	    
    34	    template <OpCode Op, OpImpl ImplFn>
    35	    static void op_wrapper(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    36	        const uint8_t* next_ip = ImplFn(ip, regs, constants, state);
    37	        if (next_ip) [[likely]] {
    38	            if constexpr (IsFrameChange<Op>) {
    39	                regs = state->registers;
    40	                constants = state->constants;
    41	            }
    42	            [[clang::musttail]] return dispatch(next_ip, regs, constants, state);
    43	        }
    44	    }
    45	
    46	    struct TableInitializer {
    47	        TableInitializer() {
    48	            for (int i = 0; i < 256; ++i) {
    49	                dispatch_table[i] = op_wrapper<OpCode::HALT, handlers::impl_UNIMPL>;
    50	            }
    51	
    52	            #define reg(NAME) dispatch_table[static_cast<size_t>(OpCode::NAME)] = op_wrapper<OpCode::NAME, handlers::impl_##NAME>
    53	            
    54	            // Load / Move
    55	            reg(LOAD_CONST); reg(LOAD_NULL); reg(LOAD_TRUE); reg(LOAD_FALSE);
    56	            reg(LOAD_INT); reg(LOAD_FLOAT); reg(MOVE);
    57	
    58	            reg(INC); reg(DEC);
    59	
    60	            // Math
    61	            reg(ADD); reg(SUB); reg(MUL); reg(DIV); reg(MOD); reg(POW);
    62	            reg(EQ); reg(NEQ); reg(GT); reg(GE); reg(LT); reg(LE);
    63	            reg(NEG); reg(NOT);
    64	            reg(BIT_AND); reg(BIT_OR); reg(BIT_XOR); reg(BIT_NOT);
    65	            reg(LSHIFT); reg(RSHIFT);
    66	
    67	            // Variables / Memory
    68	            reg(GET_GLOBAL); reg(SET_GLOBAL);
    69	            reg(GET_UPVALUE); reg(SET_UPVALUE);
    70	            reg(CLOSURE); reg(CLOSE_UPVALUES);
    71	
    72	            // Control Flow
    73	            reg(JUMP); reg(JUMP_IF_FALSE); reg(JUMP_IF_TRUE);
    74	            reg(CALL); reg(CALL_VOID); reg(RETURN); reg(HALT);
    75	
    76	            // Data Structures
    77	            reg(NEW_ARRAY); reg(NEW_HASH);
    78	            reg(GET_INDEX); reg(SET_INDEX);
    79	            reg(GET_KEYS); reg(GET_VALUES);
    80	
    81	            // OOP
    82	            reg(NEW_CLASS); reg(NEW_INSTANCE);
    83	            reg(GET_PROP); reg(SET_PROP); reg(SET_METHOD);
    84	            reg(INHERIT); reg(GET_SUPER);
    85	
    86	            reg(INVOKE);
    87	
    88	            // Exception
    89	            reg(THROW); reg(SETUP_TRY); reg(POP_TRY);
    90	
    91	            // Modules
    92	            reg(IMPORT_MODULE); reg(EXPORT); reg(GET_EXPORT); reg(IMPORT_ALL);
    93	
    94	            reg(TAIL_CALL);
    95	
    96	            reg(ADD_B); reg(SUB_B); reg(MUL_B); reg(DIV_B); reg(MOD_B);
    97	            reg(EQ_B); reg(NEQ_B); reg(GT_B); reg(GE_B); reg(LT_B); reg(LE_B);
    98	            
    99	            reg(JUMP_IF_TRUE_B); 
   100	            reg(JUMP_IF_FALSE_B);
   101	            
   102	            reg(JUMP_IF_TRUE_B); 
   103	            reg(JUMP_IF_FALSE_B);
   104	
   105	            #undef reg
   106	        }
   107	    };
   108	    
   109	    static TableInitializer init_trigger;
   110	
   111	} // namespace anonymous
   112	
   113	void Interpreter::run(VMState state) noexcept {
   114	    MemoryManager::set_current(&state.heap);
   115	    if (!state.ctx.current_frame_) return;
   116	    
   117	    state.update_pointers();
   118	    
   119	    Value* regs = state.registers;
   120	    const Value* constants = state.constants;
   121	    const uint8_t* ip = state.ctx.current_frame_->ip_;
   122	    
   123	    dispatch(ip, regs, constants, &state);
   124	}
   125	
   126	} // namespace meow


// =============================================================================
//  FILE PATH: src/vm/interpreter.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include "vm/vm_state.h"
     5	
     6	namespace meow {
     7	
     8	class Interpreter {
     9	public:
    10	    static void run(VMState state) noexcept;
    11	};
    12	
    13	}


// =============================================================================
//  FILE PATH: src/vm/lifecycle.cpp
// =============================================================================

     1	#include <meow/machine.h>
     2	#include "pch.h"
     3	#include "memory/generational_gc.h"
     4	#include <meow/memory/memory_manager.h>
     5	#include "module/module_manager.h"
     6	#include "runtime/execution_context.h"
     7	
     8	using namespace meow;
     9	
    10	Machine::Machine(const std::string& entry_point_directory, const std::string& entry_path, int argc, char* argv[]) {
    11	    args_.entry_point_directory_ = entry_point_directory;
    12	    args_.entry_path_ = entry_path;
    13	    for (int i = 0; i < argc; ++i) {
    14	        args_.command_line_arguments_.emplace_back(argv[i]);
    15	    }
    16	
    17	    context_ = std::make_unique<ExecutionContext>();
    18	    
    19	    auto gc = std::make_unique<GenerationalGC>(context_.get());
    20	    GenerationalGC* gc_ptr = gc.get(); 
    21	    heap_ = std::make_unique<MemoryManager>(std::move(gc));
    22	    mod_manager_ = std::make_unique<ModuleManager>(heap_.get(), this);
    23	    gc_ptr->set_module_manager(mod_manager_.get());
    24	    load_builtins();
    25	}
    26	
    27	Machine::~Machine() noexcept {}
    28	
    29	void Machine::interpret() noexcept {
    30	    if (prepare()) {
    31	        run();
    32	        if (has_error()) {
    33	            std::println(stderr, "VM Runtime Error: {}", get_error_message());
    34	        }
    35	    } else {
    36	        if (has_error()) {
    37	            std::println(stderr, "VM Init Error: {}", get_error_message());
    38	        }
    39	    }
    40	}
    41	
    42	bool Machine::prepare() noexcept {
    43	    std::filesystem::path full_path = std::filesystem::path(args_.entry_point_directory_) / args_.entry_path_;
    44	    
    45	    auto path_str = heap_->new_string(full_path.string());
    46	    auto importer_str = heap_->new_string(""); 
    47	
    48	    try {
    49	        module_t main_module = mod_manager_->load_module(path_str, importer_str);
    50	
    51	        if (!main_module) {
    52	            error("Could not load entry module (module is null).");
    53	            return false;
    54	        }
    55	
    56	        auto native_name = heap_->new_string("native");
    57	        module_t native_mod = mod_manager_->load_module(native_name, importer_str);
    58	        
    59	        if (native_mod) [[likely]] {
    60	            main_module->import_all_global(native_mod);
    61	        } else {
    62	            std::println("Warning: Could not inject 'native' module.");
    63	        }
    64	
    65	        proto_t main_proto = main_module->get_main_proto();
    66	        function_t main_func = heap_->new_function(main_proto);
    67	
    68	        context_->reset();
    69	
    70	        size_t num_regs = main_proto->get_num_registers();
    71	        if (!context_->check_overflow(num_regs)) [[unlikely]] {
    72	            error("Stack Overflow: Không đủ bộ nhớ khởi chạy main.");
    73	            return false;
    74	        }
    75	
    76	        *context_->frame_ptr_ = CallFrame(
    77	            main_func, 
    78	            context_->stack_,
    79	            nullptr,
    80	            main_proto->get_chunk().get_code()
    81	        );
    82	
    83	        context_->current_regs_ = context_->stack_;
    84	        context_->stack_top_ += num_regs; 
    85	        
    86	        context_->current_frame_ = context_->frame_ptr_;
    87	        
    88	        return true; 
    89	
    90	    } catch (const std::exception& e) {
    91	        error(std::format("Fatal error during preparation: {}", e.what()));
    92	        return false;
    93	    }
    94	}


// =============================================================================
//  FILE PATH: src/vm/machine.cpp
// =============================================================================

     1	#include <meow/machine.h>
     2	#include "vm/interpreter.h"
     3	#include "vm/vm_state.h"
     4	#include <meow/memory/memory_manager.h>
     5	
     6	namespace meow {
     7	
     8	Value Machine::call_callable(Value callable, const std::vector<Value>& args) noexcept {
     9	    if (callable.is_native()) {
    10	        return callable.as_native()(this, static_cast<int>(args.size()), const_cast<Value*>(args.data()));
    11	    }
    12	
    13	    function_t closure = nullptr;
    14	    instance_t self = nullptr;
    15	
    16	    if (callable.is_function()) {
    17	        closure = callable.as_function();
    18	    } else if (callable.is_bound_method()) {
    19	        auto bm = callable.as_bound_method();
    20	        Value receiver_val = bm->get_receiver();
    21	        self = receiver_val.is_instance() ? receiver_val.as_instance() : nullptr;
    22	        closure = bm->get_method().as_function();
    23	    } else if (callable.is_class()) {
    24	        class_t k = callable.as_class();
    25	        self = heap_->new_instance(k, heap_->get_empty_shape());
    26	        Value init = k->get_method(heap_->new_string("init"));
    27	        if (init.is_function()) {
    28	            closure = init.as_function();
    29	        } else {
    30	            return Value(self);
    31	        }
    32	    } else {
    33	        error("Runtime Error: Attempt to call a non-callable value.");
    34	        return Value(null_t{});
    35	    }
    36	
    37	    proto_t proto = closure->get_proto();
    38	    const size_t num_params = proto->get_num_registers();
    39	    const size_t argc = args.size();
    40	
    41	    if (!context_->check_overflow(num_params) || !context_->check_frame_overflow()) [[unlikely]] {
    42	        error("Stack Overflow: Cannot push arguments for native callback.");
    43	        return Value(null_t{});
    44	    }
    45	
    46	    Value* base = context_->stack_top_;
    47	    size_t arg_offset = 0;
    48	
    49	    if (self) {
    50	        base[0] = Value(self);
    51	        arg_offset = 1;
    52	    }
    53	
    54	    const size_t copy_count = std::min(argc, num_params - arg_offset);
    55	    for (size_t i = 0; i < copy_count; ++i) {
    56	        base[arg_offset + i] = args[i];
    57	    }
    58	
    59	    for (size_t i = arg_offset + copy_count; i < num_params; ++i) {
    60	        base[i] = Value(null_t{});
    61	    }
    62	
    63	    context_->frame_ptr_++;
    64	    Value return_val = Value(null_t{});
    65	
    66	    *context_->frame_ptr_ = CallFrame(
    67	        closure,
    68	        base,
    69	        &return_val,
    70	        proto->get_chunk().get_code()
    71	    );
    72	
    73	    context_->current_regs_ = base;
    74	    context_->stack_top_ += num_params;
    75	    context_->current_frame_ = context_->frame_ptr_;
    76	
    77	    VMState state {
    78	        *this, *context_, *heap_, *mod_manager_,
    79	        context_->current_regs_, nullptr, proto->get_chunk().get_code(),
    80	        nullptr, "", false
    81	    };
    82	    Interpreter::run(state);
    83	
    84	    if (state.has_error()) [[unlikely]] {
    85	        error(std::string(state.get_error_message()));
    86	        return Value(null_t{});
    87	    }
    88	
    89	    if (self && callable.is_class()) return Value(self);
    90	
    91	    return return_val;
    92	}
    93	
    94	void Machine::execute(function_t func) {
    95	    if (!func) return;
    96	
    97	    context_->reset();
    98	
    99	    proto_t proto = func->get_proto();
   100	    size_t num_regs = proto->get_num_registers();
   101	    
   102	    if (!context_->check_overflow(num_regs)) [[unlikely]] {
   103	        error("Stack Overflow on startup");
   104	        return;
   105	    }
   106	
   107	    *context_->frame_ptr_ = CallFrame(
   108	        func, 
   109	        context_->stack_, 
   110	        nullptr,          
   111	        proto->get_chunk().get_code()
   112	    );
   113	
   114	    context_->current_regs_ = context_->stack_;
   115	    context_->stack_top_ += num_regs; 
   116	    context_->current_frame_ = context_->frame_ptr_;
   117	
   118	    VMState state {
   119	        *this,           
   120	        *context_,       
   121	        *heap_,          
   122	        *mod_manager_,   
   123	        context_->current_regs_,
   124	        nullptr,                 
   125	        proto->get_chunk().get_code(),
   126	        nullptr,                 
   127	        "", false        
   128	    };
   129	    
   130	    Interpreter::run(state);
   131	    
   132	    if (state.has_error()) {
   133	        this->error(std::string(state.get_error_message()));
   134	    }
   135	}
   136	
   137	void Machine::run() noexcept {
   138	    const uint8_t* initial_code = context_->frame_ptr_->function_->get_proto()->get_chunk().get_code();
   139	    
   140	    VMState state {
   141	        *this,           
   142	        *context_,       
   143	        *heap_,          
   144	        *mod_manager_,   
   145	        context_->current_regs_,
   146	        nullptr,                 
   147	        initial_code,
   148	        nullptr,                 
   149	        "", false        
   150	    };
   151	    
   152	    if (context_->frame_ptr_->function_->get_proto()->get_module()) {
   153	        state.current_module = context_->frame_ptr_->function_->get_proto()->get_module();
   154	    }
   155	
   156	    Interpreter::run(state);
   157	    
   158	    if (state.has_error()) [[unlikely]] {
   159	        this->error(std::string(state.get_error_message()));
   160	    }
   161	}
   162	
   163	}



// =============================================================================
//  FILE PATH: src/vm/stdlib/array_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/core/module.h>
     7	#include <meow/core/array.h> 
     8	#include <meow/cast.h>
     9	#include <format> 
    10	#include <algorithm>
    11	
    12	namespace meow::natives::array {
    13	
    14	constexpr size_t MAX_ARRAY_CAPACITY = 64 * 1024 * 1024; 
    15	
    16	#define CHECK_SELF() \
    17	    if (argc < 1 || !argv[0].is_array()) { \
    18	        vm->error("Array method expects 'this' to be an Array."); \
    19	        return Value(null_t{}); \
    20	    } \
    21	    array_t self = argv[0].as_array();
    22	
    23	static Value push(Machine* vm, int argc, Value* argv) {
    24	    CHECK_SELF();
    25	    if (self->size() + (argc - 1) >= MAX_ARRAY_CAPACITY) [[unlikely]] {
    26	        vm->error("Array size exceeded limit during push.");
    27	        return Value(null_t{});
    28	    }
    29	
    30	    for (int i = 1; i < argc; ++i) {
    31	        self->push(argv[i]);
    32	    }
    33	    return Value((int64_t)self->size());
    34	}
    35	
    36	static Value pop(Machine* vm, int argc, Value* argv) {
    37	    CHECK_SELF();
    38	    if (self->empty()) return Value(null_t{}); 
    39	    
    40	    Value val = self->back();
    41	    self->pop();
    42	    return val;
    43	}
    44	
    45	static Value clear(Machine* vm, int argc, Value* argv) {
    46	    CHECK_SELF();
    47	    self->clear();
    48	    return Value(null_t{});
    49	}
    50	
    51	static Value length(Machine* vm, int argc, Value* argv) {
    52	    CHECK_SELF();
    53	    return Value((int64_t)self->size());
    54	}
    55	
    56	static Value reserve(Machine* vm, int argc, Value* argv) {
    57	    CHECK_SELF();
    58	    if (argc < 2 || !argv[1].is_int()) return Value(null_t{});
    59	    int64_t cap = argv[1].as_int();
    60	    
    61	    if (cap > 0 && static_cast<size_t>(cap) < MAX_ARRAY_CAPACITY) {
    62	        self->reserve(static_cast<size_t>(cap));
    63	    }
    64	    return Value(null_t{});
    65	}
    66	
    67	static Value resize(Machine* vm, int argc, Value* argv) {
    68	    CHECK_SELF();
    69	    if (argc < 2 || !argv[1].is_int()) {
    70	        vm->error("resize expects an integer size.");
    71	        return Value(null_t{});
    72	    }
    73	
    74	    int64_t input_size = argv[1].as_int();
    75	    Value fill_val = (argc > 2) ? argv[2] : Value(null_t{});
    76	
    77	    if (input_size < 0) {
    78	        vm->error("New size cannot be negative.");
    79	        return Value(null_t{});
    80	    }
    81	
    82	    if (static_cast<size_t>(input_size) > MAX_ARRAY_CAPACITY) {
    83	        vm->error(std::format("New size too large ({}). Max allowed: {}", input_size, MAX_ARRAY_CAPACITY));
    84	        return Value(null_t{});
    85	    }
    86	
    87	    try {
    88	        size_t old_size = self->size();
    89	        size_t new_size = static_cast<size_t>(input_size);
    90	        self->resize(new_size);
    91	        
    92	        if (new_size > old_size && !fill_val.is_null()) {
    93	            for(size_t i = old_size; i < new_size; ++i) {
    94	                self->set(i, fill_val);
    95	            }
    96	        }
    97	    } catch (const std::exception& e) {
    98	        vm->error("Out of memory during array resize.");
    99	    }
   100	
   101	    return Value(null_t{});
   102	}
   103	
   104	static Value slice(Machine* vm, int argc, Value* argv) {
   105	    CHECK_SELF();
   106	    
   107	    int64_t len = static_cast<int64_t>(self->size());
   108	    int64_t start = 0;
   109	    int64_t end = len;
   110	
   111	    if (argc >= 2 && argv[1].is_int()) {
   112	        start = argv[1].as_int();
   113	        if (start < 0) start += len;
   114	        if (start < 0) start = 0;
   115	        if (start > len) start = len;
   116	    }
   117	
   118	    if (argc >= 3 && argv[2].is_int()) {
   119	        end = argv[2].as_int();
   120	        if (end < 0) end += len;
   121	        if (end < 0) end = 0;
   122	        if (end > len) end = len;
   123	    }
   124	
   125	    if (start >= end) {
   126	        return Value(vm->get_heap()->new_array());
   127	    }
   128	
   129	    auto new_arr = vm->get_heap()->new_array();
   130	    new_arr->reserve(static_cast<size_t>(end - start));
   131	    
   132	    for (int64_t i = start; i < end; ++i) {
   133	        new_arr->push(self->get(static_cast<size_t>(i)));
   134	    }
   135	    
   136	    return Value(new_arr);
   137	}
   138	
   139	static Value reverse(Machine* vm, int argc, Value* argv) {
   140	    CHECK_SELF();
   141	    std::reverse(self->begin(), self->end());
   142	    return argv[0];
   143	}
   144	
   145	static Value forEach(Machine* vm, int argc, Value* argv) {
   146	    CHECK_SELF();
   147	    if (argc < 2) return Value(null_t{});
   148	    Value callback = argv[1];
   149	
   150	    for (size_t i = 0; i < self->size(); ++i) {
   151	        std::vector<Value> args = { self->get(i), Value((int64_t)i) };
   152	        vm->call_callable(callback, args);
   153	        if (vm->has_error()) return Value(null_t{});
   154	    }
   155	    return Value(null_t{});
   156	}
   157	
   158	static Value map(Machine* vm, int argc, Value* argv) {
   159	    CHECK_SELF();
   160	    if (argc < 2) return Value(null_t{});
   161	    Value callback = argv[1];
   162	
   163	    auto result_arr = vm->get_heap()->new_array();
   164	    result_arr->reserve(self->size());
   165	
   166	    for (size_t i = 0; i < self->size(); ++i) {
   167	        std::vector<Value> args = { self->get(i), Value((int64_t)i) };
   168	        Value res = vm->call_callable(callback, args);
   169	        
   170	        if (vm->has_error()) return Value(null_t{});
   171	        result_arr->push(res);
   172	    }
   173	    return Value(result_arr);
   174	}
   175	
   176	static Value filter(Machine* vm, int argc, Value* argv) {
   177	    CHECK_SELF();
   178	    if (argc < 2) return Value(null_t{});
   179	    Value callback = argv[1];
   180	
   181	    auto result_arr = vm->get_heap()->new_array();
   182	
   183	    for (size_t i = 0; i < self->size(); ++i) {
   184	        Value val = self->get(i);
   185	        std::vector<Value> args = { val, Value((int64_t)i) };
   186	        Value condition = vm->call_callable(callback, args);
   187	        if (vm->has_error()) return Value(null_t{});
   188	        
   189	        if (to_bool(condition)) {
   190	            result_arr->push(val);
   191	        }
   192	    }
   193	    return Value(result_arr);
   194	}
   195	
   196	static Value reduce(Machine* vm, int argc, Value* argv) {
   197	    CHECK_SELF();
   198	    if (argc < 2) return Value(null_t{});
   199	    Value callback = argv[1];
   200	    Value accumulator = (argc > 2) ? argv[2] : Value(null_t{});
   201	    
   202	    size_t start_index = 0;
   203	
   204	    if (argc < 3) {
   205	        if (self->empty()) {
   206	            vm->error("Reduce on empty array with no initial value.");
   207	            return Value(null_t{});
   208	        }
   209	        accumulator = self->get(0);
   210	        start_index = 1;
   211	    }
   212	
   213	    for (size_t i = start_index; i < self->size(); ++i) {
   214	        std::vector<Value> args = { accumulator, self->get(i), Value((int64_t)i) };
   215	        accumulator = vm->call_callable(callback, args);
   216	        if (vm->has_error()) return Value(null_t{});
   217	    }
   218	    return accumulator;
   219	}
   220	
   221	static Value find(Machine* vm, int argc, Value* argv) {
   222	    CHECK_SELF();
   223	    if (argc < 2) return Value(null_t{});
   224	    Value callback = argv[1];
   225	
   226	    for (size_t i = 0; i < self->size(); ++i) {
   227	        Value val = self->get(i);
   228	        std::vector<Value> args = { val, Value((int64_t)i) };
   229	        Value res = vm->call_callable(callback, args);
   230	        if (vm->has_error()) return Value(null_t{});
   231	        
   232	        if (to_bool(res)) return val;
   233	    }
   234	    return Value(null_t{});
   235	}
   236	
   237	static Value findIndex(Machine* vm, int argc, Value* argv) {
   238	    CHECK_SELF();
   239	    if (argc < 2) return Value((int64_t)-1);
   240	    Value callback = argv[1];
   241	
   242	    for (size_t i = 0; i < self->size(); ++i) {
   243	        Value val = self->get(i);
   244	        std::vector<Value> args = { val, Value((int64_t)i) };
   245	        Value res = vm->call_callable(callback, args);
   246	        if (vm->has_error()) return Value((int64_t)-1);
   247	        
   248	        if (to_bool(res)) return Value((int64_t)i);
   249	    }
   250	    return Value((int64_t)-1);
   251	}
   252	
   253	static Value sort(Machine* vm, int argc, Value* argv) {
   254	    CHECK_SELF();
   255	    
   256	    size_t n = self->size();
   257	    if (n < 2) return argv[0];
   258	
   259	    bool has_comparator = (argc > 1);
   260	    Value comparator = has_comparator ? argv[1] : Value(null_t{});
   261	
   262	    for (size_t i = 0; i < n - 1; i++) {
   263	        for (size_t j = 0; j < n - i - 1; j++) {
   264	            Value a = self->get(j);
   265	            Value b = self->get(j + 1);
   266	            bool swap = false;
   267	
   268	            if (has_comparator) {
   269	                std::vector<Value> args = { a, b };
   270	                Value res = vm->call_callable(comparator, args);
   271	                if (vm->has_error()) return Value(null_t{});
   272	                if (res.is_int() && res.as_int() > 0) swap = true; 
   273	                else if (res.is_float() && res.as_float() > 0) swap = true;
   274	            } else {
   275	                if (a.is_int() && b.is_int()) {
   276	                    if (a.as_int() > b.as_int()) swap = true;
   277	                } else if (a.is_float() || b.is_float()) {
   278	                    if (to_float(a) > to_float(b)) swap = true;
   279	                } else {
   280	                    if (std::string_view(to_string(a)) > std::string_view(to_string(b))) swap = true;
   281	                }
   282	            }
   283	
   284	            if (swap) {
   285	                self->set(j, b);
   286	                self->set(j + 1, a);
   287	            }
   288	        }
   289	    }
   290	    return argv[0];
   291	}
   292	
   293	} // namespace meow::natives::array
   294	
   295	namespace meow::stdlib {
   296	module_t create_array_module(Machine* vm, MemoryManager* heap) noexcept {
   297	    auto name = heap->new_string("array");
   298	    auto mod = heap->new_module(name, name);
   299	    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };
   300	
   301	    using namespace meow::natives::array;
   302	    reg("push", push);
   303	    reg("pop", pop);
   304	    reg("clear", clear);
   305	    reg("len", length);
   306	    reg("size", length); 
   307	    reg("length", length);
   308	    reg("resize", resize);
   309	    reg("reserve", reserve);
   310	    reg("slice", slice); 
   311	    
   312	    reg("map", map);
   313	    reg("filter", filter);
   314	    reg("reduce", reduce);
   315	    reg("forEach", forEach);
   316	    reg("find", find);
   317	    reg("findIndex", findIndex);
   318	    reg("reverse", reverse);
   319	    reg("sort", sort);
   320	
   321	    return mod;
   322	}
   323	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/io_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/cast.h>
     7	#include <meow/core/module.h>
     8	
     9	namespace meow::natives::io {
    10	
    11	namespace fs = std::filesystem;
    12	
    13	#define CHECK_ARGS(n) \
    14	    if (argc < n) [[unlikely]] { \
    15	        vm->error("IO Error: Expected at least " #n " arguments."); \
    16	        return Value(null_t{}); \
    17	    }
    18	
    19	#define CHECK_PATH_ARG(idx) \
    20	    if (argc <= idx || !argv[idx].is_string()) [[unlikely]] { \
    21	        vm->error(std::format("IO Error: Argument {} (Path) expects a String, but received {}.", idx, to_string(argv[idx]))); \
    22	        return Value(null_t{}); \
    23	    } \
    24	    const char* path_str_##idx = argv[idx].as_string()->c_str();
    25	
    26	
    27	static Value input(Machine* vm, int argc, Value* argv) {
    28	    if (argc > 0) {
    29	        std::print("{}", to_string(argv[0]));
    30	        std::cout.flush();
    31	    }
    32	    
    33	    std::string line;
    34	    if (std::getline(std::cin, line)) {
    35	        if (!line.empty() && line.back() == '\r') line.pop_back();
    36	        return Value(vm->get_heap()->new_string(line));
    37	    }
    38	    return Value(null_t{});
    39	}
    40	
    41	static Value read_file(Machine* vm, int argc, Value* argv) {
    42	    CHECK_ARGS(1);
    43	    CHECK_PATH_ARG(0);
    44	    
    45	    std::ifstream file(path_str_0, std::ios::binary | std::ios::ate);
    46	    if (!file) return Value(null_t{});
    47	
    48	    auto size = file.tellg();
    49	    if (size == -1) return Value(null_t{});
    50	    
    51	    file.seekg(0);
    52	
    53	    std::string content(static_cast<size_t>(size), '\0');
    54	    if (file.read(content.data(), size)) {
    55	        if (content.size() >= 3 && 
    56	            static_cast<unsigned char>(content[0]) == 0xEF && 
    57	            static_cast<unsigned char>(content[1]) == 0xBB && 
    58	            static_cast<unsigned char>(content[2]) == 0xBF) {
    59	            content.erase(0, 3);
    60	        }
    61	
    62	        return Value(vm->get_heap()->new_string(content));
    63	    }
    64	    return Value(null_t{});
    65	}
    66	
    67	static Value write_file(Machine* vm, int argc, Value* argv) {
    68	    CHECK_ARGS(2);
    69	    CHECK_PATH_ARG(0);
    70	    
    71	    std::string data = to_string(argv[1]);
    72	    bool append = (argc > 2) ? to_bool(argv[2]) : false;
    73	
    74	    auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    75	    std::ofstream file(path_str_0, mode);
    76	    
    77	    return Value(file && (file << data));
    78	}
    79	
    80	static Value file_exists(Machine* vm, int argc, Value* argv) {
    81	    CHECK_ARGS(1);
    82	    CHECK_PATH_ARG(0);
    83	    std::error_code ec;
    84	    return Value(fs::exists(path_str_0, ec));
    85	}
    86	
    87	static Value is_directory(Machine* vm, int argc, Value* argv) {
    88	    CHECK_ARGS(1);
    89	    CHECK_PATH_ARG(0);
    90	    std::error_code ec;
    91	    return Value(fs::is_directory(path_str_0, ec));
    92	}
    93	
    94	static Value list_dir(Machine* vm, int argc, Value* argv) {
    95	    CHECK_ARGS(1);
    96	    CHECK_PATH_ARG(0);
    97	    std::error_code ec;
    98	    
    99	    if (!fs::exists(path_str_0, ec) || !fs::is_directory(path_str_0, ec)) return Value(null_t{});
   100	
   101	    auto arr = vm->get_heap()->new_array();
   102	    
   103	    auto dir_it = fs::directory_iterator(path_str_0, ec);
   104	    if (ec) return Value(null_t{});
   105	
   106	    for (const auto& entry : dir_it) {
   107	        arr->push(Value(vm->get_heap()->new_string(entry.path().filename().string())));
   108	    }
   109	    return Value(arr);
   110	}
   111	
   112	static Value create_dir(Machine* vm, int argc, Value* argv) {
   113	    CHECK_ARGS(1);
   114	    CHECK_PATH_ARG(0);
   115	    std::error_code ec;
   116	    return Value(fs::create_directories(path_str_0, ec));
   117	}
   118	
   119	static Value delete_file(Machine* vm, int argc, Value* argv) {
   120	    CHECK_ARGS(1);
   121	    CHECK_PATH_ARG(0);
   122	    std::error_code ec;
   123	    return Value(fs::remove_all(path_str_0, ec) > 0);
   124	}
   125	
   126	static Value rename_file(Machine* vm, int argc, Value* argv) {
   127	    CHECK_ARGS(2);
   128	    CHECK_PATH_ARG(0); // Source
   129	    CHECK_PATH_ARG(1); // Destination
   130	    std::error_code ec;
   131	    fs::rename(path_str_0, path_str_1, ec);
   132	    return Value(!ec);
   133	}
   134	
   135	static Value copy_file(Machine* vm, int argc, Value* argv) {
   136	    CHECK_ARGS(2);
   137	    CHECK_PATH_ARG(0);
   138	    CHECK_PATH_ARG(1);
   139	    std::error_code ec;
   140	    fs::copy(path_str_0, path_str_1, 
   141	             fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
   142	    return Value(!ec);
   143	}
   144	
   145	static Value get_file_size(Machine* vm, int argc, Value* argv) {
   146	    CHECK_ARGS(1);
   147	    CHECK_PATH_ARG(0);
   148	    std::error_code ec;
   149	    auto sz = fs::file_size(path_str_0, ec);
   150	    if (ec) return Value(static_cast<int64_t>(-1));
   151	    return Value(static_cast<int64_t>(sz));
   152	}
   153	
   154	static Value get_file_timestamp(Machine* vm, int argc, Value* argv) {
   155	    CHECK_ARGS(1);
   156	    CHECK_PATH_ARG(0);
   157	    std::error_code ec;
   158	    auto ftime = fs::last_write_time(path_str_0, ec);
   159	    if (ec) return Value(static_cast<int64_t>(-1));
   160	
   161	    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
   162	        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
   163	    );
   164	    return Value(static_cast<int64_t>(
   165	        std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count()
   166	    ));
   167	}
   168	
   169	static Value get_file_name(Machine* vm, int argc, Value* argv) {
   170	    CHECK_ARGS(1);
   171	    CHECK_PATH_ARG(0);
   172	    fs::path p(path_str_0);
   173	    return Value(vm->get_heap()->new_string(p.filename().string()));
   174	}
   175	
   176	static Value get_file_extension(Machine* vm, int argc, Value* argv) {
   177	    CHECK_ARGS(1);
   178	    CHECK_PATH_ARG(0);
   179	    fs::path p(path_str_0);
   180	    std::string ext = p.extension().string();
   181	    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
   182	    return Value(vm->get_heap()->new_string(ext));
   183	}
   184	
   185	static Value get_file_stem(Machine* vm, int argc, Value* argv) {
   186	    CHECK_ARGS(1);
   187	    CHECK_PATH_ARG(0);
   188	    fs::path p(path_str_0);
   189	    return Value(vm->get_heap()->new_string(p.stem().string()));
   190	}
   191	
   192	static Value get_abs_path(Machine* vm, int argc, Value* argv) {
   193	    CHECK_ARGS(1);
   194	    CHECK_PATH_ARG(0);
   195	    std::error_code ec;
   196	    
   197	    fs::path p = fs::absolute(path_str_0, ec);
   198	    
   199	    if (ec) {
   200	        vm->error(std::format("IO Error: Could not resolve absolute path for '{}'. Error: {}", path_str_0, ec.message()));
   201	        return Value(null_t{});
   202	    }
   203	    
   204	    return Value(vm->get_heap()->new_string(p.string()));
   205	}
   206	
   207	} // namespace meow::natives::io
   208	
   209	namespace meow::stdlib {
   210	
   211	module_t create_io_module(Machine* vm, MemoryManager* heap) noexcept {
   212	    auto name = heap->new_string("io");
   213	    auto mod = heap->new_module(name, name);
   214	
   215	    auto reg = [&](const char* n, native_t fn) {
   216	        mod->set_export(heap->new_string(n), Value(fn));
   217	    };
   218	
   219	    using namespace meow::natives::io;
   220	    
   221	    reg("input", input);
   222	    reg("read", read_file);
   223	    reg("write", write_file);
   224	    
   225	    reg("fileExists", file_exists);
   226	    reg("isDirectory", is_directory);
   227	    reg("listDir", list_dir);
   228	    reg("createDir", create_dir);
   229	    reg("deleteFile", delete_file);
   230	    reg("renameFile", rename_file);
   231	    reg("copyFile", copy_file);
   232	    
   233	    reg("getFileSize", get_file_size);
   234	    reg("getFileTimestamp", get_file_timestamp);
   235	    reg("getFileName", get_file_name);
   236	    reg("getFileExtension", get_file_extension);
   237	    reg("getFileStem", get_file_stem);
   238	    reg("getAbsolutePath", get_abs_path);
   239	
   240	    return mod;
   241	}
   242	
   243	} // namespace meow::stdlib
   244	
   245	// Dọn dẹp macros
   246	#undef CHECK_ARGS
   247	#undef CHECK_PATH_ARG


// =============================================================================
//  FILE PATH: src/vm/stdlib/json_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/memory/gc_disable_guard.h>
     7	#include <meow/core/array.h>
     8	#include <meow/core/hash_table.h>
     9	#include <meow/core/string.h>
    10	#include <meow/core/module.h>
    11	#include <meow/cast.h>
    12	
    13	namespace meow::natives::json {
    14	
    15	class JsonParser {
    16	private:
    17	    std::string_view json_;
    18	    size_t pos_ = 0;
    19	    Machine* vm_;
    20	    bool has_error_ = false;
    21	
    22	    Value report_error() {
    23	        has_error_ = true;
    24	        return Value(null_t{});
    25	    }
    26	
    27	    char peek() const {
    28	        return pos_ < json_.length() ? json_[pos_] : '\0';
    29	    }
    30	
    31	    void advance() {
    32	        if (pos_ < json_.length()) pos_++;
    33	    }
    34	
    35	    void skip_whitespace() {
    36	        while (pos_ < json_.length() && std::isspace(static_cast<unsigned char>(json_[pos_]))) {
    37	            pos_++;
    38	        }
    39	    }
    40	
    41	    Value parse_value();
    42	    Value parse_object();
    43	    Value parse_array();
    44	    Value parse_string();
    45	    Value parse_number();
    46	    Value parse_true();
    47	    Value parse_false();
    48	    Value parse_null();
    49	
    50	public:
    51	    explicit JsonParser(Machine* vm) : vm_(vm) {}
    52	
    53	    Value parse(std::string_view str) {
    54	        json_ = str;
    55	        pos_ = 0;
    56	        has_error_ = false;
    57	        
    58	        skip_whitespace();
    59	        if (json_.empty()) return Value(null_t{});
    60	
    61	        Value result = parse_value();
    62	        
    63	        if (has_error_) return Value(null_t{});
    64	
    65	        skip_whitespace();
    66	        if (pos_ < json_.length()) {
    67	            return report_error();
    68	        }
    69	        return result;
    70	    }
    71	};
    72	
    73	Value JsonParser::parse_value() {
    74	    skip_whitespace();
    75	    if (pos_ >= json_.length()) return report_error();
    76	
    77	    char c = peek();
    78	    switch (c) {
    79	        case '{': return parse_object();
    80	        case '[': return parse_array();
    81	        case '"': return parse_string();
    82	        case 't': return parse_true();
    83	        case 'f': return parse_false();
    84	        case 'n': return parse_null();
    85	        default:
    86	            if (std::isdigit(c) || c == '-') {
    87	                return parse_number();
    88	            }
    89	            return report_error();
    90	    }
    91	}
    92	
    93	Value JsonParser::parse_object() {
    94	    advance();
    95	    
    96	    auto hash = vm_->get_heap()->new_hash();
    97	
    98	    skip_whitespace();
    99	    if (peek() == '}') {
   100	        advance();
   101	        return Value(hash);
   102	    }
   103	
   104	    while (true) {
   105	        skip_whitespace();
   106	        if (peek() != '"') return report_error();
   107	
   108	        Value key_val = parse_string();
   109	        if (has_error_) return key_val;
   110	
   111	        string_t key_str = key_val.as_string();
   112	
   113	        skip_whitespace();
   114	        if (peek() != ':') return report_error();
   115	        advance();
   116	
   117	        Value val = parse_value();
   118	        if (has_error_) return val;
   119	
   120	        hash->set(key_str, val);
   121	
   122	        skip_whitespace();
   123	        char next = peek();
   124	        if (next == '}') {
   125	            advance();
   126	            break;
   127	        }
   128	        if (next != ',') return report_error();
   129	        advance();
   130	    }
   131	    return Value(hash);
   132	}
   133	
   134	Value JsonParser::parse_array() {
   135	    advance();
   136	    
   137	    auto arr = vm_->get_heap()->new_array();
   138	
   139	    skip_whitespace();
   140	    if (peek() == ']') {
   141	        advance();
   142	        return Value(arr);
   143	    }
   144	
   145	    while (true) {
   146	        Value elem = parse_value();
   147	        if (has_error_) return elem;
   148	        
   149	        arr->push(elem);
   150	
   151	        skip_whitespace();
   152	        char next = peek();
   153	        if (next == ']') {
   154	            advance();
   155	            break;
   156	        }
   157	        if (next != ',') return report_error();
   158	        advance();
   159	    }
   160	    return Value(arr);
   161	}
   162	
   163	Value JsonParser::parse_string() {
   164	    advance();
   165	    std::string s;
   166	    s.reserve(32);
   167	
   168	    while (pos_ < json_.length() && peek() != '"') {
   169	        if (peek() == '\\') {
   170	            advance();
   171	            if (pos_ >= json_.length()) return report_error();
   172	            
   173	            char escaped = peek();
   174	            switch (escaped) {
   175	                case '"':  s += '"'; break;
   176	                case '\\': s += '\\'; break;
   177	                case '/':  s += '/'; break;
   178	                case 'b':  s += '\b'; break;
   179	                case 'f':  s += '\f'; break;
   180	                case 'n':  s += '\n'; break;
   181	                case 'r':  s += '\r'; break;
   182	                case 't':  s += '\t'; break;
   183	                case 'u': {
   184	                    advance(); // Skip 'u'
   185	                    if (pos_ + 4 > json_.length()) return report_error();
   186	
   187	                    unsigned int codepoint = 0;
   188	                    for (int i = 0; i < 4; ++i) {
   189	                        char h = peek();
   190	                        advance();
   191	                        codepoint <<= 4;
   192	                        if (h >= '0' && h <= '9') codepoint |= (h - '0');
   193	                        else if (h >= 'a' && h <= 'f') codepoint |= (10 + h - 'a');
   194	                        else if (h >= 'A' && h <= 'F') codepoint |= (10 + h - 'A');
   195	                        else return report_error();
   196	                    }
   197	
   198	                    if (codepoint <= 0x7F) {
   199	                        s += static_cast<char>(codepoint);
   200	                    } else if (codepoint <= 0x7FF) {
   201	                        s += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
   202	                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
   203	                    } else {
   204	                        s += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
   205	                        s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
   206	                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
   207	                    }
   208	                    continue;
   209	                }
   210	                default:
   211	                    s += escaped; break;
   212	            }
   213	        } else {
   214	            s += peek();
   215	        }
   216	        advance();
   217	    }
   218	
   219	    if (pos_ >= json_.length() || peek() != '"') {
   220	        return report_error();
   221	    }
   222	    advance();
   223	
   224	    return Value(vm_->get_heap()->new_string(s));
   225	}
   226	
   227	Value JsonParser::parse_number() {
   228	    size_t start = pos_;
   229	    if (peek() == '-') advance();
   230	
   231	    if (peek() == '0') {
   232	        advance();
   233	    } else if (std::isdigit(peek())) {
   234	        while (pos_ < json_.length() && std::isdigit(peek())) advance();
   235	    } else {
   236	        return report_error();
   237	    }
   238	
   239	    bool is_float = false;
   240	    if (pos_ < json_.length() && peek() == '.') {
   241	        is_float = true;
   242	        advance();
   243	        if (!std::isdigit(peek())) return report_error();
   244	        while (pos_ < json_.length() && std::isdigit(peek())) advance();
   245	    }
   246	
   247	    if (pos_ < json_.length() && (peek() == 'e' || peek() == 'E')) {
   248	        is_float = true;
   249	        advance();
   250	        if (pos_ < json_.length() && (peek() == '+' || peek() == '-')) advance();
   251	        if (!std::isdigit(peek())) return report_error();
   252	        while (pos_ < json_.length() && std::isdigit(peek())) advance();
   253	    }
   254	
   255	    std::string num_str(json_.substr(start, pos_ - start));
   256	    
   257	    try {
   258	        if (is_float) {
   259	            return Value(std::stod(num_str));
   260	        } else {
   261	            return Value(static_cast<int64_t>(std::stoll(num_str)));
   262	        }
   263	    } catch (...) {
   264	        return report_error();
   265	    }
   266	}
   267	
   268	Value JsonParser::parse_true() {
   269	    if (json_.substr(pos_, 4) == "true") {
   270	        pos_ += 4;
   271	        return Value(true);
   272	    }
   273	    return report_error();
   274	}
   275	
   276	Value JsonParser::parse_false() {
   277	    if (json_.substr(pos_, 5) == "false") {
   278	        pos_ += 5;
   279	        return Value(false);
   280	    }
   281	    return report_error();
   282	}
   283	
   284	Value JsonParser::parse_null() {
   285	    if (json_.substr(pos_, 4) == "null") {
   286	        pos_ += 4;
   287	        return Value(null_t{});
   288	    }
   289	    return report_error();
   290	}
   291	
   292	// ============================================================================
   293	// 🖨️ JSON STRINGIFIER
   294	// ============================================================================
   295	
   296	static std::string escape_string(std::string_view s) {
   297	    std::ostringstream o;
   298	    o << '"';
   299	    for (unsigned char c : s) {
   300	        switch (c) {
   301	            case '"':  o << "\\\""; break;
   302	            case '\\': o << "\\\\"; break;
   303	            case '\b': o << "\\b"; break;
   304	            case '\f': o << "\\f"; break;
   305	            case '\n': o << "\\n"; break;
   306	            case '\r': o << "\\r"; break;
   307	            case '\t': o << "\\t"; break;
   308	            default:
   309	                if (c <= 0x1F) {
   310	                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
   311	                } else {
   312	                    o << c;
   313	                }
   314	        }
   315	    }
   316	    o << '"';
   317	    return o.str();
   318	}
   319	
   320	static std::string to_json_recursive(const Value& val, int indent_level, int tab_size) {
   321	    std::ostringstream ss;
   322	    std::string indent(indent_level * tab_size, ' ');
   323	    std::string inner_indent((indent_level + 1) * tab_size, ' ');
   324	    bool pretty = (tab_size > 0);
   325	    const char* newline = pretty ? "\n" : "";
   326	    const char* sep = pretty ? ": " : ":";
   327	
   328	    if (val.is_null()) {
   329	        ss << "null";
   330	    } 
   331	    else if (val.is_bool()) {
   332	        ss << (val.as_bool() ? "true" : "false");
   333	    } 
   334	    else if (val.is_int()) {
   335	        ss << val.as_int();
   336	    } 
   337	    else if (val.is_float()) {
   338	        ss << val.as_float();
   339	    } 
   340	    else if (val.is_string()) {
   341	        ss << escape_string(val.as_string()->c_str());
   342	    } 
   343	    else if (val.is_array()) {
   344	        array_t arr = val.as_array();
   345	        if (arr->empty()) {
   346	            ss << "[]";
   347	        } else {
   348	            ss << "[" << newline;
   349	            for (size_t i = 0; i < arr->size(); ++i) {
   350	                if (pretty) ss << inner_indent;
   351	                ss << to_json_recursive(arr->get(i), indent_level + 1, tab_size);
   352	                if (i + 1 < arr->size()) ss << ",";
   353	                ss << newline;
   354	            }
   355	            if (pretty) ss << indent;
   356	            ss << "]";
   357	        }
   358	    } 
   359	    else if (val.is_hash_table()) {
   360	        hash_table_t obj = val.as_hash_table();
   361	        if (obj->empty()) {
   362	            ss << "{}";
   363	        } else {
   364	            ss << "{" << newline;
   365	            size_t i = 0;
   366	            size_t size = obj->size();
   367	            for (auto it = obj->begin(); it != obj->end(); ++it) {
   368	                if (pretty) ss << inner_indent;
   369	                ss << escape_string(it->first->c_str()) << sep;
   370	                ss << to_json_recursive(it->second, indent_level + 1, tab_size);
   371	                if (i + 1 < size) ss << ",";
   372	                ss << newline;
   373	                i++;
   374	            }
   375	            if (pretty) ss << indent;
   376	            ss << "}";
   377	        }
   378	    } 
   379	    else {
   380	        ss << "\"<unsupported_type>\"";
   381	    }
   382	    
   383	    return ss.str();
   384	}
   385	
   386	static Value json_parse(Machine* vm, int argc, Value* argv) {
   387	    if (argc < 1 || !argv[0].is_string()) {
   388	        return Value(null_t{});
   389	    }
   390	
   391	    std::string_view json_str = argv[0].as_string()->c_str();
   392	    JsonParser parser(vm);
   393	    return parser.parse(json_str);
   394	}
   395	
   396	static Value json_stringify(Machine* vm, int argc, Value* argv) {
   397	    if (argc < 1) return Value(null_t{});
   398	    
   399	    int tab_size = 2;
   400	    if (argc > 1 && argv[1].is_int()) {
   401	        tab_size = static_cast<int>(argv[1].as_int());
   402	        if (tab_size < 0) tab_size = 0;
   403	    }
   404	    
   405	    std::string res = to_json_recursive(argv[0], 0, tab_size);
   406	    return Value(vm->get_heap()->new_string(res));
   407	}
   408	
   409	} // namespace meow::natives::json
   410	
   411	namespace meow::stdlib {
   412	
   413	module_t create_json_module(Machine* vm, MemoryManager* heap) noexcept {
   414	    auto name = heap->new_string("json");
   415	    auto mod = heap->new_module(name, name);
   416	
   417	    auto reg = [&](const char* n, native_t fn) {
   418	        mod->set_export(heap->new_string(n), Value(fn));
   419	    };
   420	
   421	    using namespace meow::natives::json;
   422	    reg("parse", json_parse);
   423	    reg("stringify", json_stringify);
   424	
   425	    return mod;
   426	}
   427	
   428	} // namespace meow::stdlib


// =============================================================================
//  FILE PATH: src/vm/stdlib/memory_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <cstdlib>
     4	#include <meow/machine.h>
     5	#include <meow/value.h>
     6	#include <meow/memory/memory_manager.h>
     7	
     8	namespace meow::stdlib {
     9	
    10	// malloc(size: int) -> pointer
    11	static Value malloc(Machine* vm, int argc, Value* argv) {
    12	    if (argc < 1 || !argv[0].is_int()) [[unlikely]] {
    13	        return Value(); 
    14	    }
    15	
    16	    size_t size = static_cast<size_t>(argv[0].as_int());
    17	    
    18	    if (size == 0) return Value();
    19	
    20	    void* buffer = std::malloc(size);
    21	    return Value(buffer);
    22	} 
    23	
    24	// free(ptr: pointer) -> null
    25	static Value free(Machine* vm, int argc, Value* argv) {
    26	    if (argc >= 1 && argv[0].is_pointer()) [[likely]] {
    27	        void* ptr = argv[0].as_pointer();
    28	        if (ptr) std::free(ptr);
    29	    }
    30	    return Value();
    31	}
    32	    
    33	module_t create_memory_module(Machine* vm, MemoryManager* heap) noexcept {
    34	    auto name = heap->new_string("memory");
    35	    auto mod = heap->new_module(name, name);
    36	    
    37	    auto reg = [&](const char* n, native_t fn) { 
    38	        mod->set_export(heap->new_string(n), Value(fn)); 
    39	    };
    40	
    41	    reg("malloc", malloc);
    42	    reg("free", free);
    43	
    44	    return mod;
    45	}
    46	
    47	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/object_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/memory/memory_manager.h>
     5	#include <meow/memory/gc_disable_guard.h>
     6	#include <meow/core/module.h>
     7	#include <meow/core/hash_table.h>
     8	#include <meow/core/array.h>
     9	
    10	namespace meow::natives::obj {
    11	
    12	#define CHECK_SELF() \
    13	    if (argc < 1 || !argv[0].is_hash_table()) [[unlikely]] { \
    14	        vm->error("Object method expects 'this' to be a Hash Table."); \
    15	        return Value(null_t{}); \
    16	    } \
    17	    hash_table_t self = argv[0].as_hash_table(); \
    18	
    19	static Value keys(Machine* vm, int argc, Value* argv) {
    20	    CHECK_SELF();
    21	    auto arr = vm->get_heap()->new_array();
    22	    arr->reserve(self->size());
    23	    for(auto it = self->begin(); it != self->end(); ++it) {
    24	        arr->push(Value(it->first));
    25	    }
    26	    return Value(arr);
    27	}
    28	
    29	static Value values(Machine* vm, int argc, Value* argv) {
    30	    CHECK_SELF();
    31	    auto arr = vm->get_heap()->new_array();
    32	    arr->reserve(self->size());
    33	    for(auto it = self->begin(); it != self->end(); ++it) {
    34	        arr->push(it->second);
    35	    }
    36	    return Value(arr);
    37	}
    38	
    39	static Value entries(Machine* vm, int argc, Value* argv) {
    40	    CHECK_SELF();
    41	    auto arr = vm->get_heap()->new_array();
    42	    arr->reserve(self->size());
    43	    
    44	    for(auto it = self->begin(); it != self->end(); ++it) {
    45	        auto pair = vm->get_heap()->new_array();
    46	        pair->push(Value(it->first));
    47	        pair->push(it->second);
    48	        arr->push(Value(pair));
    49	    }
    50	    return Value(arr);
    51	}
    52	
    53	static Value has(Machine* vm, int argc, Value* argv) {
    54	    CHECK_SELF();
    55	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    56	    return Value(self->has(argv[1].as_string()));
    57	}
    58	
    59	static Value len(Machine* vm, int argc, Value* argv) {
    60	    CHECK_SELF();
    61	    return Value((int64_t)self->size());
    62	}
    63	
    64	static Value merge(Machine* vm, int argc, Value* argv) {    
    65	    auto result = vm->get_heap()->new_hash();
    66	    
    67	    for (int i = 0; i < argc; ++i) {
    68	        if (argv[i].is_hash_table()) {
    69	            hash_table_t src = argv[i].as_hash_table();
    70	            for (auto it = src->begin(); it != src->end(); ++it) {
    71	                result->set(it->first, it->second);
    72	            }
    73	        }
    74	    }
    75	    return Value(result);
    76	}
    77	
    78	} // namespace
    79	
    80	namespace meow::stdlib {
    81	module_t create_object_module(Machine* vm, MemoryManager* heap) noexcept {
    82	    auto name = heap->new_string("object");
    83	    auto mod = heap->new_module(name, name);
    84	    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };
    85	
    86	    using namespace meow::natives::obj;
    87	    reg("keys", keys);
    88	    reg("values", values);
    89	    reg("entries", entries);
    90	    reg("has", has);
    91	    reg("len", len);
    92	    reg("merge", merge);
    93	    
    94	    return mod;
    95	}
    96	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/stdlib.h
// =============================================================================

     1	/**
     2	 * @file stdlib.h
     3	 * @brief Factory definitions for Standard Libraries
     4	 * @note  Zero-overhead abstractions.
     5	 */
     6	#pragma once
     7	
     8	#include <meow/common.h>
     9	
    10	namespace meow {
    11	    class Machine;
    12	    class MemoryManager;
    13	}
    14	
    15	namespace meow::stdlib {
    16	    // Factory functions - Return raw Module Object pointer
    17	    [[nodiscard]] module_t create_io_module(Machine* vm, MemoryManager* heap) noexcept;
    18	    [[nodiscard]] module_t create_system_module(Machine* vm, MemoryManager* heap) noexcept;
    19	    [[nodiscard]] module_t create_array_module(Machine* vm, MemoryManager* heap) noexcept;
    20	    [[nodiscard]] module_t create_string_module(Machine* vm, MemoryManager* heap) noexcept;
    21	    [[nodiscard]] module_t create_object_module(Machine* vm, MemoryManager* heap) noexcept;
    22	    [[nodiscard]] module_t create_json_module(Machine* vm, MemoryManager* heap) noexcept;
    23	    [[nodiscard]] module_t create_memory_module(Machine* vm, MemoryManager* heap) noexcept;
    24	}



// =============================================================================
//  FILE PATH: src/vm/stdlib/string_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/memory/memory_manager.h>
     5	#include <meow/memory/gc_disable_guard.h>
     6	#include <meow/core/module.h>
     7	#include <meow/core/array.h>
     8	#include <meow/cast.h>
     9	
    10	namespace meow::natives::str {
    11	
    12	#define CHECK_SELF() \
    13	    if (argc < 1 || !argv[0].is_string()) [[unlikely]] { \
    14	        vm->error("String method expects 'this' to be a String."); \
    15	        return Value(null_t{}); \
    16	    } \
    17	    string_t self_obj = argv[0].as_string(); \
    18	    std::string_view self(self_obj->c_str(), self_obj->size()); \
    19	
    20	static Value len(Machine* vm, int argc, Value* argv) {
    21	    CHECK_SELF();
    22	    return Value((int64_t)self.size());
    23	}
    24	
    25	static Value upper(Machine* vm, int argc, Value* argv) {
    26	    CHECK_SELF();
    27	    std::string s(self);
    28	    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    29	    return Value(vm->get_heap()->new_string(s));
    30	}
    31	
    32	static Value lower(Machine* vm, int argc, Value* argv) {
    33	    CHECK_SELF();
    34	    std::string s(self);
    35	    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    36	    return Value(vm->get_heap()->new_string(s));
    37	}
    38	
    39	static Value trim(Machine* vm, int argc, Value* argv) {
    40	    CHECK_SELF();
    41	    while (!self.empty() && std::isspace(self.front())) self.remove_prefix(1);
    42	    while (!self.empty() && std::isspace(self.back())) self.remove_suffix(1);
    43	    return Value(vm->get_heap()->new_string(self));
    44	}
    45	
    46	static Value contains(Machine* vm, int argc, Value* argv) {
    47	    CHECK_SELF();
    48	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    49	    string_t needle_obj = argv[1].as_string();
    50	    std::string_view needle(needle_obj->c_str(), needle_obj->size());
    51	    return Value(self.find(needle) != std::string::npos);
    52	}
    53	
    54	static Value startsWith(Machine* vm, int argc, Value* argv) {
    55	    CHECK_SELF();
    56	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    57	    string_t prefix_obj = argv[1].as_string();
    58	    std::string_view prefix(prefix_obj->c_str(), prefix_obj->size());
    59	    return Value(self.starts_with(prefix));
    60	}
    61	
    62	static Value endsWith(Machine* vm, int argc, Value* argv) {
    63	    CHECK_SELF();
    64	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    65	    string_t suffix_obj = argv[1].as_string();
    66	    std::string_view suffix(suffix_obj->c_str(), suffix_obj->size());
    67	    return Value(self.ends_with(suffix));
    68	}
    69	
    70	static Value join(Machine* vm, int argc, Value* argv) {
    71	    CHECK_SELF();
    72	    if (argc < 2 || !argv[1].is_array()) return Value(vm->get_heap()->new_string(""));
    73	
    74	    array_t arr = argv[1].as_array();
    75	    std::ostringstream ss;
    76	    for (size_t i = 0; i < arr->size(); ++i) {
    77	        if (i > 0) ss << self;
    78	        Value item = arr->get(i);
    79	        if (item.is_string()) ss << item.as_string()->c_str();
    80	        else ss << to_string(item);
    81	    }
    82	    return Value(vm->get_heap()->new_string(ss.str()));
    83	}
    84	
    85	static Value split(Machine* vm, int argc, Value* argv) {
    86	    CHECK_SELF();
    87	    std::string_view delim = " ";
    88	    if (argc >= 2 && argv[1].is_string()) {
    89	        string_t d = argv[1].as_string();
    90	        delim = std::string_view(d->c_str(), d->size());
    91	    }
    92	
    93	    auto arr = vm->get_heap()->new_array();
    94	    
    95	    if (delim.empty()) {
    96	        for (char c : self) {
    97	            arr->push(Value(vm->get_heap()->new_string(&c, 1)));
    98	        }
    99	    } else {
   100	        size_t start = 0;
   101	        size_t end = self.find(delim);
   102	        while (end != std::string::npos) {
   103	            std::string_view token = self.substr(start, end - start);
   104	            arr->push(Value(vm->get_heap()->new_string(token)));
   105	            start = end + delim.length();
   106	            end = self.find(delim, start);
   107	        }
   108	        std::string_view last = self.substr(start);
   109	        arr->push(Value(vm->get_heap()->new_string(last)));
   110	    }
   111	    return Value(arr);
   112	}
   113	
   114	static Value replace(Machine* vm, int argc, Value* argv) {
   115	    CHECK_SELF();
   116	    if (argc < 3 || !argv[1].is_string() || !argv[2].is_string()) {
   117	        return argv[0];
   118	    }
   119	    string_t from_obj = argv[1].as_string();
   120	    string_t to_obj = argv[2].as_string();
   121	    
   122	    std::string_view from(from_obj->c_str(), from_obj->size());
   123	    std::string_view to(to_obj->c_str(), to_obj->size());
   124	    
   125	    std::string s(self);
   126	    size_t pos = s.find(from);
   127	    if (pos != std::string::npos) {
   128	        s.replace(pos, from.length(), to);
   129	    }
   130	    return Value(vm->get_heap()->new_string(s));
   131	}
   132	
   133	static Value indexOf(Machine* vm, int argc, Value* argv) {
   134	    CHECK_SELF();
   135	    if (argc < 2 || !argv[1].is_string()) return Value((int64_t)-1);
   136	    
   137	    string_t sub_obj = argv[1].as_string();
   138	    size_t start_pos = 0;
   139	    if (argc > 2 && argv[2].is_int()) {
   140	        int64_t p = argv[2].as_int();
   141	        if (p > 0) start_pos = static_cast<size_t>(p);
   142	    }
   143	
   144	    size_t pos = self.find(sub_obj->c_str(), start_pos);
   145	    if (pos == std::string::npos) return Value((int64_t)-1);
   146	    return Value((int64_t)pos);
   147	}
   148	
   149	static Value lastIndexOf(Machine* vm, int argc, Value* argv) {
   150	    CHECK_SELF();
   151	    if (argc < 2 || !argv[1].is_string()) return Value((int64_t)-1);
   152	    
   153	    string_t sub_obj = argv[1].as_string();
   154	    size_t pos = self.rfind(sub_obj->c_str());
   155	    if (pos == std::string::npos) return Value((int64_t)-1);
   156	    return Value((int64_t)pos);
   157	}
   158	
   159	static Value substring(Machine* vm, int argc, Value* argv) {
   160	    CHECK_SELF();
   161	    if (argc < 2 || !argv[1].is_int()) return argv[0];
   162	    
   163	    int64_t start = argv[1].as_int();
   164	    int64_t length = (int64_t)self.size();
   165	    
   166	    if (argc > 2 && argv[2].is_int()) {
   167	        length = argv[2].as_int();
   168	    }
   169	    
   170	    if (start < 0) start = 0;
   171	    if (start >= (int64_t)self.size()) return Value(vm->get_heap()->new_string(""));
   172	    
   173	    return Value(vm->get_heap()->new_string(self.substr(start, length)));
   174	}
   175	
   176	static Value slice_str(Machine* vm, int argc, Value* argv) {
   177	    CHECK_SELF();
   178	    int64_t len = (int64_t)self.size();
   179	    int64_t start = 0;
   180	    int64_t end = len;
   181	
   182	    if (argc >= 2 && argv[1].is_int()) {
   183	        start = argv[1].as_int();
   184	        if (start < 0) start += len;
   185	        if (start < 0) start = 0;
   186	    }
   187	    if (argc >= 3 && argv[2].is_int()) {
   188	        end = argv[2].as_int();
   189	        if (end < 0) end += len;
   190	    }
   191	    if (start >= end || start >= len) return Value(vm->get_heap()->new_string(""));
   192	    if (end > len) end = len;
   193	
   194	    return Value(vm->get_heap()->new_string(self.substr(start, end - start)));
   195	}
   196	
   197	static Value repeat(Machine* vm, int argc, Value* argv) {
   198	    CHECK_SELF();
   199	    if (argc < 2 || !argv[1].is_int()) return Value(vm->get_heap()->new_string(""));
   200	    int64_t count = argv[1].as_int();
   201	    if (count <= 0) return Value(vm->get_heap()->new_string(""));
   202	    
   203	    std::string res;
   204	    res.reserve(self.size() * count);
   205	    for(int i=0; i<count; ++i) res.append(self);
   206	    
   207	    return Value(vm->get_heap()->new_string(res));
   208	}
   209	
   210	static Value padLeft(Machine* vm, int argc, Value* argv) {
   211	    CHECK_SELF();
   212	    if (argc < 2 || !argv[1].is_int()) return argv[0];
   213	    int64_t target_len_i64 = argv[1].as_int();
   214	    if (target_len_i64 < 0) return argv[0];
   215	    size_t target_len = static_cast<size_t>(target_len_i64);
   216	
   217	    if (target_len <= self.size()) return argv[0];
   218	    
   219	    std::string_view pad_char = " ";
   220	    if (argc > 2 && argv[2].is_string()) {
   221	        string_t p = argv[2].as_string();
   222	        if (!p->empty()) pad_char = std::string_view(p->c_str(), p->size());
   223	    }
   224	
   225	    std::string res;
   226	    size_t needed_len = target_len - self.size();
   227	    while (res.size() < needed_len) res.append(pad_char);
   228	    res.resize(needed_len); 
   229	    res.append(self);
   230	    
   231	    return Value(vm->get_heap()->new_string(res));
   232	}
   233	
   234	static Value padRight(Machine* vm, int argc, Value* argv) {
   235	    CHECK_SELF();
   236	    if (argc < 2 || !argv[1].is_int()) return argv[0];
   237	    int64_t target_len_i64 = argv[1].as_int();
   238	    if (target_len_i64 < 0) return argv[0];
   239	    size_t target_len = static_cast<size_t>(target_len_i64);
   240	
   241	    if (target_len <= self.size()) return argv[0];
   242	    
   243	    std::string_view pad_char = " ";
   244	    if (argc > 2 && argv[2].is_string()) {
   245	        string_t p = argv[2].as_string();
   246	        if (!p->empty()) pad_char = std::string_view(p->c_str(), p->size());
   247	    }
   248	
   249	    std::string res(self);
   250	    while (res.size() < target_len) res.append(pad_char);
   251	    res.resize(target_len);
   252	    
   253	    return Value(vm->get_heap()->new_string(res));
   254	}
   255	
   256	static Value equalsIgnoreCase(Machine* vm, int argc, Value* argv) {
   257	    CHECK_SELF();
   258	    if (argc < 2 || !argv[1].is_string()) return Value(false);
   259	    string_t other = argv[1].as_string();
   260	    if (self.size() != other->size()) return Value(false);
   261	    
   262	    return Value(std::equal(self.begin(), self.end(), other->c_str(), 
   263	        [](char a, char b) { return tolower(a) == tolower(b); }));
   264	}
   265	
   266	static Value charAt(Machine* vm, int argc, Value* argv) {
   267	    CHECK_SELF();
   268	    if (argc < 2 || !argv[1].is_int()) return Value(vm->get_heap()->new_string(""));
   269	    int64_t idx = argv[1].as_int();
   270	    if (idx < 0 || idx >= (int64_t)self.size()) return Value(vm->get_heap()->new_string(""));
   271	    
   272	    char c = self[idx];
   273	    return Value(vm->get_heap()->new_string(&c, 1));
   274	}
   275	
   276	static Value charCodeAt(Machine* vm, int argc, Value* argv) {
   277	    CHECK_SELF();
   278	    if (argc < 2 || !argv[1].is_int()) return Value((int64_t)-1);
   279	    int64_t idx = argv[1].as_int();
   280	    if (idx < 0 || idx >= (int64_t)self.size()) return Value((int64_t)-1);
   281	    
   282	    return Value((int64_t)(unsigned char)self[idx]);
   283	}
   284	
   285	} // namespace meow::natives::str
   286	
   287	namespace meow::stdlib {
   288	module_t create_string_module(Machine* vm, MemoryManager* heap) noexcept {
   289	    auto name = heap->new_string("string");
   290	    auto mod = heap->new_module(name, name);
   291	    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };
   292	
   293	    using namespace meow::natives::str;
   294	    reg("len", len);
   295	    reg("size", len);
   296	    reg("length", len);
   297	    
   298	    reg("upper", upper);
   299	    reg("lower", lower);
   300	    reg("trim", trim);
   301	    
   302	    reg("contains", contains);
   303	    reg("startsWith", startsWith);
   304	    reg("endsWith", endsWith);
   305	    reg("join", join);
   306	    reg("split", split);
   307	    reg("replace", replace);
   308	    reg("indexOf", indexOf);
   309	    reg("lastIndexOf", lastIndexOf);
   310	    reg("substring", substring);
   311	    reg("slice", slice_str);
   312	    reg("repeat", repeat);
   313	    reg("padLeft", padLeft);
   314	    reg("padRight", padRight);
   315	    reg("equalsIgnoreCase", equalsIgnoreCase);
   316	    reg("charAt", charAt);
   317	    reg("charCodeAt", charCodeAt);
   318	    
   319	    return mod;
   320	}
   321	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/system_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/cast.h>
     7	#include <meow/core/module.h>
     8	
     9	namespace meow::natives::sys {
    10	
    11	// system.argv()
    12	static Value get_argv(Machine* vm, int, Value*) {
    13	    const auto& cmd_args = vm->get_args().command_line_arguments_; 
    14	    auto arr = vm->get_heap()->new_array();
    15	    arr->reserve(cmd_args.size());
    16	    
    17	    for (const auto& arg : cmd_args) {
    18	        arr->push(Value(vm->get_heap()->new_string(arg)));
    19	    }
    20	    return Value(arr);
    21	}
    22	
    23	// system.exit(code)
    24	static Value exit_vm(Machine*, int argc, Value* argv) {
    25	    int code = 0;
    26	    if (argc > 0) code = static_cast<int>(to_int(argv[0]));
    27	    std::exit(code);
    28	    std::unreachable();
    29	}
    30	
    31	// system.exec(command)
    32	static Value exec_cmd(Machine* vm, int argc, Value* argv) {
    33	    if (argc < 1) [[unlikely]] return Value(static_cast<int64_t>(-1));
    34	    const char* cmd = argv[0].as_string()->c_str();
    35	    int code = std::system(cmd);
    36	    return Value(static_cast<int64_t>(code));
    37	}
    38	
    39	// system.time() -> ms
    40	static Value time_now(Machine*, int, Value*) {
    41	    auto now = std::chrono::system_clock::now();
    42	    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    43	    return Value(static_cast<int64_t>(ms));
    44	}
    45	
    46	// system.env(name)
    47	static Value get_env(Machine* vm, int argc, Value* argv) {
    48	    if (argc < 1) [[unlikely]] return Value(null_t{});
    49	    const char* val = std::getenv(argv[0].as_string()->c_str());
    50	    if (val) return Value(vm->get_heap()->new_string(val));
    51	    return Value(null_t{});
    52	}
    53	
    54	} // namespace meow::natives::sys
    55	
    56	namespace meow::stdlib {
    57	
    58	module_t create_system_module(Machine* vm, MemoryManager* heap) noexcept {
    59	    auto name = heap->new_string("system");
    60	    auto mod = heap->new_module(name, name);
    61	
    62	    auto reg = [&](const char* n, native_t fn) {
    63	        mod->set_export(heap->new_string(n), Value(fn));
    64	    };
    65	
    66	    using namespace meow::natives::sys;
    67	    reg("argv", get_argv);
    68	    reg("exit", exit_vm);
    69	    reg("exec", exec_cmd);
    70	    reg("time", time_now);
    71	    reg("env", get_env);
    72	
    73	    return mod;
    74	}
    75	
    76	} // namespace meow::stdlib


// =============================================================================
//  FILE PATH: src/vm/vm_state.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/common.h>
     5	#include "runtime/execution_context.h"
     6	#include "runtime/call_frame.h"
     7	#include <meow/bytecode/chunk.h>
     8	#include <meow/core/function.h>
     9	#include <meow/bytecode/disassemble.h>
    10	
    11	namespace meow {
    12	struct ExecutionContext;
    13	class MemoryManager;
    14	class ModuleManager;
    15	class Machine;
    16	}
    17	
    18	namespace meow {
    19	struct VMState {
    20	    Machine& machine;
    21	    ExecutionContext& ctx;
    22	    MemoryManager& heap;
    23	    ModuleManager& modules;
    24	
    25	    Value* registers;       
    26	    const Value* constants;       
    27	    const uint8_t* instruction_base;
    28	    module_t current_module = nullptr;
    29	
    30	    std::string error_msg;
    31	    bool has_error_ = false;
    32	
    33	    void error(std::string_view msg) noexcept {
    34	        std::cerr << "Runtime Error: " << msg << "\n";
    35	        error_msg = msg;
    36	        has_error_ = true;
    37	        if (ctx.current_frame_ && ctx.current_frame_->function_) {
    38	            const auto& chunk = ctx.current_frame_->function_->get_proto()->get_chunk();
    39	            size_t ip = ctx.current_frame_->ip_ - chunk.get_code();
    40	            std::cerr << disassemble_around(chunk, ip, 3);
    41	        }
    42	    }
    43	    bool has_error() const noexcept { return has_error_; }
    44	    void clear_error() noexcept { has_error_ = false; error_msg.clear(); }
    45	    std::string_view get_error_message() const noexcept { return error_msg; }
    46	
    47	    [[gnu::always_inline]]
    48	    inline void update_pointers() noexcept {
    49	        registers = ctx.current_regs_;
    50	        
    51	        auto proto = ctx.frame_ptr_->function_->get_proto();
    52	        
    53	        constants = proto->get_chunk().get_constants_raw();
    54	        instruction_base = proto->get_chunk().get_code();
    55	        
    56	        current_module = proto->get_module();
    57	    }
    58	
    59	    [[gnu::always_inline]] 
    60	    inline Value& reg(uint16_t idx) noexcept {
    61	        return registers[idx];
    62	    }
    63	};
    64	}


