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
     8	// Mã lỗi giả định (Bạn nên move vào enum chung)
     9	constexpr int ERR_TYPE = 10;
    10	constexpr int ERR_BOUNDS = 11;
    11	constexpr int ERR_READ_ONLY = 12;
    12	
    13	// =========================================================
    14	// STANDARD 16-BIT REGISTER OPERATIONS
    15	// =========================================================
    16	
    17	[[gnu::always_inline]] 
    18	static const uint8_t* impl_LOAD_CONST(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    19	    // 2 * u16 = 4 bytes -> Tự động load u32
    20	    auto [dst, idx] = decode::args<u16, u16>(ip);
    21	    regs[dst] = constants[idx];
    22	    return ip;
    23	}
    24	
    25	[[gnu::always_inline]] 
    26	static const uint8_t* impl_LOAD_NULL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    27	    auto [dst] = decode::args<u16>(ip);
    28	    regs[dst] = null_t{};
    29	    return ip;
    30	}
    31	
    32	[[gnu::always_inline]] 
    33	static const uint8_t* impl_LOAD_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    34	    auto [dst] = decode::args<u16>(ip);
    35	    regs[dst] = true;
    36	    return ip;
    37	}
    38	
    39	[[gnu::always_inline]] 
    40	static const uint8_t* impl_LOAD_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    41	    auto [dst] = decode::args<u16>(ip);
    42	    regs[dst] = false;
    43	    return ip;
    44	}
    45	
    46	[[gnu::always_inline]] 
    47	static const uint8_t* impl_LOAD_INT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    48	    // Tổng > 8 bytes -> Decoder tự động fallback đọc tuần tự (an toàn alignment)
    49	    auto [dst, val] = decode::args<u16, i64>(ip);
    50	    regs[dst] = val;
    51	    return ip;
    52	}
    53	
    54	[[gnu::always_inline]] 
    55	static const uint8_t* impl_LOAD_FLOAT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    56	    auto [dst, val] = decode::args<u16, f64>(ip);
    57	    regs[dst] = val;
    58	    return ip;
    59	}
    60	
    61	[[gnu::always_inline]] 
    62	static const uint8_t* impl_MOVE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    63	    // 2 * u16 = 4 bytes -> Load u32
    64	    auto [dst, src] = decode::args<u16, u16>(ip);
    65	    regs[dst] = regs[src];
    66	    return ip;
    67	}
    68	
    69	[[gnu::always_inline]] 
    70	static const uint8_t* impl_NEW_ARRAY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    71	    // 3 * u16 = 6 bytes -> Load u64 (đọc lố padding)
    72	    auto [dst, start_idx, count] = decode::args<u16, u16, u16>(ip);
    73	
    74	    auto array = state->heap.new_array();
    75	    regs[dst] = object_t(array);
    76	    array->reserve(count);
    77	    
    78	    // Unroll loop nhẹ nếu cần, hoặc để compiler lo
    79	    for (size_t i = 0; i < count; ++i) {
    80	        array->push(regs[start_idx + i]);
    81	    }
    82	    return ip;
    83	}
    84	
    85	[[gnu::always_inline]] 
    86	static const uint8_t* impl_NEW_HASH(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    87	    // 3 * u16 = 6 bytes -> Load u64
    88	    auto [dst, start_idx, count] = decode::args<u16, u16, u16>(ip);
    89	    
    90	    auto hash = state->heap.new_hash(count); 
    91	    regs[dst] = Value(hash); 
    92	
    93	    for (size_t i = 0; i < count; ++i) {
    94	        Value& key = regs[start_idx + i * 2];
    95	        Value& val = regs[start_idx + i * 2 + 1];
    96	        
    97	        if (key.is_string()) [[likely]] {
    98	            hash->set(key.as_string(), val);
    99	        } else {
   100	            std::string s = to_string(key);
   101	            string_t k = state->heap.new_string(s);
   102	            hash->set(k, val);
   103	        }
   104	    }
   105	    return ip;
   106	}
   107	
   108	[[gnu::always_inline]] 
   109	static const uint8_t* impl_GET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   110	    // 3 * u16 = 6 bytes -> Load u64
   111	    auto [dst, src_reg, key_reg] = decode::args<u16, u16, u16>(ip);
   112	    
   113	    Value& src = regs[src_reg];
   114	    Value& key = regs[key_reg];
   115	
   116	    if (src.is_array()) {
   117	        if (!key.is_int()) {
   118	            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Array index must be integer");
   119	        }
   120	        array_t arr = src.as_array();
   121	        int64_t idx = key.as_int();
   122	
   123	        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
   124	            regs[dst] = null_t{};
   125	        } else {
   126	            regs[dst] = arr->get(idx);
   127	        }
   128	    } 
   129	    else if (src.is_hash_table()) {
   130	        hash_table_t hash = src.as_hash_table();
   131	        string_t k = nullptr;
   132	        
   133	        if (!key.is_string()) {
   134	            std::string s = to_string(key);
   135	            k = state->heap.new_string(s);
   136	        } else {
   137	            k = key.as_string();
   138	        }
   139	
   140	        if (hash->has(k)) {
   141	            regs[dst] = hash->get(k);
   142	        } else {
   143	            regs[dst] = Value(null_t{});
   144	        }
   145	    }
   146	    else if (src.is_string()) {
   147	        if (!key.is_int()) {
   148	            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "String index must be integer");
   149	        }
   150	        string_t str = src.as_string();
   151	        int64_t idx = key.as_int();
   152	        if (idx < 0 || static_cast<size_t>(idx) >= str->size()) {
   153	            return ERROR<6>(ip, regs, constants, state, ERR_BOUNDS, "String index out of bounds");
   154	        }
   155	        char c = str->get(idx);
   156	        regs[dst] = Value(state->heap.new_string(&c, 1));
   157	    }
   158	    else if (src.is_instance()) {
   159	        if (!key.is_string()) {
   160	            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Instance index key must be string");
   161	        }
   162	        
   163	        string_t name = key.as_string();
   164	        instance_t inst = src.as_instance();
   165	        
   166	        int offset = inst->get_shape()->get_offset(name);
   167	        if (offset != -1) {
   168	            regs[dst] = inst->get_field_at(offset);
   169	        } 
   170	        else {
   171	            // Fallback to method lookup
   172	            class_t k = inst->get_class();
   173	            Value method = null_t{};
   174	            while (k) {
   175	                if (k->has_method(name)) {
   176	                    method = k->get_method(name);
   177	                    break;
   178	                }
   179	                k = k->get_super();
   180	            }
   181	            if (!method.is_null()) {
   182	                if (method.is_function() || method.is_native()) {
   183	                    auto bound = state->heap.new_bound_method(src, method);
   184	                    regs[dst] = Value(bound);
   185	                } else {
   186	                    regs[dst] = method;
   187	                }
   188	            } else {
   189	                regs[dst] = Value(null_t{});
   190	            }
   191	        }
   192	    }
   193	    else {
   194	        return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Cannot index on type {}", to_string(src));
   195	    }
   196	    return ip;
   197	}
   198	
   199	[[gnu::always_inline]] 
   200	static const uint8_t* impl_SET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   201	    // 3 * u16 = 6 bytes -> Load u64
   202	    auto [src_reg, key_reg, val_reg] = decode::args<u16, u16, u16>(ip);
   203	
   204	    Value& src = regs[src_reg];
   205	    Value& key = regs[key_reg];
   206	    Value& val = regs[val_reg];
   207	
   208	    if (src.is_array()) {
   209	        if (!key.is_int()) {
   210	            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Array index must be integer");
   211	        }
   212	        array_t arr = src.as_array();
   213	        int64_t idx = key.as_int();
   214	        if (idx < 0) {
   215	            return ERROR<6>(ip, regs, constants, state, ERR_BOUNDS, "Array index cannot be negative");
   216	        }
   217	        // Auto resize functionality
   218	        if (static_cast<size_t>(idx) >= arr->size()) {
   219	            arr->resize(idx + 1);
   220	        }
   221	        arr->set(idx, val);
   222	        state->heap.write_barrier(src.as_object(), val);
   223	    }
   224	    else if (src.is_hash_table()) {
   225	        hash_table_t hash = src.as_hash_table();
   226	        string_t k = nullptr;
   227	        if (!key.is_string()) {
   228	            std::string s = to_string(key);
   229	            k = state->heap.new_string(s);
   230	        } else {
   231	            k = key.as_string();
   232	        }
   233	        hash->set(k, val);
   234	        state->heap.write_barrier(src.as_object(), val);
   235	    } 
   236	    else if (src.is_instance()) {
   237	        if (!key.is_string()) {
   238	            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Instance key must be string");
   239	        }
   240	        string_t name = key.as_string();
   241	        instance_t inst = src.as_instance();
   242	        
   243	        int offset = inst->get_shape()->get_offset(name);
   244	        if (offset != -1) {
   245	            inst->set_field_at(offset, val);
   246	            state->heap.write_barrier(inst, val);
   247	        } else {
   248	            // Shape Transition (Poly/Morphism support)
   249	            Shape* current_shape = inst->get_shape();
   250	            Shape* next_shape = current_shape->get_transition(name);
   251	            if (next_shape == nullptr) {
   252	                next_shape = current_shape->add_transition(name, &state->heap);
   253	            }
   254	            inst->set_shape(next_shape);
   255	            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape)));
   256	            inst->add_field(val);
   257	            state->heap.write_barrier(inst, val);
   258	        }
   259	    }
   260	    else {
   261	        return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Cannot set index on this type");
   262	    }
   263	    return ip;
   264	}
   265	
   266	[[gnu::always_inline]] 
   267	static const uint8_t* impl_GET_KEYS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   268	    // 2 * u16 = 4 bytes -> Load u32
   269	    auto [dst, src_reg] = decode::args<u16, u16>(ip);
   270	    Value& src = regs[src_reg];
   271	    
   272	    auto keys_array = state->heap.new_array();
   273	    
   274	    if (src.is_hash_table()) {
   275	        hash_table_t hash = src.as_hash_table();
   276	        keys_array->reserve(hash->size());
   277	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   278	            keys_array->push(Value(it->first));
   279	        }
   280	    } else if (src.is_array()) {
   281	        size_t sz = src.as_array()->size();
   282	        keys_array->reserve(sz);
   283	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   284	    } else if (src.is_string()) {
   285	        size_t sz = src.as_string()->size();
   286	        keys_array->reserve(sz);
   287	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   288	    }
   289	    
   290	    regs[dst] = Value(keys_array);
   291	    return ip;
   292	}
   293	
   294	[[gnu::always_inline]] 
   295	static const uint8_t* impl_GET_VALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   296	    auto [dst, src_reg] = decode::args<u16, u16>(ip);
   297	    Value& src = regs[src_reg];
   298	    
   299	    auto vals_array = state->heap.new_array();
   300	
   301	    if (src.is_hash_table()) {
   302	        hash_table_t hash = src.as_hash_table();
   303	        vals_array->reserve(hash->size());
   304	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   305	            vals_array->push(it->second);
   306	        }
   307	    } else if (src.is_array()) {
   308	        array_t arr = src.as_array();
   309	        vals_array->reserve(arr->size());
   310	        for (size_t i = 0; i < arr->size(); ++i) vals_array->push(arr->get(i));
   311	    } else if (src.is_string()) {
   312	        string_t str = src.as_string();
   313	        vals_array->reserve(str->size());
   314	        for (size_t i = 0; i < str->size(); ++i) {
   315	            char c = str->get(i);
   316	            vals_array->push(Value(state->heap.new_string(&c, 1)));
   317	        }
   318	    }
   319	
   320	    regs[dst] = Value(vals_array);
   321	    return ip;
   322	}
   323	
   324	// =========================================================
   325	// OPTIMIZED 8-BIT REGISTER OPERATIONS (_B VARIANTS)
   326	// =========================================================
   327	
   328	[[gnu::always_inline]] 
   329	static const uint8_t* impl_MOVE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   330	    // 2 * u8 = 2 bytes -> Load u16
   331	    auto [dst, src] = decode::args<u8, u8>(ip);
   332	    regs[dst] = regs[src];
   333	    return ip;
   334	}
   335	
   336	[[gnu::always_inline]] 
   337	static const uint8_t* impl_LOAD_CONST_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   338	    // u8 + u16 = 3 bytes -> Load u32 (tận dụng padding)
   339	    auto [dst, idx] = decode::args<u8, u16>(ip);
   340	    regs[dst] = constants[idx];
   341	    return ip;
   342	}
   343	
   344	[[gnu::always_inline]] 
   345	static const uint8_t* impl_LOAD_NULL_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   346	    auto [dst] = decode::args<u8>(ip);
   347	    regs[dst] = null_t{};
   348	    return ip;
   349	}
   350	
   351	[[gnu::always_inline]] 
   352	static const uint8_t* impl_LOAD_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   353	    auto [dst] = decode::args<u8>(ip);
   354	    regs[dst] = true;
   355	    return ip;
   356	}
   357	
   358	[[gnu::always_inline]] 
   359	static const uint8_t* impl_LOAD_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   360	    auto [dst] = decode::args<u8>(ip);
   361	    regs[dst] = false;
   362	    return ip;
   363	}
   364	
   365	[[gnu::always_inline]] 
   366	static const uint8_t* impl_LOAD_INT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   367	    // Fallback tuần tự: u8 (1b) + i64 (8b)
   368	    auto [dst, val] = decode::args<u8, i64>(ip);
   369	    regs[dst] = val;
   370	    return ip;
   371	}
   372	
   373	[[gnu::always_inline]] 
   374	static const uint8_t* impl_LOAD_FLOAT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   375	    auto [dst, val] = decode::args<u8, f64>(ip);
   376	    regs[dst] = val;
   377	    return ip;
   378	}
   379	
   380	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/exception_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	
     5	namespace meow::handlers {
     6	
     7	// Mã lỗi giả định cho Exception
     8	constexpr int ERR_RUNTIME = 99;
     9	
    10	[[gnu::always_inline]] 
    11	static const uint8_t* impl_THROW(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    12	    // 1 * u16 = 2 bytes -> Load u16 (hoặc u32 tùy implement decode, nhưng u16 đủ nhanh)
    13	    auto [reg] = decode::args<u16>(ip);
    14	    
    15	    Value& val = regs[reg];
    16	    
    17	    // Sử dụng ERROR<2> để trỏ đúng về lệnh THROW
    18	    // Format string: "{}" để in nội dung exception
    19	    return ERROR<2>(ip, regs, constants, state, ERR_RUNTIME, "Uncaught Exception: {}", to_string(val));
    20	}
    21	
    22	[[gnu::always_inline]] 
    23	static const uint8_t* impl_SETUP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    24	    // 2 * u16 = 4 bytes -> Load u32
    25	    auto [offset, err_reg] = decode::args<u16, u16>(ip);
    26	    
    27	    size_t frame_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
    28	    size_t stack_depth = state->ctx.stack_top_ - state->ctx.stack_;
    29	    
    30	    size_t catch_ip_abs = offset; 
    31	    
    32	    state->ctx.exception_handlers_.emplace_back(catch_ip_abs, frame_depth, stack_depth, err_reg);
    33	    return ip;
    34	}
    35	
    36	[[gnu::always_inline]] 
    37	static const uint8_t* impl_POP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    38	    if (!state->ctx.exception_handlers_.empty()) {
    39	        state->ctx.exception_handlers_.pop_back();
    40	    }
    41	    return ip;
    42	}
    43	
    44	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/flow_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include <meow/core/objects.h>
     4	#include <meow/machine.h>
     5	#include <cstring>
     6	#include <vector>
     7	
     8	namespace meow::handlers {
     9	
    10	    // Giữ lại CallIC vì nó được patch trực tiếp vào bytecode khi chạy
    11	    struct CallIC {
    12	        void* check_tag;
    13	        void* destination;
    14	    } __attribute__((packed)); 
    15	
    16	    // Helper: Push Stack Frame (Giữ nguyên logic nhưng cleanup code)
    17	    [[gnu::always_inline]]
    18	    inline static const uint8_t* push_call_frame(
    19	        VMState* state, 
    20	        function_t closure, 
    21	        int argc, 
    22	        Value* args_src,       
    23	        Value* receiver,       
    24	        Value* ret_dest,       
    25	        const uint8_t* ret_ip, 
    26	        const uint8_t* err_ip  
    27	    ) {
    28	        proto_t proto = closure->get_proto();
    29	        size_t num_params = proto->get_num_registers();
    30	
    31	        // Check Stack Overflow
    32	        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
    33	            state->error("Stack Overflow!", err_ip); 
    34	            return nullptr;
    35	        }
    36	
    37	        Value* new_base = state->ctx.stack_top_;
    38	        size_t arg_offset = 0;
    39	
    40	        // Setup Receiver (this)
    41	        if (receiver != nullptr && num_params > 0) {
    42	            new_base[0] = *receiver;
    43	            arg_offset = 1; 
    44	        }
    45	
    46	        // Copy Arguments
    47	        size_t copy_count = (argc < (num_params - arg_offset)) ? argc : (num_params - arg_offset);
    48	        if (copy_count > 0) {
    49	            // Dùng memcpy hoặc loop tùy compiler optimize, ở đây loop cho an toàn type
    50	            for (size_t i = 0; i < copy_count; ++i) {
    51	                new_base[arg_offset + i] = args_src[i];
    52	            }
    53	        }
    54	
    55	        // Fill Null cho các param còn thiếu
    56	        size_t filled = arg_offset + argc;
    57	        for (size_t i = filled; i < num_params; ++i) {
    58	            new_base[i] = Value(null_t{});
    59	        }
    60	
    61	        // Push Frame
    62	        state->ctx.frame_ptr_++;
    63	        *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest, ret_ip);
    64	        
    65	        // Update State Pointers
    66	        state->ctx.current_regs_ = new_base;
    67	        state->ctx.stack_top_ += num_params;
    68	        state->ctx.current_frame_ = state->ctx.frame_ptr_;
    69	        state->update_pointers(); 
    70	
    71	        // Jump to function code
    72	        return state->instruction_base; 
    73	    }
    74	
    75	    // --- BASIC OPS ---
    76	
    77	    [[gnu::always_inline]]
    78	    inline static const uint8_t* impl_NOP(const uint8_t* ip, Value*, const Value*, VMState*) { return ip; }
    79	
    80	    [[gnu::always_inline]]
    81	    inline static const uint8_t* impl_HALT(const uint8_t*, Value*, const Value*, VMState*) { return nullptr; }
    82	
    83	    // --- PANIC HANDLER (Exception Unwinding) ---
    84	    [[gnu::always_inline]]
    85	    inline const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    86	        (void)ip; (void)regs; (void)constants;
    87	        
    88	        // Nếu có Exception Handler (Try-Catch)
    89	        if (!state->ctx.exception_handlers_.empty()) {
    90	            auto& handler = state->ctx.exception_handlers_.back();
    91	            
    92	            // Unwind Stack Frames
    93	            long current_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
    94	            while (current_depth > (long)handler.frame_depth_) {
    95	                size_t reg_idx = state->ctx.frame_ptr_->regs_base_ - state->ctx.stack_;
    96	                meow::close_upvalues(state->ctx, reg_idx);
    97	                state->ctx.frame_ptr_--;
    98	                current_depth--;
    99	            }
   100	            
   101	            // Restore State
   102	            state->ctx.stack_top_ = state->ctx.stack_ + handler.stack_depth_;
   103	            state->ctx.current_regs_ = state->ctx.frame_ptr_->regs_base_;
   104	            state->ctx.current_frame_ = state->ctx.frame_ptr_; 
   105	            state->update_pointers();
   106	            
   107	            const uint8_t* catch_ip = state->instruction_base + handler.catch_ip_;
   108	            
   109	            // Push Error Object
   110	            if (handler.error_reg_ != static_cast<size_t>(-1)) {
   111	                auto err_str = state->heap.new_string(state->get_error_message());
   112	                regs[handler.error_reg_] = Value(err_str);
   113	            }
   114	            
   115	            state->clear_error();
   116	            state->ctx.exception_handlers_.pop_back();
   117	            return catch_ip;
   118	        } 
   119	        
   120	        // Crash nếu không bắt được lỗi
   121	        std::cerr << "VM Panic: " << state->get_error_message() << "\n";
   122	        return nullptr; 
   123	    }
   124	
   125	    [[gnu::always_inline]]
   126	    inline static const uint8_t* impl_UNIMPL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   127	        state->error("Opcode chưa được hỗ trợ (UNIMPL)", ip);
   128	        return impl_PANIC(ip, regs, constants, state);
   129	    }
   130	
   131	    // --- JUMP OPS ---
   132	
   133	    [[gnu::always_inline]] 
   134	    inline static const uint8_t* impl_JUMP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   135	        // Load i16 -> IP đã tự tăng 2 byte
   136	        auto [offset] = decode::args<i16>(ip);
   137	        return ip + offset; 
   138	    }
   139	
   140	    [[gnu::always_inline]] 
   141	    inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   142	        // 2 * u16 = 4 bytes -> Load u32
   143	        auto [cond_idx, offset] = decode::args<u16, i16>(ip);
   144	        
   145	        Value& cond = regs[cond_idx];
   146	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   147	        
   148	        if (truthy) return ip + offset;
   149	        return ip;
   150	    }
   151	
   152	    [[gnu::always_inline]] 
   153	    inline static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   154	        auto [cond_idx, offset] = decode::args<u16, i16>(ip);
   155	        
   156	        Value& cond = regs[cond_idx];
   157	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   158	        
   159	        if (!truthy) return ip + offset;
   160	        return ip;
   161	    }
   162	
   163	    // --- BYTE JUMPS (_B) ---
   164	
   165	    [[gnu::always_inline, gnu::hot]]
   166	    inline static const uint8_t* impl_JUMP_IF_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   167	        // u8 + i16 = 3 bytes -> Load u32 (tận dụng padding)
   168	        auto [cond_idx, offset] = decode::args<u8, i16>(ip);
   169	        
   170	        Value& cond = regs[cond_idx];
   171	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   172	        
   173	        if (truthy) return ip + offset;
   174	        return ip;
   175	    }
   176	
   177	    [[gnu::always_inline]] 
   178	    inline static const uint8_t* impl_JUMP_IF_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   179	        auto [cond_idx, offset] = decode::args<u8, i16>(ip);
   180	        
   181	        Value& cond = regs[cond_idx];
   182	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   183	        
   184	        if (!truthy) return ip + offset;
   185	        return ip;
   186	    }
   187	
   188	    // --- RETURN ---
   189	
   190	    [[gnu::always_inline]]
   191	    inline static const uint8_t* impl_RETURN(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   192	        auto [ret_reg_idx] = decode::args<u16>(ip);
   193	        
   194	        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : regs[ret_reg_idx];
   195	
   196	        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
   197	        meow::close_upvalues(state->ctx, base_idx);
   198	
   199	        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) [[unlikely]] return nullptr; 
   200	
   201	        CallFrame* popped_frame = state->ctx.frame_ptr_;
   202	        
   203	        // Module Execution Flag
   204	        if (state->current_module) [[likely]] {
   205	             if (popped_frame->function_->get_proto() == state->current_module->get_main_proto()) [[unlikely]] {
   206	                 state->current_module->set_executed();
   207	             }
   208	        }
   209	
   210	        state->ctx.frame_ptr_--;
   211	        CallFrame* caller = state->ctx.frame_ptr_;
   212	        
   213	        state->ctx.stack_top_ = popped_frame->regs_base_;
   214	        state->ctx.current_regs_ = caller->regs_base_;
   215	        state->ctx.current_frame_ = caller; 
   216	        state->update_pointers(); 
   217	
   218	        if (popped_frame->ret_dest_ != nullptr) *popped_frame->ret_dest_ = result;
   219	        return popped_frame->ip_; 
   220	    }
   221	
   222	    // --- CALL INFRASTRUCTURE ---
   223	
   224	template <bool IsVoid>
   225	    [[gnu::always_inline]] 
   226	    static inline const uint8_t* do_call(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   227	        u16 dst = 0xFFFF;
   228	        u16 fn_reg, arg_start, argc;
   229	
   230	        // 1. Decode Arguments
   231	        if constexpr (!IsVoid) {
   232	            auto [d, f, s, c] = decode::args<u16, u16, u16, u16>(ip);
   233	            dst       = d;
   234	            fn_reg    = f;
   235	            arg_start = s;
   236	            argc      = c;
   237	        } else {
   238	            auto [f, s, c] = decode::args<u16, u16, u16>(ip);
   239	            fn_reg    = f;
   240	            arg_start = s;
   241	            argc      = c;
   242	        }
   243	
   244	        // 2. Decode Inline Cache
   245	        const CallIC* ic = decode::as_struct<CallIC>(ip);
   246	        
   247	        // Tính offset để báo lỗi chính xác
   248	        constexpr size_t ErrOffset = (IsVoid ? 6 : 8) + 16;
   249	
   250	        Value& callee = regs[fn_reg];
   251	        function_t closure = nullptr;
   252	
   253	        // [SỬA LỖI Ở ĐÂY]: Tách logic constexpr ra khỏi dòng khởi tạo
   254	        Value* ret_dest_ptr = nullptr;
   255	        if constexpr (!IsVoid) {
   256	            if (dst != 0xFFFF) {
   257	                ret_dest_ptr = &regs[dst];
   258	            }
   259	        }
   260	
   261	        // --- A. Function Call ---
   262	        if (callee.is_function()) [[likely]] {
   263	            closure = callee.as_function();
   264	            if (ic->check_tag == closure->get_proto()) [[likely]] goto SETUP_FRAME;
   265	            const_cast<CallIC*>(ic)->check_tag = closure->get_proto();
   266	            goto SETUP_FRAME;
   267	        }
   268	
   269	        // --- B. Native Call ---
   270	        if (callee.is_native()) {
   271	            native_t fn = callee.as_native();
   272	            if (ic->check_tag != (void*)fn) const_cast<CallIC*>(ic)->check_tag = (void*)fn;
   273	            
   274	            Value result = fn(&state->machine, argc, &regs[arg_start]);
   275	            
   276	            if (state->machine.has_error()) [[unlikely]] {
   277	                state->error(std::string(state->machine.get_error_message()), ip - ErrOffset - 1);
   278	                state->machine.clear_error();
   279	                return impl_PANIC(ip, regs, constants, state);
   280	            }
   281	            if constexpr (!IsVoid) {
   282	                if (dst != 0xFFFF) regs[dst] = result;
   283	            }
   284	            return ip;
   285	        }
   286	
   287	        // --- C. Bound Method ---
   288	        if (callee.is_bound_method()) {
   289	            bound_method_t bound = callee.as_bound_method();
   290	            Value method = bound->get_method();
   291	            Value receiver = bound->get_receiver();
   292	
   293	            if (method.is_function()) {
   294	                closure = method.as_function();
   295	                proto_t proto = closure->get_proto();
   296	                size_t num_params = proto->get_num_registers();
   297	
   298	                if (!state->ctx.check_frame_overflow()) 
   299	                     return ERROR<ErrOffset>(ip, regs, constants, state, 90, "Stack Overflow");
   300	
   301	                Value* new_base = state->ctx.stack_top_;
   302	                
   303	                new_base[0] = receiver; 
   304	                size_t safe_argc = static_cast<size_t>(argc);
   305	                size_t copy_cnt = (safe_argc < num_params - 1) ? safe_argc : num_params - 1;
   306	                
   307	                if (copy_cnt > 0) std::memcpy((void*)(new_base + 1), &regs[arg_start], copy_cnt * sizeof(Value));
   308	                
   309	                size_t filled = 1 + copy_cnt;
   310	                for (size_t i = filled; i < num_params; ++i) new_base[i] = Value(null_t{});
   311	
   312	                state->ctx.frame_ptr_++;
   313	                *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest_ptr, ip);
   314	                
   315	                state->ctx.current_regs_ = new_base;
   316	                state->ctx.stack_top_ += num_params;
   317	                state->ctx.current_frame_ = state->ctx.frame_ptr_;
   318	                state->update_pointers(); 
   319	                return state->instruction_base;
   320	            }
   321	            
   322	            if (method.is_native()) {
   323	                native_t fn = method.as_native();
   324	                constexpr size_t STATIC_ARG_LIMIT = 64;
   325	                Value stack_args[STATIC_ARG_LIMIT];
   326	                Value* arg_ptr = stack_args;
   327	                
   328	                std::vector<Value> heap_args;
   329	                if (argc + 1 > STATIC_ARG_LIMIT) {
   330	                        heap_args.resize(argc + 1);
   331	                        arg_ptr = heap_args.data();
   332	                }
   333	                arg_ptr[0] = receiver;
   334	                if (argc > 0) std::memcpy((void*)(arg_ptr + 1), &regs[arg_start], argc * sizeof(Value));
   335	
   336	                Value result = fn(&state->machine, argc + 1, arg_ptr);
   337	                
   338	                if (state->machine.has_error()) return impl_PANIC(ip, regs, constants, state);
   339	                if constexpr (!IsVoid) if (dst != 0xFFFF) regs[dst] = result;
   340	                return ip;
   341	            }
   342	        }
   343	        
   344	        // --- D. Class Constructor ---
   345	        else if (callee.is_class()) {
   346	            class_t klass = callee.as_class();
   347	            instance_t self = state->heap.new_instance(klass, state->heap.get_empty_shape());
   348	            if (ret_dest_ptr) *ret_dest_ptr = Value(self);
   349	            
   350	            Value init_method = klass->get_method(state->heap.new_string("init"));
   351	            if (init_method.is_function()) {
   352	                closure = init_method.as_function();
   353	                proto_t proto = closure->get_proto();
   354	                size_t num_params = proto->get_num_registers();
   355	                
   356	                if (!state->ctx.check_frame_overflow()) 
   357	                    return ERROR<ErrOffset>(ip, regs, constants, state, 90, "Stack Overflow");
   358	                
   359	                Value* new_base = state->ctx.stack_top_;
   360	                new_base[0] = Value(self);
   361	                
   362	                size_t safe_argc = static_cast<size_t>(argc);
   363	                size_t copy_cnt = (safe_argc < num_params - 1) ? safe_argc : num_params - 1;
   364	                
   365	                if (copy_cnt > 0) std::memcpy((void*)(new_base + 1), &regs[arg_start], copy_cnt * sizeof(Value));
   366	                
   367	                for (size_t i = 1 + copy_cnt; i < num_params; ++i) new_base[i] = Value(null_t{});
   368	                
   369	                state->ctx.frame_ptr_++;
   370	                *state->ctx.frame_ptr_ = CallFrame(closure, new_base, nullptr, ip);
   371	                state->ctx.current_regs_ = new_base;
   372	                state->ctx.stack_top_ += num_params;
   373	                state->ctx.current_frame_ = state->ctx.frame_ptr_;
   374	                state->update_pointers();
   375	                return state->instruction_base;
   376	            } 
   377	            return ip;
   378	        }
   379	
   380	        return ERROR<ErrOffset>(ip, regs, constants, state, 91, "Value '{}' is not callable.", to_string(callee));
   381	
   382	    SETUP_FRAME:
   383	        {
   384	            proto_t proto = closure->get_proto();
   385	            size_t num_params = proto->get_num_registers();
   386	
   387	            if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
   388	                return ERROR<ErrOffset>(ip, regs, constants, state, 90, "Stack Overflow");
   389	            }
   390	
   391	            Value* new_base = state->ctx.stack_top_;
   392	            
   393	            size_t safe_argc = static_cast<size_t>(argc);
   394	            size_t copy_count = (safe_argc < num_params) ? safe_argc : num_params;
   395	            
   396	            if (copy_count > 0) {
   397	                std::memcpy((void*)new_base, &regs[arg_start], copy_count * sizeof(Value));
   398	            }
   399	
   400	            for (size_t i = copy_count; i < num_params; ++i) {
   401	                new_base[i] = Value(null_t{});
   402	            }
   403	
   404	            state->ctx.frame_ptr_++;
   405	            *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest_ptr, ip);
   406	            
   407	            state->ctx.current_regs_ = new_base;
   408	            state->ctx.stack_top_ += num_params;
   409	            state->ctx.current_frame_ = state->ctx.frame_ptr_;
   410	            state->update_pointers(); 
   411	
   412	            return state->instruction_base;
   413	        }
   414	    }
   415	    
   416	    // --- WRAPPERS ---
   417	
   418	    [[gnu::always_inline]] 
   419	    inline static const uint8_t* impl_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   420	        return do_call<false>(ip, regs, constants, state);
   421	    }
   422	
   423	    [[gnu::always_inline]] 
   424	    inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   425	        return do_call<true>(ip, regs, constants, state);
   426	    }
   427	
   428	    [[gnu::always_inline]] 
   429	    static const uint8_t* impl_TAIL_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   430	        // 4 * u16 = 8 bytes
   431	        auto [dst, fn_reg, arg_start, argc] = decode::args<u16, u16, u16, u16>(ip);
   432	        
   433	        // Skip IC (16 bytes) vì Tail Call không dùng IC (hoặc chưa implement)
   434	        ip += 16; 
   435	
   436	        Value& callee = regs[fn_reg];
   437	        if (!callee.is_function()) [[unlikely]] {
   438	            // Offset = 8 bytes args + 16 bytes IC = 24
   439	            return ERROR<24>(ip, regs, constants, state, 91, "TAIL_CALL target is not a function");
   440	        }
   441	        
   442	        function_t closure = callee.as_function();
   443	        proto_t proto = closure->get_proto();
   444	        size_t num_params = proto->get_num_registers();
   445	
   446	        size_t current_base_idx = regs - state->ctx.stack_;
   447	        meow::close_upvalues(state->ctx, current_base_idx);
   448	
   449	        size_t copy_count = (argc < num_params) ? argc : num_params;
   450	        for (size_t i = 0; i < copy_count; ++i) regs[i] = regs[arg_start + i];
   451	        for (size_t i = copy_count; i < num_params; ++i) regs[i] = Value(null_t{});
   452	
   453	        CallFrame* current_frame = state->ctx.frame_ptr_;
   454	        current_frame->function_ = closure;
   455	        state->ctx.stack_top_ = regs + num_params;
   456	        state->update_pointers();
   457	
   458	        return proto->get_chunk().get_code();
   459	    }
   460	
   461	    // --- JUMP COMPARE LOGIC (Refactored) ---
   462	
   463	    #define IMPL_CMP_JUMP(OP_NAME, OP_ENUM, OPERATOR) \
   464	    [[gnu::always_inline]] \
   465	    inline static const uint8_t* impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
   466	        auto [lhs, rhs, offset] = decode::args<u16, u16, i16>(ip); \
   467	        Value& left = regs[lhs]; \
   468	        Value& right = regs[rhs]; \
   469	        bool condition = false; \
   470	        if (left.holds_both<int_t>(right)) [[likely]] { condition = (left.as_int() OPERATOR right.as_int()); } \
   471	        else if (left.holds_both<float_t>(right)) { condition = (left.as_float() OPERATOR right.as_float()); } \
   472	        else [[unlikely]] { \
   473	            Value res = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
   474	            condition = meow::to_bool(res); \
   475	        } \
   476	        if (condition) return ip + offset; \
   477	        return ip; \
   478	    }
   479	
   480	#define IMPL_CMP_JUMP_B(OP_NAME, OP_ENUM, OPERATOR) \
   481	[[gnu::always_inline]] \
   482	inline static const uint8_t* impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
   483	    /* [TỐI ƯU] Dùng Proxy (make) thay vì Structured Binding (view) */ \
   484	    auto args = decode::make<u8, u8, i16>(ip); \
   485	    \
   486	    /* v0() -> lhs, v1() -> rhs */ \
   487	    Value& left = regs[args.v0()]; \
   488	    Value& right = regs[args.v1()]; \
   489	    \
   490	    bool condition = false; \
   491	    if (left.holds_both<int_t>(right)) [[likely]] { condition = (left.as_int() OPERATOR right.as_int()); } \
   492	    else if (left.holds_both<float_t>(right)) { condition = (left.as_float() OPERATOR right.as_float()); } \
   493	    else [[unlikely]] { \
   494	        Value res = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
   495	        condition = meow::to_bool(res); \
   496	    } \
   497	    \
   498	    /* Tính size */ \
   499	    constexpr size_t INSTR_SIZE = decode::size_of_v<u8, u8, i16>; \
   500	    \
   501	    /* v2() là offset (i16) */ \
   502	    if (condition) return ip + INSTR_SIZE + args.v2(); \
   503	    \
   504	    return ip + INSTR_SIZE; \
   505	}
   506	    IMPL_CMP_JUMP(JUMP_IF_EQ, EQ, ==)
   507	    IMPL_CMP_JUMP(JUMP_IF_NEQ, NEQ, !=)
   508	    IMPL_CMP_JUMP(JUMP_IF_LT, LT, <)
   509	    IMPL_CMP_JUMP(JUMP_IF_LE, LE, <=)
   510	    IMPL_CMP_JUMP(JUMP_IF_GT, GT, >)
   511	    IMPL_CMP_JUMP(JUMP_IF_GE, GE, >=)
   512	
   513	    IMPL_CMP_JUMP_B(JUMP_IF_EQ, EQ, ==)
   514	    IMPL_CMP_JUMP_B(JUMP_IF_NEQ, NEQ, !=)
   515	    IMPL_CMP_JUMP_B(JUMP_IF_LT, LT, <)
   516	    IMPL_CMP_JUMP_B(JUMP_IF_LE, LE, <=)
   517	    IMPL_CMP_JUMP_B(JUMP_IF_GT, GT, >)
   518	    IMPL_CMP_JUMP_B(JUMP_IF_GE, GE, >=)
   519	
   520	    #undef IMPL_CMP_JUMP
   521	    #undef IMPL_CMP_JUMP_B
   522	
   523	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/math_ops.h
// =============================================================================

     1	#pragma once
     2	#include "vm/handlers/utils.h"
     3	#include "vm/handlers/flow_ops.h"
     4	
     5	namespace meow::handlers {
     6	
     7	// --- MACROS (Updated for decode::args) ---
     8	
     9	// Binary Op (Standard 16-bit regs) -> Tổng 6 bytes -> decode đọc u64
    10	#define BINARY_OP_IMPL(NAME, OP_ENUM) \
    11	    HOT_HANDLER impl_##NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    12	        auto [dst, r1, r2] = decode::args<u16, u16, u16>(ip); \
    13	        Value& left  = regs[r1]; \
    14	        Value& right = regs[r2]; \
    15	        regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    16	        return ip; \
    17	    }
    18	
    19	// Binary Op (Byte 8-bit regs) -> Tổng 3 bytes -> decode đọc u32
    20	#define BINARY_OP_B_IMPL(NAME, OP_ENUM) \
    21	    HOT_HANDLER impl_##NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    22	        auto [dst, r1, r2] = decode::args<u8, u8, u8>(ip); \
    23	        Value& left  = regs[r1]; \
    24	        Value& right = regs[r2]; \
    25	        regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    26	        return ip; \
    27	    }
    28	
    29	// --- ADD (Specialized Arithmetic) ---
    30	
    31	HOT_HANDLER impl_ADD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    32	    // 3 * u16 = 6 bytes. Decoder tự động dùng u64 load.
    33	    auto [dst, r1, r2] = decode::args<u16, u16, u16>(ip);
    34	    
    35	    Value& left = regs[r1];
    36	    Value& right = regs[r2];
    37	    
    38	    if (left.holds_both<int_t>(right)) [[likely]] {
    39	        regs[dst] = left.as_int() + right.as_int();
    40	    }
    41	    else if (left.holds_both<float_t>(right)) {
    42	        regs[dst] = Value(left.as_float() + right.as_float());
    43	    }
    44	    else [[unlikely]] {
    45	        regs[dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    46	    }
    47	    return ip;
    48	}
    49	
    50	HOT_HANDLER impl_ADD_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    51	    // 3 * u8 = 3 bytes. Decoder tự động dùng u32 load.
    52	    auto [dst, r1, r2] = decode::args<u8, u8, u8>(ip);
    53	    
    54	    Value& left = regs[r1];
    55	    Value& right = regs[r2];
    56	    
    57	    if (left.holds_both<int_t>(right)) [[likely]] {
    58	        regs[dst] = left.as_int() + right.as_int();
    59	    }
    60	    else if (left.holds_both<float_t>(right)) {
    61	        regs[dst] = Value(left.as_float() + right.as_float());
    62	    }
    63	    else [[unlikely]] {
    64	        regs[dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    65	    }
    66	    return ip;
    67	}
    68	
    69	// --- Arithmetic Ops ---
    70	BINARY_OP_IMPL(SUB, SUB)
    71	BINARY_OP_IMPL(MUL, MUL)
    72	BINARY_OP_IMPL(DIV, DIV)
    73	BINARY_OP_IMPL(MOD, MOD)
    74	BINARY_OP_IMPL(POW, POW)
    75	
    76	BINARY_OP_B_IMPL(SUB, SUB)
    77	BINARY_OP_B_IMPL(MUL, MUL)
    78	BINARY_OP_B_IMPL(DIV, DIV)
    79	BINARY_OP_B_IMPL(MOD, MOD)
    80	
    81	// --- Bitwise Ops ---
    82	BINARY_OP_IMPL(BIT_AND, BIT_AND)
    83	BINARY_OP_IMPL(BIT_OR, BIT_OR)
    84	BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
    85	BINARY_OP_IMPL(LSHIFT, LSHIFT)
    86	BINARY_OP_IMPL(RSHIFT, RSHIFT)
    87	
    88	BINARY_OP_B_IMPL(BIT_AND, BIT_AND)
    89	BINARY_OP_B_IMPL(BIT_OR, BIT_OR)
    90	BINARY_OP_B_IMPL(BIT_XOR, BIT_XOR)
    91	BINARY_OP_B_IMPL(LSHIFT, LSHIFT)
    92	BINARY_OP_B_IMPL(RSHIFT, RSHIFT)
    93	
    94	// --- Comparison Ops ---
    95	
    96	#define CMP_FAST_IMPL(OP_NAME, OP_ENUM, OPERATOR) \
    97	    HOT_HANDLER impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    98	        auto [dst, r1, r2] = decode::args<u16, u16, u16>(ip); \
    99	        Value& left = regs[r1]; \
   100	        Value& right = regs[r2]; \
   101	        if (left.holds_both<int_t>(right)) [[likely]] { \
   102	            regs[dst] = Value(left.as_int() OPERATOR right.as_int()); \
   103	        } else [[unlikely]] { \
   104	            regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
   105	        } \
   106	        return ip; \
   107	    } \
   108	    HOT_HANDLER impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
   109	        auto [dst, r1, r2] = decode::args<u8, u8, u8>(ip); \
   110	        Value& left = regs[r1]; \
   111	        Value& right = regs[r2]; \
   112	        if (left.holds_both<int_t>(right)) [[likely]] { \
   113	            regs[dst] = Value(left.as_int() OPERATOR right.as_int()); \
   114	        } else [[unlikely]] { \
   115	            regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
   116	        } \
   117	        return ip; \
   118	    }
   119	
   120	CMP_FAST_IMPL(EQ, EQ, ==)
   121	CMP_FAST_IMPL(NEQ, NEQ, !=)
   122	CMP_FAST_IMPL(GT, GT, >)
   123	CMP_FAST_IMPL(GE, GE, >=)
   124	CMP_FAST_IMPL(LT, LT, <)
   125	CMP_FAST_IMPL(LE, LE, <=)
   126	
   127	// --- Unary Ops ---
   128	
   129	// NEG
   130	HOT_HANDLER impl_NEG(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   131	    // 2 * u16 = 4 bytes -> Load u32
   132	    auto [dst, src] = decode::args<u16, u16>(ip);
   133	    
   134	    Value& val = regs[src];
   135	    if (val.is_int()) [[likely]] regs[dst] = Value(-val.as_int());
   136	    else if (val.is_float()) regs[dst] = Value(-val.as_float());
   137	    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
   138	    return ip;
   139	}
   140	
   141	HOT_HANDLER impl_NEG_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   142	    // 2 * u8 = 2 bytes -> Load u16
   143	    auto [dst, src] = decode::args<u8, u8>(ip);
   144	    
   145	    Value& val = regs[src];
   146	    if (val.is_int()) [[likely]] regs[dst] = Value(-val.as_int());
   147	    else if (val.is_float()) regs[dst] = Value(-val.as_float());
   148	    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
   149	    return ip;
   150	}
   151	
   152	// NOT (!)
   153	HOT_HANDLER impl_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   154	    auto [dst, src] = decode::args<u16, u16>(ip);
   155	    Value& val = regs[src];
   156	    if (val.is_bool()) [[likely]] regs[dst] = Value(!val.as_bool());
   157	    else if (val.is_int()) regs[dst] = Value(val.as_int() == 0);
   158	    else if (val.is_null()) regs[dst] = Value(true);
   159	    else [[unlikely]] regs[dst] = Value(!to_bool(val));
   160	    return ip;
   161	}
   162	
   163	HOT_HANDLER impl_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   164	    auto [dst, src] = decode::args<u8, u8>(ip);
   165	    Value& val = regs[src];
   166	    if (val.is_bool()) [[likely]] regs[dst] = Value(!val.as_bool());
   167	    else if (val.is_int()) regs[dst] = Value(val.as_int() == 0);
   168	    else if (val.is_null()) regs[dst] = Value(true);
   169	    else [[unlikely]] regs[dst] = Value(!to_bool(val));
   170	    return ip;
   171	}
   172	
   173	// BIT_NOT (~)
   174	HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   175	    auto [dst, src] = decode::args<u16, u16>(ip);
   176	    Value& val = regs[src];
   177	    if (val.is_int()) [[likely]] regs[dst] = Value(~val.as_int());
   178	    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
   179	    return ip;
   180	}
   181	
   182	HOT_HANDLER impl_BIT_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   183	    auto [dst, src] = decode::args<u8, u8>(ip);
   184	    Value& val = regs[src];
   185	    if (val.is_int()) [[likely]] regs[dst] = Value(~val.as_int());
   186	    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
   187	    return ip;
   188	}
   189	
   190	// INC / DEC (Toán hạng 1 register)
   191	
   192	// Bản 16-bit
   193	HOT_HANDLER impl_INC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   194	    // 1 * u16 = 2 bytes
   195	    auto [reg_idx] = decode::args<u16>(ip);
   196	    Value& val = regs[reg_idx];
   197	    
   198	    if (val.is_int()) [[likely]] val = Value(val.as_int() + 1);
   199	    else if (val.is_float()) val = Value(val.as_float() + 1.0);
   200	    else [[unlikely]] { 
   201	        // Đã đọc 2 bytes -> Offset = 2
   202	        return ERROR<2>(ip, regs, constants, state, 1 /*TYPE_ERR*/, "INC requires Number"); 
   203	    }
   204	    return ip;
   205	}
   206	
   207	HOT_HANDLER impl_DEC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   208	    auto [reg_idx] = decode::args<u16>(ip);
   209	    Value& val = regs[reg_idx];
   210	    
   211	    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
   212	    else if (val.is_float()) val = Value(val.as_float() - 1.0);
   213	    else [[unlikely]] { 
   214	        return ERROR<2>(ip, regs, constants, state, 1 /*TYPE_ERR*/, "DEC requires Number"); 
   215	    }
   216	    return ip;
   217	}
   218	
   219	// Bản 8-bit
   220	HOT_HANDLER impl_INC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   221	    auto args = decode::make<u8>(ip);
   222	    
   223	    Value& val = regs[args.v0()];
   224	
   225	    if (val.holds<int64_t>()) [[likely]] {
   226	        val.unsafe_set<int64_t>(val.unsafe_get<int64_t>() + 1);
   227	    } else if (val.holds<double>()) {
   228	        val.unsafe_set<double>(val.unsafe_get<double>() + 1.0);
   229	    } else [[unlikely]] {
   230	        return ERROR<1>(ip, regs, constants, state, 1, "INC requires Number");
   231	    }
   232	
   233	    // Lazy return
   234	    return ip + decode::size_of_v<u8>;
   235	}
   236	
   237	HOT_HANDLER impl_DEC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   238	    auto [reg_idx] = decode::args<u8>(ip);
   239	    Value& val = regs[reg_idx];
   240	    
   241	    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
   242	    else if (val.is_float()) val = Value(val.as_float() - 1.0);
   243	    else [[unlikely]] { 
   244	        return ERROR<1>(ip, regs, constants, state, 1 /*TYPE_ERR*/, "DEC requires Number"); 
   245	    }
   246	    return ip;
   247	}
   248	
   249	#undef BINARY_OP_IMPL
   250	#undef BINARY_OP_B_IMPL
   251	#undef CMP_FAST_IMPL
   252	
   253	} // namespace meow::handlers


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
     8	constexpr int ERR_CONST_TYPE = 20;
     9	
    10	[[gnu::always_inline]] 
    11	static const uint8_t* impl_GET_GLOBAL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    12	    // 2 * u16 = 4 bytes -> Load u32
    13	    auto [dst, global_idx] = decode::args<u16, u16>(ip);
    14	    regs[dst] = state->current_module->get_global_by_index(global_idx);
    15	    return ip;
    16	}
    17	
    18	[[gnu::always_inline]] 
    19	static const uint8_t* impl_SET_GLOBAL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    20	    // 2 * u16 = 4 bytes -> Load u32
    21	    auto [global_idx, src] = decode::args<u16, u16>(ip);
    22	    Value val = regs[src];
    23	        
    24	    state->current_module->set_global_by_index(global_idx, val);
    25	    state->heap.write_barrier(state->current_module, val);
    26	    
    27	    return ip;
    28	}
    29	
    30	[[gnu::always_inline]] 
    31	static const uint8_t* impl_GET_UPVALUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    32	    auto [dst, uv_idx] = decode::args<u16, u16>(ip);
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
    43	[[gnu::always_inline]] 
    44	static const uint8_t* impl_SET_UPVALUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    45	    auto [uv_idx, src] = decode::args<u16, u16>(ip);
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
    58	[[gnu::always_inline]] 
    59	static const uint8_t* impl_CLOSURE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    60	    // 2 * u16 = 4 bytes -> Load u32
    61	    auto [dst, proto_idx] = decode::args<u16, u16>(ip);
    62	    
    63	    Value val = constants[proto_idx];
    64	    if (!val.is_proto()) [[unlikely]] {
    65	        // Offset 4 bytes -> ERROR<4> sẽ tự tính ip - 5 (đúng logic cũ)
    66	        return ERROR<4>(ip, regs, constants, state, ERR_CONST_TYPE, 
    67	                       "CLOSURE: Constant index {} is not a Proto", proto_idx);
    68	    }
    69	
    70	    proto_t proto = val.as_proto();
    71	    function_t closure = state->heap.new_function(proto);
    72	    
    73	    regs[dst] = Value(closure); 
    74	
    75	    size_t current_base_idx = regs - state->ctx.stack_;
    76	
    77	    for (size_t i = 0; i < proto->get_num_upvalues(); ++i) {
    78	        const auto& desc = proto->get_desc(i);
    79	        if (desc.is_local_) {
    80	            closure->set_upvalue(i, capture_upvalue(&state->ctx, &state->heap, current_base_idx + desc.index_));
    81	        } else {
    82	            closure->set_upvalue(i, state->ctx.frame_ptr_->function_->get_upvalue(desc.index_));
    83	        }
    84	    }
    85	
    86	    return ip;
    87	}
    88	
    89	[[gnu::always_inline]] 
    90	static const uint8_t* impl_CLOSE_UPVALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    91	    auto [last_reg] = decode::args<u16>(ip);
    92	    
    93	    size_t current_base_idx = regs - state->ctx.stack_;
    94	    close_upvalues(&state->ctx, current_base_idx + last_reg);
    95	    return ip;
    96	}
    97	
    98	} // namespace meow::handlers


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
     8	constexpr int ERR_MODULE = 30;
     9	constexpr int ERR_STACK_OVERFLOW = 31;
    10	
    11	[[gnu::always_inline]] 
    12	static const uint8_t* impl_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    13	    // 2 * u16 = 4 bytes -> Load u32
    14	    auto [name_idx, src_reg] = decode::args<u16, u16>(ip);
    15	    Value val = regs[src_reg];
    16	    
    17	    string_t name = constants[name_idx].as_string();
    18	    state->current_module->set_export(name, val);
    19	    
    20	    state->heap.write_barrier(state->current_module, val);
    21	    
    22	    return ip;
    23	}
    24	
    25	[[gnu::always_inline]] 
    26	static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    27	    // 3 * u16 = 6 bytes -> "Dân chơi" load u64 (đọc lố padding)
    28	    auto [dst, mod_reg, name_idx] = decode::args<u16, u16, u16>(ip);
    29	    
    30	    Value& mod_val = regs[mod_reg];
    31	    string_t name = constants[name_idx].as_string();
    32	    
    33	    if (!mod_val.is_module()) [[unlikely]] {
    34	        return ERROR<6>(ip, regs, constants, state, ERR_MODULE, "GET_EXPORT: Operand is not a Module");
    35	    }
    36	    
    37	    module_t mod = mod_val.as_module();
    38	    if (!mod->has_export(name)) [[unlikely]] {
    39	        return ERROR<6>(ip, regs, constants, state, ERR_MODULE, "Module does not export '{}'", name->c_str());
    40	    }
    41	    
    42	    regs[dst] = mod->get_export(name);
    43	    return ip;
    44	}
    45	
    46	[[gnu::always_inline]] 
    47	static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    48	    auto [src_idx] = decode::args<u16>(ip);
    49	    
    50	    const Value& mod_val = regs[src_idx];
    51	    
    52	    if (auto src_mod = mod_val.as_if_module()) {
    53	        state->current_module->import_all_export(src_mod);
    54	    } else [[unlikely]] {
    55	        return ERROR<2>(ip, regs, constants, state, ERR_MODULE, "IMPORT_ALL: Register does not contain a Module");
    56	    }
    57	    return ip;
    58	}
    59	
    60	[[gnu::always_inline]] 
    61	static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    62	    // 2 * u16 = 4 bytes -> Load u32
    63	    auto [dst, path_idx] = decode::args<u16, u16>(ip);
    64	    
    65	    string_t path = constants[path_idx].as_string();
    66	    string_t importer_path = state->current_module ? state->current_module->get_file_path() : nullptr;
    67	    
    68	    auto load_result = state->modules.load_module(path, importer_path);
    69	
    70	    if (load_result.failed()) {
    71	        auto err = load_result.error();
    72	        return ERROR<4>(ip, regs, constants, state, ERR_MODULE, 
    73	                        "Cannot import module '{}'. Code: {}", path->c_str(), static_cast<int>(err.code()));
    74	    }
    75	
    76	    module_t mod = load_result.value();
    77	    regs[dst] = Value(mod);
    78	
    79	    if (mod->is_executed() || mod->is_executing()) {
    80	        return ip;
    81	    }
    82	    
    83	    if (!mod->is_has_main()) {
    84	        mod->set_executed();
    85	        return ip;
    86	    }
    87	
    88	    mod->set_execution();
    89	    
    90	    proto_t main_proto = mod->get_main_proto();
    91	    function_t main_closure = state->heap.new_function(main_proto);
    92	    
    93	    size_t num_regs = main_proto->get_num_registers();
    94	
    95	    if (!state->ctx.check_frame_overflow()) [[unlikely]] {
    96	        return ERROR<4>(ip, regs, constants, state, ERR_STACK_OVERFLOW, "Call Stack Overflow (too many imports)");
    97	    }
    98	    if (!state->ctx.check_overflow(num_regs)) [[unlikely]] {
    99	        return ERROR<4>(ip, regs, constants, state, ERR_STACK_OVERFLOW, "Register Stack Overflow at import");
   100	    }
   101	    
   102	    Value* new_base = state->ctx.stack_top_;
   103	    state->ctx.frame_ptr_++; 
   104	    *state->ctx.frame_ptr_ = CallFrame(
   105	        main_closure,
   106	        new_base,
   107	        nullptr,
   108	        ip
   109	    );
   110	    
   111	    state->ctx.current_regs_ = new_base;
   112	    state->ctx.stack_top_ += num_regs;
   113	    state->ctx.current_frame_ = state->ctx.frame_ptr_;
   114	    
   115	    state->update_pointers();
   116	    
   117	    return main_proto->get_chunk().get_code(); 
   118	}
   119	
   120	} // namespace meow::handlers


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
     9	#include <array>
    10	#include <algorithm>
    11	
    12	namespace meow::handlers {
    13	
    14	// Mã lỗi giả định
    15	constexpr int ERR_PROP = 40;
    16	constexpr int ERR_METHOD = 41;
    17	constexpr int ERR_INHERIT = 42;
    18	
    19	static constexpr int IC_CAPACITY = 4;
    20	
    21	struct PrimitiveShapes {
    22	    static inline const Shape* ARRAY  = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x1));
    23	    static inline const Shape* STRING = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x2));
    24	    static inline const Shape* OBJECT = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x3));
    25	};
    26	
    27	struct InlineCacheEntry {
    28	    const Shape* shape;
    29	    const Shape* transition;
    30	    uint32_t offset;
    31	} __attribute__((packed));
    32	
    33	struct InlineCache {
    34	    InlineCacheEntry entries[IC_CAPACITY];
    35	} __attribute__((packed));
    36	
    37	// Helper cập nhật Inline Cache (giữ nguyên logic Move-to-front)
    38	inline static void update_inline_cache(InlineCache* ic, const Shape* shape, const Shape* transition, uint32_t offset) {
    39	    for (int i = 0; i < IC_CAPACITY; ++i) {
    40	        if (ic->entries[i].shape == shape) {
    41	            if (i > 0) {
    42	                InlineCacheEntry temp = ic->entries[i];
    43	                // Move to front
    44	                std::memmove(&ic->entries[1], &ic->entries[0], i * sizeof(InlineCacheEntry));
    45	                ic->entries[0] = temp;
    46	                ic->entries[0].transition = transition;
    47	                ic->entries[0].offset = offset;
    48	            } else {
    49	                ic->entries[0].transition = transition;
    50	                ic->entries[0].offset = offset;
    51	            }
    52	            return;
    53	        }
    54	    }
    55	    // Shift right & Insert new at front
    56	    std::memmove(&ic->entries[1], &ic->entries[0], (IC_CAPACITY - 1) * sizeof(InlineCacheEntry));
    57	    ic->entries[0].shape = shape;
    58	    ic->entries[0].transition = transition;
    59	    ic->entries[0].offset = offset;
    60	}
    61	
    62	enum class CoreModType { ARRAY, STRING, OBJECT };
    63	
    64	[[gnu::always_inline]]
    65	static inline module_t get_core_module(VMState* state, CoreModType type) {
    66	    static module_t mod_array = nullptr;
    67	    static module_t mod_string = nullptr;
    68	    static module_t mod_object = nullptr;
    69	
    70	    switch (type) {
    71	        case CoreModType::ARRAY:
    72	            if (!mod_array) [[unlikely]] {
    73	                auto res = state->modules.load_module(state->heap.new_string("array"), nullptr);
    74	                if (res.ok()) mod_array = res.value();
    75	            }
    76	            return mod_array;
    77	        case CoreModType::STRING:
    78	            if (!mod_string) [[unlikely]] {
    79	                auto res = state->modules.load_module(state->heap.new_string("string"), nullptr);
    80	                if (res.ok()) mod_string = res.value();
    81	            }
    82	            return mod_string;
    83	        case CoreModType::OBJECT:
    84	            if (!mod_object) [[unlikely]] {
    85	                auto res = state->modules.load_module(state->heap.new_string("object"), nullptr);
    86	                if (res.ok()) mod_object = res.value();
    87	            }
    88	            return mod_object;
    89	    }
    90	    return nullptr;
    91	}
    92	
    93	static inline Value find_primitive_method_slow(VMState* state, const Value& obj, string_t name, int32_t* out_index = nullptr) {
    94	    module_t mod = nullptr;
    95	
    96	    if (obj.is_array()) mod = get_core_module(state, CoreModType::ARRAY);
    97	    else if (obj.is_string()) mod = get_core_module(state, CoreModType::STRING);
    98	    else if (obj.is_hash_table()) mod = get_core_module(state, CoreModType::OBJECT);
    99	
   100	    if (mod) {
   101	        int32_t idx = mod->resolve_export_index(name);
   102	        if (idx != -1) {
   103	            if (out_index) *out_index = idx;
   104	            return mod->get_export_by_index(static_cast<uint32_t>(idx));
   105	        }
   106	    }
   107	    return Value(null_t{});
   108	}
   109	
   110	// --- HANDLERS ---
   111	
   112	[[gnu::always_inline]] 
   113	static const uint8_t* impl_INVOKE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   114	    auto [dst, obj_reg, name_idx, arg_start, argc] = decode::args<u16, u16, u16, u16, u16>(ip);
   115	    
   116	    // 2. Inline Cache (Tự động tính size dựa trên architecture)
   117	    InlineCache* ic = const_cast<InlineCache*>(decode::as_struct<InlineCache>(ip));
   118	    
   119	    // IP lúc này đã trỏ sang lệnh tiếp theo (Next IP)
   120	    const uint8_t* next_ip = ip; 
   121	    
   122	    // Tính offset để báo lỗi (10 byte args + Size IC)
   123	    constexpr size_t ErrOffset = 10 + sizeof(InlineCache);
   124	
   125	    Value& receiver = regs[obj_reg];
   126	    string_t name = constants[name_idx].as_string();
   127	
   128	    // 3. Instance Method Call (Fast Path)
   129	    if (receiver.is_instance()) [[likely]] {
   130	        instance_t inst = receiver.as_instance();
   131	        class_t k = inst->get_class();
   132	                
   133	        while (k) {
   134	            if (k->has_method(name)) {
   135	                Value method = k->get_method(name);
   136	                
   137	                if (method.is_function()) {
   138	                    const uint8_t* jump_target = push_call_frame(
   139	                        state, method.as_function(), argc, &regs[arg_start], &receiver,
   140	                        (dst == 0xFFFF) ? nullptr : &regs[dst], next_ip, ip - ErrOffset - 1 // Error IP (approx)
   141	                    );
   142	                    if (!jump_target) return impl_PANIC(ip, regs, constants, state);
   143	                    return jump_target;
   144	                }
   145	                else if (method.is_native()) {
   146	                    constexpr size_t MAX_NATIVE_ARGS = 64;
   147	                    Value arg_buffer[MAX_NATIVE_ARGS];
   148	                    arg_buffer[0] = receiver;
   149	                    size_t copy_count = std::min(static_cast<size_t>(argc), MAX_NATIVE_ARGS - 1);
   150	                    if (copy_count > 0) std::copy_n(regs + arg_start, copy_count, arg_buffer + 1);
   151	
   152	                    Value result = method.as_native()(&state->machine, copy_count + 1, arg_buffer);
   153	                    if (state->machine.has_error()) return impl_PANIC(ip, regs, constants, state);
   154	                    if (dst != 0xFFFF) regs[dst] = result;
   155	                    return next_ip;
   156	                }
   157	                break;
   158	            }
   159	            k = k->get_super();
   160	        }
   161	    }
   162	
   163	    // 4. Primitive Method Call (Optimized Stack Allocation)
   164	    Value method_val = find_primitive_method_slow(state, receiver, name); 
   165	    
   166	    if (!method_val.is_null()) [[likely]] {         
   167	         if (method_val.is_native()) {
   168	             constexpr size_t MAX_NATIVE_ARGS = 64;
   169	             Value arg_buffer[MAX_NATIVE_ARGS];
   170	
   171	             arg_buffer[0] = receiver; 
   172	             size_t copy_count = std::min(static_cast<size_t>(argc), MAX_NATIVE_ARGS - 1);
   173	             if (copy_count > 0) std::copy_n(regs + arg_start, copy_count, arg_buffer + 1);
   174	
   175	             Value result = method_val.as_native()(&state->machine, copy_count + 1, arg_buffer);
   176	
   177	             if (state->machine.has_error()) return impl_PANIC(ip, regs, constants, state);
   178	             if (dst != 0xFFFF) regs[dst] = result;
   179	             return next_ip;
   180	         } else {
   181	             return ERROR<ErrOffset>(ip, regs, constants, state, ERR_METHOD, "Primitive method must be native");
   182	         }
   183	    }
   184	
   185	    return ERROR<ErrOffset>(ip, regs, constants, state, ERR_METHOD, 
   186	                            "Method '{}' not found on object '{}'.", name->c_str(), to_string(receiver));
   187	}
   188	
   189	[[gnu::always_inline]] 
   190	static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   191	    // 2 * u16 = 4 bytes -> Load u32
   192	    auto [dst, name_idx] = decode::args<u16, u16>(ip);
   193	    regs[dst] = Value(state->heap.new_class(constants[name_idx].as_string()));
   194	    return ip;
   195	}
   196	
   197	[[gnu::always_inline]] 
   198	static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   199	    // 2 * u16 = 4 bytes -> Load u32
   200	    auto [dst, class_reg] = decode::args<u16, u16>(ip);
   201	    Value& class_val = regs[class_reg];
   202	    
   203	    if (!class_val.is_class()) [[unlikely]] {
   204	        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "NEW_INSTANCE: Operand is not a Class");
   205	    }
   206	    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
   207	    return ip;
   208	}
   209	
   210	[[gnu::always_inline]] 
   211	static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   212	    // 1. Decode Args: 3 * u16 = 6 bytes -> Load u64
   213	    auto [dst, obj_reg, name_idx] = decode::args<u16, u16, u16>(ip);
   214	    
   215	    // 2. Decode IC
   216	    InlineCache* ic = const_cast<InlineCache*>(decode::as_struct<InlineCache>(ip));
   217	
   218	    constexpr size_t ErrOffset = 6 + sizeof(InlineCache);
   219	
   220	    Value& obj = regs[obj_reg];
   221	    string_t name = constants[name_idx].as_string();
   222	
   223	    // 1. Magic Prop: length
   224	    static string_t str_length = nullptr;
   225	    if (!str_length) [[unlikely]] str_length = state->heap.new_string("length");
   226	
   227	    if (name == str_length) {
   228	        if (obj.is_array()) {
   229	            regs[dst] = Value(static_cast<int64_t>(obj.as_array()->size()));
   230	            return ip;
   231	        }
   232	        if (obj.is_string()) {
   233	            regs[dst] = Value(static_cast<int64_t>(obj.as_string()->size()));
   234	            return ip;
   235	        }
   236	    }
   237	
   238	    // 2. Instance Access (IC Optimized)
   239	    if (obj.is_instance()) [[likely]] {
   240	        instance_t inst = obj.as_instance();
   241	        Shape* current_shape = inst->get_shape();
   242	
   243	        // IC Hit (Slot 0)
   244	        if (ic->entries[0].shape == current_shape) {
   245	            regs[dst] = inst->get_field_at(ic->entries[0].offset);
   246	            return ip;
   247	        }
   248	        // Move-to-front (Slot 1)
   249	        if (ic->entries[1].shape == current_shape) {
   250	            InlineCacheEntry temp = ic->entries[1];
   251	            ic->entries[1] = ic->entries[0];
   252	            ic->entries[0] = temp;
   253	            regs[dst] = inst->get_field_at(temp.offset);
   254	            return ip;
   255	        }
   256	        
   257	        // IC Miss
   258	        int offset = current_shape->get_offset(name);
   259	        if (offset != -1) {
   260	            update_inline_cache(ic, current_shape, nullptr, static_cast<uint32_t>(offset));
   261	            regs[dst] = inst->get_field_at(offset);
   262	            return ip;
   263	        }
   264	        
   265	        // Lookup Class Methods
   266	        class_t k = inst->get_class();
   267	        while (k) {
   268	            if (k->has_method(name)) {
   269	                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
   270	                return ip;
   271	            }
   272	            k = k->get_super();
   273	        }
   274	    }
   275	    // 3. Array/String Primitive Access (IC Optimized)
   276	    else if (obj.is_array() || obj.is_string()) {
   277	        const Shape* sentinel = obj.is_array() ? PrimitiveShapes::ARRAY : PrimitiveShapes::STRING;
   278	        
   279	        if (ic->entries[0].shape == sentinel) {
   280	            module_t mod = get_core_module(state, obj.is_array() ? CoreModType::ARRAY : CoreModType::STRING);
   281	            Value method = mod->get_export_by_index(ic->entries[0].offset);
   282	            regs[dst] = Value(state->heap.new_bound_method(obj, method));
   283	            return ip;
   284	        }
   285	
   286	        int32_t idx = -1;
   287	        Value method = find_primitive_method_slow(state, obj, name, &idx);
   288	        
   289	        if (!method.is_null()) {
   290	            if (idx != -1) {
   291	                update_inline_cache(ic, sentinel, nullptr, static_cast<uint32_t>(idx));
   292	            }
   293	            regs[dst] = Value(state->heap.new_bound_method(obj, method));
   294	            return ip;
   295	        }
   296	    }
   297	    // 4. Hash Table & Module
   298	    else if (obj.is_hash_table()) {
   299	        hash_table_t hash = obj.as_hash_table();
   300	        if (hash->get(name, &regs[dst])) return ip;
   301	        
   302	        int32_t idx = -1;
   303	        Value method = find_primitive_method_slow(state, obj, name, &idx);
   304	        if (!method.is_null()) {
   305	            regs[dst] = Value(state->heap.new_bound_method(obj, method));
   306	            return ip;
   307	        }
   308	        regs[dst] = Value(null_t{});
   309	        return ip;
   310	    }
   311	    else if (obj.is_module()) {
   312	        module_t mod = obj.as_module();
   313	        if (mod->has_export(name)) {
   314	            regs[dst] = mod->get_export(name);
   315	            return ip;
   316	        }
   317	    }
   318	    else if (obj.is_class()) {
   319	        class_t k = obj.as_class();
   320	        if (k->has_method(name)) {
   321	            regs[dst] = k->get_method(name); 
   322	            return ip;
   323	        }
   324	    }
   325	    else if (obj.is_null()) [[unlikely]] {
   326	        return ERROR<ErrOffset>(ip, regs, constants, state, ERR_PROP, "Cannot read property '{}' of null.", name->c_str());
   327	    }
   328	
   329	    return ERROR<ErrOffset>(ip, regs, constants, state, ERR_PROP, 
   330	                            "Property '{}' not found on type '{}'.", name->c_str(), to_string(obj));
   331	}
   332	
   333	[[gnu::always_inline]] 
   334	static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   335	    // 1. Decode: 3 * u16 = 6 bytes -> Load u64
   336	    auto [obj_reg, name_idx, val_reg] = decode::args<u16, u16, u16>(ip);
   337	    
   338	    // 2. Decode IC
   339	    InlineCache* ic = const_cast<InlineCache*>(decode::as_struct<InlineCache>(ip));
   340	    
   341	    constexpr size_t ErrOffset = 6 + sizeof(InlineCache);
   342	
   343	    Value& obj = regs[obj_reg];
   344	    Value& val = regs[val_reg];
   345	    
   346	    if (obj.is_instance()) [[likely]] {
   347	        instance_t inst = obj.as_instance();
   348	        Shape* current_shape = inst->get_shape();
   349	
   350	        // IC Check
   351	        for (int i = 0; i < IC_CAPACITY; ++i) {
   352	            if (ic->entries[i].shape == current_shape) {
   353	                if (ic->entries[i].transition) { // Transition
   354	                    inst->set_shape(const_cast<Shape*>(ic->entries[i].transition));
   355	                    inst->add_field(val); 
   356	                    state->heap.write_barrier(inst, val);
   357	                    return ip;
   358	                }
   359	                // Update
   360	                inst->set_field_at(ic->entries[i].offset, val);
   361	                state->heap.write_barrier(inst, val);
   362	                return ip;
   363	            }
   364	        }
   365	
   366	        string_t name = constants[name_idx].as_string();
   367	        int offset = current_shape->get_offset(name);
   368	
   369	        if (offset != -1) { // Update existing
   370	            update_inline_cache(ic, current_shape, nullptr, static_cast<uint32_t>(offset));
   371	            inst->set_field_at(offset, val);
   372	            state->heap.write_barrier(inst, val);
   373	        } 
   374	        else { // Transition new
   375	            Shape* next_shape = current_shape->get_transition(name);
   376	            if (!next_shape) next_shape = current_shape->add_transition(name, &state->heap);
   377	            
   378	            uint32_t new_offset = static_cast<uint32_t>(inst->get_field_count());
   379	            update_inline_cache(ic, current_shape, next_shape, new_offset);
   380	
   381	            inst->set_shape(next_shape);
   382	            inst->add_field(val);
   383	            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape))); 
   384	            state->heap.write_barrier(inst, val);
   385	        }
   386	        return ip;
   387	    }
   388	    else if (obj.is_hash_table()) {
   389	        string_t name = constants[name_idx].as_string();
   390	        obj.as_hash_table()->set(name, val);
   391	        state->heap.write_barrier(obj.as_object(), val);
   392	    }
   393	    else {
   394	        return ERROR<ErrOffset>(ip, regs, constants, state, ERR_PROP, 
   395	                                "SET_PROP: Cannot set property '{}' on type '{}'.", 
   396	                                constants[name_idx].as_string()->c_str(), to_string(obj));
   397	    }
   398	    return ip;
   399	}
   400	
   401	[[gnu::always_inline]] 
   402	static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   403	    // 3 * u16 = 6 bytes -> Load u64
   404	    auto [class_reg, name_idx, method_reg] = decode::args<u16, u16, u16>(ip);
   405	    
   406	    Value& class_val = regs[class_reg];
   407	    Value& method_val = regs[method_reg];
   408	    
   409	    if (!class_val.is_class()) [[unlikely]] {
   410	        return ERROR<6>(ip, regs, constants, state, ERR_INHERIT, "SET_METHOD: Operand is not a Class");
   411	    }
   412	    
   413	    class_val.as_class()->set_method(constants[name_idx].as_string(), method_val);
   414	    state->heap.write_barrier(class_val.as_class(), method_val);
   415	    return ip;
   416	}
   417	
   418	[[gnu::always_inline]] 
   419	static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   420	    // 2 * u16 = 4 bytes -> Load u32
   421	    auto [sub_reg, super_reg] = decode::args<u16, u16>(ip);
   422	    
   423	    Value& sub_val = regs[sub_reg];
   424	    Value& super_val = regs[super_reg];
   425	    
   426	    if (!sub_val.is_class() || !super_val.is_class()) [[unlikely]] {
   427	        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "INHERIT: Both operands must be Classes");
   428	    }
   429	    
   430	    sub_val.as_class()->set_super(super_val.as_class());
   431	    return ip;
   432	}
   433	
   434	[[gnu::always_inline]] 
   435	static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   436	    // 2 * u16 = 4 bytes -> Load u32
   437	    auto [dst, name_idx] = decode::args<u16, u16>(ip);
   438	    string_t name = constants[name_idx].as_string();
   439	    
   440	    Value& receiver_val = regs[0]; // Convention: this/receiver is always reg[0] in method call? 
   441	    // FIXME: Nếu GET_SUPER được gọi ngoài method thì sao? 
   442	    // Tuy nhiên bytecode GET_SUPER thường chỉ sinh ra bên trong method của class.
   443	    
   444	    if (!receiver_val.is_instance()) [[unlikely]] {
   445	        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "GET_SUPER: 'this' is not an instance");
   446	    }
   447	    
   448	    instance_t receiver = receiver_val.as_instance();
   449	    class_t super = receiver->get_class()->get_super();
   450	    
   451	    if (!super) {
   452	        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "GET_SUPER: Class has no superclass");
   453	    }
   454	    
   455	    class_t k = super;
   456	    while (k) {
   457	        if (k->has_method(name)) {
   458	            regs[dst] = Value(state->heap.new_bound_method(receiver, k->get_method(name).as_function()));
   459	            return ip;
   460	        }
   461	        k = k->get_super();
   462	    }
   463	    
   464	    return ERROR<4>(ip, regs, constants, state, ERR_METHOD, "GET_SUPER: Method '{}' not found in superclass", name->c_str());
   465	}
   466	
   467	} // namespace meow::handlers


// =============================================================================
//  FILE PATH: src/vm/handlers/utils.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstring>
     4	#include <tuple>
     5	#include <array>
     6	#include <type_traits>
     7	#include <utility>
     8	#include <concepts>
     9	#include <format>
    10	
    11	#include "vm/vm_state.h"
    12	#include <meow/bytecode/op_codes.h>
    13	#include <meow/value.h>
    14	#include <meow/cast.h>
    15	#include "runtime/operator_dispatcher.h"
    16	#include "runtime/execution_context.h"
    17	#include "runtime/call_frame.h"
    18	#include <meow/core/function.h>
    19	#include <meow/memory/memory_manager.h>
    20	#include "runtime/upvalue.h"
    21	
    22	using u8  = uint8_t;
    23	using u16 = uint16_t;
    24	using u32 = uint32_t;
    25	using u64 = uint64_t;
    26	using i16 = int16_t;
    27	using i64 = int64_t;
    28	using f64 = double;
    29	
    30	#define HOT_HANDLER [[gnu::always_inline, gnu::hot, gnu::aligned(32)]] static const uint8_t*
    31	
    32	namespace meow::handlers {
    33	
    34	const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state);
    35	
    36	template <size_t Offset = 0, typename... Args>
    37	[[gnu::cold, gnu::noinline]]
    38	static const uint8_t* ERROR(
    39	    const uint8_t* current_ip, 
    40	    Value* regs, 
    41	    const Value* constants, 
    42	    VMState* state,
    43	    int code, 
    44	    Args&&... args
    45	) {
    46	    const uint8_t* fault_ip = current_ip - Offset - 1; 
    47	    std::string msg;
    48	    if constexpr (sizeof...(Args) > 0) {
    49	        msg = std::format("Error {}: {}", code, std::vformat(std::string_view("..."), std::make_format_args(args...))); 
    50	    } else {
    51	        msg = std::format("Runtime Error Code: {}", code);
    52	    }
    53	    state->error(msg, fault_ip);
    54	    return impl_PANIC(fault_ip, regs, constants, state);
    55	}
    56	
    57	namespace decode {
    58	
    59	    template <typename... Ts>
    60	    constexpr size_t total_size_v = (sizeof(Ts) + ...);
    61	
    62	    template <typename T>
    63	    [[gnu::always_inline, gnu::hot]]
    64	    inline T read_one(const uint8_t*& ip) {
    65	        T val;
    66	        __builtin_memcpy(&val, ip, sizeof(T));
    67	        ip += sizeof(T);
    68	        return val;
    69	    }
    70	
    71	    template <typename... Ts> struct ArgPack;
    72	    template <typename T1> struct [[gnu::packed]] ArgPack<T1> { T1 v0; };
    73	    template <typename T1, typename T2> struct [[gnu::packed]] ArgPack<T1, T2> { T1 v0; T2 v1; };
    74	    template <typename T1, typename T2, typename T3> struct [[gnu::packed]] ArgPack<T1, T2, T3> { T1 v0; T2 v1; T3 v2; };
    75	    template <typename T1, typename T2, typename T3, typename T4> struct [[gnu::packed]] ArgPack<T1, T2, T3, T4> { T1 v0; T2 v1; T3 v2; T4 v3; };
    76	    template <typename T1, typename T2, typename T3, typename T4, typename T5> struct [[gnu::packed]] ArgPack<T1, T2, T3, T4, T5> { T1 v0; T2 v1; T3 v2; T4 v3; T5 v4; };
    77	
    78	    template <typename... Ts>
    79	    [[gnu::always_inline, gnu::hot]]
    80	    inline auto args(const uint8_t*& ip) {
    81	        using PackedType = ArgPack<Ts...>;
    82	        PackedType val = *reinterpret_cast<const PackedType*>(ip);
    83	        ip += sizeof(PackedType);
    84	        return val;
    85	    }
    86	
    87	    template <typename... Ts>
    88	    [[gnu::always_inline, gnu::hot]]
    89	    inline const auto& view(const uint8_t* ip) {
    90	        using PackedType = ArgPack<Ts...>;
    91	        return *reinterpret_cast<const PackedType*>(ip);
    92	    }
    93	
    94	    template <typename... Ts>
    95	    [[gnu::always_inline, gnu::hot]]
    96	    inline ArgPack<Ts...> load(const uint8_t* ip) {
    97	        ArgPack<Ts...> val;
    98	        __builtin_memcpy(&val, ip, sizeof(val)); 
    99	        return val;
   100	    }
   101	
   102	    template <typename... Ts>
   103	    constexpr size_t size_of_v = sizeof(ArgPack<Ts...>);
   104	    
   105	    template <typename... Ts> struct ArgProxy;
   106	
   107	    template <typename T0>
   108	    struct ArgProxy<T0> {
   109	        const uint8_t* ip;
   110	        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip, sizeof(T0)); return v; }
   111	    };
   112	
   113	    template <typename T0, typename T1>
   114	    struct ArgProxy<T0, T1> {
   115	        const uint8_t* ip;
   116	        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip, sizeof(T0)); return v; }
   117	        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + sizeof(T0), sizeof(T1)); return v; }
   118	    };
   119	
   120	    template <typename T0, typename T1, typename T2>
   121	    struct ArgProxy<T0, T1, T2> {
   122	        const uint8_t* ip;
   123	        static constexpr size_t O1 = sizeof(T0);
   124	        static constexpr size_t O2 = O1 + sizeof(T1);
   125	        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip,      sizeof(T0)); return v; }
   126	        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + O1, sizeof(T1)); return v; }
   127	        [[gnu::always_inline]] T2 v2() const { T2 v; __builtin_memcpy(&v, ip + O2, sizeof(T2)); return v; }
   128	    };
   129	
   130	    template <typename T0, typename T1, typename T2, typename T3>
   131	    struct ArgProxy<T0, T1, T2, T3> {
   132	        const uint8_t* ip;
   133	        static constexpr size_t O1 = sizeof(T0);
   134	        static constexpr size_t O2 = O1 + sizeof(T1);
   135	        static constexpr size_t O3 = O2 + sizeof(T2);
   136	        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip,      sizeof(T0)); return v; }
   137	        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + O1, sizeof(T1)); return v; }
   138	        [[gnu::always_inline]] T2 v2() const { T2 v; __builtin_memcpy(&v, ip + O2, sizeof(T2)); return v; }
   139	        [[gnu::always_inline]] T3 v3() const { T3 v; __builtin_memcpy(&v, ip + O3, sizeof(T3)); return v; }
   140	    };
   141	    
   142	    template <typename T0, typename T1, typename T2, typename T3, typename T4>
   143	    struct ArgProxy<T0, T1, T2, T3, T4> {
   144	        const uint8_t* ip;
   145	        static constexpr size_t O1 = sizeof(T0);
   146	        static constexpr size_t O2 = O1 + sizeof(T1);
   147	        static constexpr size_t O3 = O2 + sizeof(T2);
   148	        static constexpr size_t O4 = O3 + sizeof(T3);
   149	        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip,      sizeof(T0)); return v; }
   150	        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + O1, sizeof(T1)); return v; }
   151	        [[gnu::always_inline]] T2 v2() const { T2 v; __builtin_memcpy(&v, ip + O2, sizeof(T2)); return v; }
   152	        [[gnu::always_inline]] T3 v3() const { T3 v; __builtin_memcpy(&v, ip + O3, sizeof(T3)); return v; }
   153	        [[gnu::always_inline]] T4 v4() const { T4 v; __builtin_memcpy(&v, ip + O4, sizeof(T4)); return v; }
   154	    };
   155	
   156	    template <typename... Ts>
   157	    [[gnu::always_inline]]
   158	    inline ArgProxy<Ts...> make(const uint8_t* ip) {
   159	        return ArgProxy<Ts...>{ip};
   160	    }
   161	    
   162	    template <typename T>
   163	    [[gnu::always_inline]]
   164	    inline const T* as_struct(const uint8_t*& ip) {
   165	        const T* ptr = reinterpret_cast<const T*>(ip);
   166	        ip += sizeof(T);
   167	        return ptr;
   168	    }
   169	} // namespace decode
   170	} // namespace meow::handlers


