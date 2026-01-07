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
     8	// =========================================================
     9	// STANDARD 16-BIT REGISTER OPERATIONS
    10	// =========================================================
    11	
    12	[[gnu::always_inline]] static const uint8_t* impl_LOAD_CONST(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    13	    uint32_t packed;
    14	    std::memcpy(&packed, ip, 4);
    15	
    16	    uint16_t dst = static_cast<uint16_t>(packed & 0xFFFF);
    17	    uint16_t idx = static_cast<uint16_t>(packed >> 16);
    18	    
    19	    regs[dst] = constants[idx];
    20	    return ip + 4;
    21	}
    22	
    23	[[gnu::always_inline]] static const uint8_t* impl_LOAD_NULL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    24	    uint16_t dst = read_u16(ip);
    25	    regs[dst] = null_t{};
    26	    return ip;
    27	}
    28	
    29	[[gnu::always_inline]] static const uint8_t* impl_LOAD_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    30	    uint16_t dst = read_u16(ip);
    31	    regs[dst] = true;
    32	    return ip;
    33	}
    34	
    35	[[gnu::always_inline]] static const uint8_t* impl_LOAD_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    36	    uint16_t dst = read_u16(ip);
    37	    regs[dst] = false;
    38	    return ip;
    39	}
    40	
    41	[[gnu::always_inline]] static const uint8_t* impl_LOAD_INT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    42	    uint16_t dst = read_u16(ip);
    43	    regs[dst] = *reinterpret_cast<const int64_t*>(ip);
    44	    ip += 8;
    45	    return ip;
    46	}
    47	
    48	[[gnu::always_inline]] static const uint8_t* impl_LOAD_FLOAT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    49	    uint16_t dst = read_u16(ip);
    50	    regs[dst] = *reinterpret_cast<const double*>(ip);
    51	    ip += 8;
    52	    return ip;
    53	}
    54	
    55	[[gnu::always_inline]] 
    56	static const uint8_t* impl_MOVE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    57	    uint32_t packed;
    58	    std::memcpy(&packed, ip, 4);
    59	
    60	    uint16_t dst = static_cast<uint16_t>(packed & 0xFFFF);
    61	    uint16_t src = static_cast<uint16_t>(packed >> 16);
    62	    regs[dst] = regs[src];
    63	
    64	    return ip + 4;
    65	}
    66	
    67	[[gnu::always_inline]] static const uint8_t* impl_NEW_ARRAY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    68	    uint16_t dst = read_u16(ip);
    69	    uint16_t start_idx = read_u16(ip);
    70	    uint16_t count = read_u16(ip);
    71	
    72	    auto array = state->heap.new_array();
    73	    regs[dst] = object_t(array);
    74	    array->reserve(count);
    75	    for (size_t i = 0; i < count; ++i) {
    76	        array->push(regs[start_idx + i]);
    77	    }
    78	    return ip;
    79	}
    80	
    81	[[gnu::always_inline]] 
    82	static const uint8_t* impl_NEW_HASH(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    83	    uint16_t dst = read_u16(ip);
    84	    uint16_t start_idx = read_u16(ip);
    85	    uint16_t count = read_u16(ip);
    86	    
    87	    auto hash = state->heap.new_hash(count); 
    88	
    89	    regs[dst] = Value(hash); 
    90	
    91	    for (size_t i = 0; i < count; ++i) {
    92	        Value& key = regs[start_idx + i * 2];
    93	        Value& val = regs[start_idx + i * 2 + 1];
    94	        
    95	        if (key.is_string()) [[likely]] {
    96	            hash->set(key.as_string(), val);
    97	        } else {
    98	            std::string s = to_string(key);
    99	            string_t k = state->heap.new_string(s);
   100	            hash->set(k, val);
   101	        }
   102	    }
   103	    
   104	    return ip;
   105	}
   106	
   107	[[gnu::always_inline]] static const uint8_t* impl_GET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   108	    uint16_t dst = read_u16(ip);
   109	    uint16_t src_reg = read_u16(ip);
   110	    uint16_t key_reg = read_u16(ip);
   111	    
   112	    Value& src = regs[src_reg];
   113	    Value& key = regs[key_reg];
   114	
   115	    if (src.is_array()) {
   116	        if (!key.is_int()) {
   117	            state->error("Array index phải là số nguyên.", ip);
   118	            return impl_PANIC(ip, regs, constants, state);
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
   148	            state->error("String index phải là số nguyên.", ip);
   149	            return impl_PANIC(ip, regs, constants, state);
   150	        }
   151	        string_t str = src.as_string();
   152	        int64_t idx = key.as_int();
   153	        if (idx < 0 || static_cast<size_t>(idx) >= str->size()) {
   154	            state->error("String index out of bounds.", ip);
   155	            return impl_PANIC(ip, regs, constants, state);
   156	        }
   157	        char c = str->get(idx);
   158	        regs[dst] = Value(state->heap.new_string(&c, 1));
   159	    }
   160	    else if (src.is_instance()) {
   161	        if (!key.is_string()) {
   162	            state->error("Instance index key phải là chuỗi.", ip);
   163	            return impl_PANIC(ip, regs, constants, state);
   164	        }
   165	        
   166	        string_t name = key.as_string();
   167	        instance_t inst = src.as_instance();
   168	        
   169	        int offset = inst->get_shape()->get_offset(name);
   170	        if (offset != -1) {
   171	            regs[dst] = inst->get_field_at(offset);
   172	        } 
   173	        else {
   174	            // Fallback to method lookup (như cũ)
   175	            class_t k = inst->get_class();
   176	            Value method = null_t{};
   177	            while (k) {
   178	                if (k->has_method(name)) {
   179	                    method = k->get_method(name);
   180	                    break;
   181	                }
   182	                k = k->get_super();
   183	            }
   184	            if (!method.is_null()) {
   185	                if (method.is_function() || method.is_native()) {
   186	                    auto bound = state->heap.new_bound_method(src, method);
   187	                    regs[dst] = Value(bound);
   188	                } else {
   189	                    regs[dst] = method;
   190	                }
   191	            } else {
   192	                regs[dst] = Value(null_t{});
   193	            }
   194	        }
   195	    }
   196	    else {
   197	        state->error("Không thể dùng toán tử index [] trên " + to_string(src), ip);
   198	        return impl_PANIC(ip, regs, constants, state);
   199	    }
   200	    return ip;
   201	}
   202	
   203	[[gnu::always_inline]] static const uint8_t* impl_SET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   204	    uint16_t src_reg = read_u16(ip);
   205	    uint16_t key_reg = read_u16(ip);
   206	    uint16_t val_reg = read_u16(ip);
   207	
   208	    Value& src = regs[src_reg];
   209	    Value& key = regs[key_reg];
   210	    Value& val = regs[val_reg];
   211	
   212	    if (src.is_array()) {
   213	        if (!key.is_int()) {
   214	            state->error("Array index phải là số nguyên.", ip);
   215	            return impl_PANIC(ip, regs, constants, state);
   216	        }
   217	        array_t arr = src.as_array();
   218	        int64_t idx = key.as_int();
   219	        if (idx < 0) {
   220	            state->error("Array index không được âm.", ip);
   221	            return impl_PANIC(ip, regs, constants, state);
   222	        }
   223	        if (static_cast<size_t>(idx) >= arr->size()) {
   224	            arr->resize(idx + 1);
   225	        }
   226	        arr->set(idx, val);
   227	        state->heap.write_barrier(src.as_object(), val);
   228	    }
   229	    else if (src.is_hash_table()) {
   230	        hash_table_t hash = src.as_hash_table();
   231	        string_t k = nullptr;
   232	        if (!key.is_string()) {
   233	            std::string s = to_string(key);
   234	            k = state->heap.new_string(s);
   235	        } else {
   236	            k = key.as_string();
   237	        }
   238	        hash->set(k, val);
   239	        state->heap.write_barrier(src.as_object(), val);
   240	    } 
   241	    else if (src.is_instance()) {
   242	        if (!key.is_string()) {
   243	            state->error("Instance set index key phải là chuỗi.", ip);
   244	            return impl_PANIC(ip, regs, constants, state);
   245	        }
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
   259	            inst->set_shape(next_shape);
   260	            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape)));
   261	            inst->add_field(val);
   262	            state->heap.write_barrier(inst, val);
   263	        }
   264	    }
   265	    else {
   266	        state->error("Không thể gán index [] trên kiểu dữ liệu này.", ip);
   267	        return impl_PANIC(ip, regs, constants, state);
   268	    }
   269	    return ip;
   270	}
   271	
   272	[[gnu::always_inline]] static const uint8_t* impl_GET_KEYS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   273	    uint16_t dst = read_u16(ip);
   274	    uint16_t src_reg = read_u16(ip);
   275	    Value& src = regs[src_reg];
   276	    
   277	    auto keys_array = state->heap.new_array();
   278	    
   279	    if (src.is_hash_table()) {
   280	        hash_table_t hash = src.as_hash_table();
   281	        keys_array->reserve(hash->size());
   282	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   283	            keys_array->push(Value(it->first));
   284	        }
   285	    } else if (src.is_array()) {
   286	        size_t sz = src.as_array()->size();
   287	        keys_array->reserve(sz);
   288	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   289	    } else if (src.is_string()) {
   290	        size_t sz = src.as_string()->size();
   291	        keys_array->reserve(sz);
   292	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   293	    }
   294	    
   295	    regs[dst] = Value(keys_array);
   296	    return ip;
   297	}
   298	
   299	[[gnu::always_inline]] static const uint8_t* impl_GET_VALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   300	    uint16_t dst = read_u16(ip);
   301	    uint16_t src_reg = read_u16(ip);
   302	    Value& src = regs[src_reg];
   303	    
   304	    auto vals_array = state->heap.new_array();
   305	
   306	    if (src.is_hash_table()) {
   307	        hash_table_t hash = src.as_hash_table();
   308	        vals_array->reserve(hash->size());
   309	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   310	            vals_array->push(it->second);
   311	        }
   312	    } else if (src.is_array()) {
   313	        array_t arr = src.as_array();
   314	        vals_array->reserve(arr->size());
   315	        for (size_t i = 0; i < arr->size(); ++i) vals_array->push(arr->get(i));
   316	    } else if (src.is_string()) {
   317	        string_t str = src.as_string();
   318	        vals_array->reserve(str->size());
   319	        for (size_t i = 0; i < str->size(); ++i) {
   320	            char c = str->get(i);
   321	            vals_array->push(Value(state->heap.new_string(&c, 1)));
   322	        }
   323	    }
   324	
   325	    regs[dst] = Value(vals_array);
   326	    return ip;
   327	}
   328	
   329	// =========================================================
   330	// OPTIMIZED 8-BIT REGISTER OPERATIONS (_B VARIANTS)
   331	// =========================================================
   332	
   333	[[gnu::always_inline]] static const uint8_t* impl_MOVE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   334	    uint8_t dst = *ip++;
   335	    uint8_t src = *ip++;
   336	    regs[dst] = regs[src];
   337	    return ip;
   338	}
   339	
   340	[[gnu::always_inline]] static const uint8_t* impl_LOAD_CONST_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   341	    uint8_t dst = *ip++;
   342	    uint16_t idx = read_u16(ip); // Index hằng số vẫn giữ 16-bit để truy cập pool lớn
   343	    regs[dst] = constants[idx];
   344	    return ip;
   345	}
   346	
   347	[[gnu::always_inline]] static const uint8_t* impl_LOAD_NULL_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   348	    uint8_t dst = *ip++;
   349	    regs[dst] = null_t{};
   350	    return ip;
   351	}
   352	
   353	[[gnu::always_inline]] static const uint8_t* impl_LOAD_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   354	    uint8_t dst = *ip++;
   355	    regs[dst] = true;
   356	    return ip;
   357	}
   358	
   359	[[gnu::always_inline]] static const uint8_t* impl_LOAD_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   360	    uint8_t dst = *ip++;
   361	    regs[dst] = false;
   362	    return ip;
   363	}
   364	
   365	[[gnu::always_inline]] static const uint8_t* impl_LOAD_INT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   366	    uint8_t dst = *ip++;
   367	    regs[dst] = *reinterpret_cast<const int64_t*>(ip);
   368	    ip += 8;
   369	    return ip;
   370	}
   371	
   372	[[gnu::always_inline]] static const uint8_t* impl_LOAD_FLOAT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   373	    uint8_t dst = *ip++;
   374	    regs[dst] = *reinterpret_cast<const double*>(ip);
   375	    ip += 8;
   376	    return ip;
   377	}
   378	
   379	} // namespace meow::handlers



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
    11	    state->error(to_string(val), ip);
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
     6	#include <vector>
     7	
     8	namespace meow::handlers {
     9	
    10	    struct JumpCondArgs { 
    11	        uint16_t cond; 
    12	        int16_t offset;
    13	    } __attribute__((packed));
    14	
    15	    struct JumpCondArgsB { 
    16	        uint8_t cond; 
    17	        int16_t offset; 
    18	    } __attribute__((packed));
    19	
    20	    struct JumpCompArgs { 
    21	        uint16_t lhs;    
    22	        uint16_t rhs;    
    23	        int16_t offset; 
    24	    } __attribute__((packed));
    25	
    26	    struct JumpCompArgsB { 
    27	        uint8_t lhs;     
    28	        uint8_t rhs;     
    29	        int16_t offset; 
    30	    } __attribute__((packed));
    31	
    32	    struct CallIC {
    33	        void* check_tag;
    34	        void* destination;
    35	    } __attribute__((packed)); 
    36	
    37	    [[gnu::always_inline]]
    38	    inline static CallIC* get_call_ic(const uint8_t*& ip) {
    39	        auto* ic = reinterpret_cast<CallIC*>(const_cast<uint8_t*>(ip));
    40	        ip += sizeof(CallIC); 
    41	        return ic;
    42	    }
    43	
    44	    [[gnu::always_inline]]
    45	    inline static const uint8_t* push_call_frame(
    46	        VMState* state, 
    47	        function_t closure, 
    48	        int argc, 
    49	        Value* args_src,       
    50	        Value* receiver,       
    51	        Value* ret_dest,       
    52	        const uint8_t* ret_ip, 
    53	        const uint8_t* err_ip  
    54	    ) {
    55	        proto_t proto = closure->get_proto();
    56	        size_t num_params = proto->get_num_registers();
    57	
    58	        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
    59	            state->error("Stack Overflow!", err_ip); 
    60	            return nullptr;
    61	        }
    62	
    63	        Value* new_base = state->ctx.stack_top_;
    64	        size_t arg_offset = 0;
    65	
    66	        if (receiver != nullptr && num_params > 0) {
    67	            new_base[0] = *receiver;
    68	            arg_offset = 1; 
    69	        }
    70	
    71	        size_t copy_count = (argc < (num_params - arg_offset)) ? argc : (num_params - arg_offset);
    72	        for (size_t i = 0; i < copy_count; ++i) {
    73	            new_base[arg_offset + i] = args_src[i];
    74	        }
    75	
    76	        size_t filled = arg_offset + argc;
    77	        for (size_t i = filled; i < num_params; ++i) {
    78	            new_base[i] = Value(null_t{});
    79	        }
    80	
    81	        state->ctx.frame_ptr_++;
    82	        *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest, ret_ip);
    83	        
    84	        state->ctx.current_regs_ = new_base;
    85	        state->ctx.stack_top_ += num_params;
    86	        state->ctx.current_frame_ = state->ctx.frame_ptr_;
    87	        state->update_pointers(); 
    88	
    89	        return state->instruction_base; 
    90	    }
    91	
    92	    [[gnu::always_inline]]
    93	    inline static const uint8_t* impl_NOP(const uint8_t* ip, Value*, const Value*, VMState*) { return ip; }
    94	
    95	    [[gnu::always_inline]]
    96	    inline static const uint8_t* impl_HALT(const uint8_t*, Value*, const Value*, VMState*) { return nullptr; }
    97	
    98	    [[gnu::always_inline]]
    99	    inline static const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   100	        (void)ip; (void)regs; (void)constants;
   101	        if (!state->ctx.exception_handlers_.empty()) {
   102	            auto& handler = state->ctx.exception_handlers_.back();
   103	            long current_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
   104	            while (current_depth > (long)handler.frame_depth_) {
   105	                size_t reg_idx = state->ctx.frame_ptr_->regs_base_ - state->ctx.stack_;
   106	                meow::close_upvalues(state->ctx, reg_idx);
   107	                state->ctx.frame_ptr_--;
   108	                current_depth--;
   109	            }
   110	            state->ctx.stack_top_ = state->ctx.stack_ + handler.stack_depth_;
   111	            state->ctx.current_regs_ = state->ctx.frame_ptr_->regs_base_;
   112	            state->ctx.current_frame_ = state->ctx.frame_ptr_; 
   113	            state->update_pointers();
   114	            const uint8_t* catch_ip = state->instruction_base + handler.catch_ip_;
   115	            if (handler.error_reg_ != static_cast<size_t>(-1)) {
   116	                auto err_str = state->heap.new_string(state->get_error_message());
   117	                regs[handler.error_reg_] = Value(err_str);
   118	            }
   119	            state->clear_error();
   120	            state->ctx.exception_handlers_.pop_back();
   121	            return catch_ip;
   122	        } 
   123	        std::println("VM Panic: {}", state->get_error_message());
   124	        return nullptr; 
   125	    }
   126	
   127	    [[gnu::always_inline]]
   128	    inline static const uint8_t* impl_UNIMPL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   129	        state->error("Opcode chưa được hỗ trợ (UNIMPL)", ip);
   130	        return impl_PANIC(ip, regs, constants, state);
   131	    }
   132	
   133	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   134	        int16_t offset = *reinterpret_cast<const int16_t*>(ip);
   135	        return ip + 2 + offset; 
   136	    }
   137	
   138	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   139	        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
   140	        Value& cond = regs[args.cond];
   141	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   142	        if (truthy) return ip + sizeof(JumpCondArgs) + args.offset;
   143	        return ip + sizeof(JumpCondArgs);
   144	    }
   145	
   146	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   147	        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
   148	        Value& cond = regs[args.cond];
   149	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   150	        if (!truthy) return ip + sizeof(JumpCondArgs) + args.offset;
   151	        return ip + sizeof(JumpCondArgs);
   152	    }
   153	
   154	    [[gnu::always_inline, gnu::hot]]
   155	    inline static const uint8_t* impl_JUMP_IF_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   156	        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
   157	        Value& cond = regs[args.cond];
   158	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   159	        if (truthy) return ip + sizeof(JumpCondArgsB) + args.offset;
   160	        return ip + sizeof(JumpCondArgsB);
   161	    }
   162	
   163	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   164	        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
   165	        Value& cond = regs[args.cond];
   166	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   167	        if (!truthy) return ip + sizeof(JumpCondArgsB) + args.offset;
   168	        return ip + sizeof(JumpCondArgsB);
   169	    }
   170	
   171	    [[gnu::always_inline]]
   172	    inline static const uint8_t* impl_RETURN(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   173	        uint16_t ret_reg_idx = read_u16(ip);
   174	        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : regs[ret_reg_idx];
   175	
   176	        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
   177	        meow::close_upvalues(state->ctx, base_idx);
   178	
   179	        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) [[unlikely]] return nullptr; 
   180	
   181	        CallFrame* popped_frame = state->ctx.frame_ptr_;
   182	        
   183	        if (state->current_module) [[likely]] {
   184	             if (popped_frame->function_->get_proto() == state->current_module->get_main_proto()) [[unlikely]] {
   185	                 state->current_module->set_executed();
   186	             }
   187	        }
   188	
   189	        state->ctx.frame_ptr_--;
   190	        CallFrame* caller = state->ctx.frame_ptr_;
   191	        
   192	        state->ctx.stack_top_ = popped_frame->regs_base_;
   193	        state->ctx.current_regs_ = caller->regs_base_;
   194	        state->ctx.current_frame_ = caller; 
   195	        state->update_pointers(); 
   196	
   197	        if (popped_frame->ret_dest_ != nullptr) *popped_frame->ret_dest_ = result;
   198	        return popped_frame->ip_; 
   199	    }
   200	
   201	    template <bool IsVoid>
   202	    [[gnu::always_inline]] 
   203	    static inline const uint8_t* do_call(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   204	        const uint8_t* start_ip = ip - 1; 
   205	
   206	        uint16_t dst = 0xFFFF;
   207	        uint16_t fn_reg, arg_start, argc;
   208	
   209	        if constexpr (!IsVoid) {
   210	            uint64_t packed;
   211	            std::memcpy(&packed, ip, 8); 
   212	            dst       = static_cast<uint16_t>(packed & 0xFFFF);
   213	            fn_reg    = static_cast<uint16_t>((packed >> 16) & 0xFFFF);
   214	            arg_start = static_cast<uint16_t>((packed >> 32) & 0xFFFF);
   215	            argc      = static_cast<uint16_t>((packed >> 48) & 0xFFFF);
   216	            ip += 8;
   217	        } else {
   218	            uint32_t packed;
   219	            std::memcpy(&packed, ip, 4);
   220	            fn_reg    = static_cast<uint16_t>(packed & 0xFFFF);
   221	            arg_start = static_cast<uint16_t>((packed >> 16) & 0xFFFF);
   222	            std::memcpy(&argc, ip + 4, 2);
   223	            ip += 6;
   224	        }
   225	
   226	        CallIC* ic = get_call_ic(ip);
   227	        Value& callee = regs[fn_reg];
   228	
   229	        function_t closure = nullptr;
   230	        Value* ret_dest_ptr = nullptr;
   231	
   232	        if (callee.is_function()) [[likely]] {
   233	            closure = callee.as_function();
   234	            if (ic->check_tag == closure->get_proto()) [[likely]] goto SETUP_FRAME;
   235	            ic->check_tag = closure->get_proto();
   236	            goto SETUP_FRAME;
   237	        }
   238	
   239	        if (callee.is_native()) {
   240	            native_t fn = callee.as_native();
   241	            if (ic->check_tag != (void*)fn) ic->check_tag = (void*)fn;
   242	            Value result = fn(&state->machine, argc, &regs[arg_start]);
   243	            if (state->machine.has_error()) [[unlikely]] {
   244	                state->error(std::string(state->machine.get_error_message()), ip);
   245	                state->machine.clear_error();
   246	                return impl_PANIC(ip, regs, constants, state);
   247	            }
   248	            if constexpr (!IsVoid) if (dst != 0xFFFF) regs[dst] = result;
   249	            return ip;
   250	        }
   251	
   252	        if constexpr (!IsVoid) if (dst != 0xFFFF) ret_dest_ptr = &regs[dst];
   253	
   254	        if (callee.is_bound_method()) {
   255	            bound_method_t bound = callee.as_bound_method();
   256	            Value method = bound->get_method();
   257	            Value receiver = bound->get_receiver();
   258	
   259	            if (method.is_function()) {
   260	                closure = method.as_function();
   261	                
   262	                proto_t proto = closure->get_proto();
   263	                size_t num_params = proto->get_num_registers();
   264	
   265	                if (!state->ctx.check_frame_overflow()) [[unlikely]] {
   266	                        state->ctx.current_frame_->ip_ = start_ip;
   267	                        state->error("Stack Overflow!", ip);
   268	                        return impl_PANIC(ip, regs, constants, state);
   269	                }
   270	
   271	                Value* new_base = state->ctx.stack_top_;
   272	                
   273	                new_base[0] = receiver; 
   274	
   275	                size_t copy_cnt = (argc < num_params - 1) ? argc : num_params - 1;
   276	                if (copy_cnt > 0) {
   277	                    std::memcpy((void*)(new_base + 1), &regs[arg_start], copy_cnt * sizeof(Value));
   278	                }
   279	                
   280	                size_t filled = 1 + copy_cnt;
   281	                for (size_t i = filled; i < num_params; ++i) new_base[i] = Value(null_t{});
   282	
   283	                state->ctx.frame_ptr_++;
   284	                *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest_ptr, ip);
   285	                
   286	                state->ctx.current_regs_ = new_base;
   287	                state->ctx.stack_top_ += num_params;
   288	                state->ctx.current_frame_ = state->ctx.frame_ptr_;
   289	                state->update_pointers(); 
   290	                return state->instruction_base;
   291	            }
   292	            
   293	            if (method.is_native()) {
   294	                native_t fn = method.as_native();
   295	                constexpr size_t STATIC_ARG_LIMIT = 64;
   296	                Value stack_args[STATIC_ARG_LIMIT];
   297	                Value* arg_ptr = stack_args;
   298	                
   299	                std::vector<Value> heap_args;
   300	                if (argc + 1 > STATIC_ARG_LIMIT) {
   301	                        heap_args.resize(argc + 1);
   302	                        arg_ptr = heap_args.data();
   303	                }
   304	                
   305	                arg_ptr[0] = receiver;
   306	                
   307	                if (argc > 0) {
   308	                    std::memcpy((void*)(arg_ptr + 1), &regs[arg_start], argc * sizeof(Value));
   309	                }
   310	
   311	                Value result = fn(&state->machine, argc + 1, arg_ptr);
   312	                
   313	                if (state->machine.has_error()) return impl_PANIC(ip, regs, constants, state);
   314	                if constexpr (!IsVoid) if (dst != 0xFFFF) regs[dst] = result;
   315	                return ip;
   316	            }
   317	        }
   318	        else if (callee.is_class()) {
   319	            class_t klass = callee.as_class();
   320	            instance_t self = state->heap.new_instance(klass, state->heap.get_empty_shape());
   321	            if (ret_dest_ptr) *ret_dest_ptr = Value(self);
   322	            
   323	            Value init_method = klass->get_method(state->heap.new_string("init"));
   324	            if (init_method.is_function()) {
   325	                closure = init_method.as_function();
   326	                
   327	                proto_t proto = closure->get_proto();
   328	                size_t num_params = proto->get_num_registers();
   329	                if (!state->ctx.check_frame_overflow()) return impl_PANIC(ip, regs, constants, state);
   330	                
   331	                Value* new_base = state->ctx.stack_top_;
   332	                new_base[0] = Value(self);
   333	                
   334	                size_t copy_cnt = (argc < num_params - 1) ? argc : num_params - 1;
   335	                if (copy_cnt > 0) {
   336	                    std::memcpy((void*)(new_base + 1), &regs[arg_start], copy_cnt * sizeof(Value));
   337	                }
   338	                
   339	                for (size_t i = 1 + copy_cnt; i < num_params; ++i) new_base[i] = Value(null_t{});
   340	                
   341	                state->ctx.frame_ptr_++;
   342	                *state->ctx.frame_ptr_ = CallFrame(closure, new_base, nullptr, ip);
   343	                state->ctx.current_regs_ = new_base;
   344	                state->ctx.stack_top_ += num_params;
   345	                state->ctx.current_frame_ = state->ctx.frame_ptr_;
   346	                state->update_pointers();
   347	                return state->instruction_base;
   348	            } 
   349	            return ip;
   350	        }
   351	
   352	        state->ctx.current_frame_->ip_ = start_ip;
   353	        state->error(std::format("Giá trị '{}' không thể gọi được.", to_string(callee)), ip);
   354	        return impl_PANIC(ip, regs, constants, state);
   355	
   356	    SETUP_FRAME:
   357	        {
   358	            proto_t proto = closure->get_proto();
   359	            size_t num_params = proto->get_num_registers();
   360	
   361	            if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
   362	                state->ctx.current_frame_->ip_ = start_ip;
   363	                state->error("Stack Overflow!", ip);
   364	                return impl_PANIC(ip, regs, constants, state);
   365	            }
   366	
   367	            Value* new_base = state->ctx.stack_top_;
   368	            
   369	            size_t copy_count = (argc < num_params) ? argc : num_params;
   370	            if (copy_count > 0) {
   371	                std::memcpy((void*)new_base, &regs[arg_start], copy_count * sizeof(Value));
   372	            }
   373	
   374	            for (size_t i = copy_count; i < num_params; ++i) {
   375	                new_base[i] = Value(null_t{});
   376	            }
   377	
   378	            state->ctx.frame_ptr_++;
   379	            if constexpr (!IsVoid) {
   380	                 *state->ctx.frame_ptr_ = CallFrame(closure, new_base, (dst == 0xFFFF) ? nullptr : &regs[dst], ip);
   381	            } else {
   382	                 *state->ctx.frame_ptr_ = CallFrame(closure, new_base, nullptr, ip);
   383	            }
   384	            
   385	            state->ctx.current_regs_ = new_base;
   386	            state->ctx.stack_top_ += num_params;
   387	            state->ctx.current_frame_ = state->ctx.frame_ptr_;
   388	            state->update_pointers(); 
   389	
   390	            return state->instruction_base;
   391	        }
   392	    }
   393	
   394	    // --- WRAPPERS ---
   395	
   396	    [[gnu::always_inline]] 
   397	    inline static const uint8_t* impl_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   398	        return do_call<false>(ip, regs, constants, state);
   399	    }
   400	
   401	    [[gnu::always_inline]] 
   402	    inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   403	        return do_call<true>(ip, regs, constants, state);
   404	    }
   405	
   406	    [[gnu::always_inline]] 
   407	    static const uint8_t* impl_TAIL_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   408	        const uint8_t* start_ip = ip - 1;
   409	
   410	        uint16_t dst = read_u16(ip); (void)dst;
   411	        uint16_t fn_reg = read_u16(ip);
   412	        uint16_t arg_start = read_u16(ip);
   413	        uint16_t argc = read_u16(ip);
   414	        ip += 16;
   415	
   416	        Value& callee = regs[fn_reg];
   417	        if (!callee.is_function()) [[unlikely]] {
   418	            state->ctx.current_frame_->ip_ = start_ip;
   419	            state->error("TAIL_CALL: Target không phải là Function.", ip);
   420	            return nullptr;
   421	        }
   422	        function_t closure = callee.as_function();
   423	        proto_t proto = closure->get_proto();
   424	        size_t num_params = proto->get_num_registers();
   425	
   426	        size_t current_base_idx = regs - state->ctx.stack_;
   427	        meow::close_upvalues(state->ctx, current_base_idx);
   428	
   429	        size_t copy_count = (argc < num_params) ? argc : num_params;
   430	        for (size_t i = 0; i < copy_count; ++i) regs[i] = regs[arg_start + i];
   431	        for (size_t i = copy_count; i < num_params; ++i) regs[i] = Value(null_t{});
   432	
   433	        CallFrame* current_frame = state->ctx.frame_ptr_;
   434	        current_frame->function_ = closure;
   435	        state->ctx.stack_top_ = regs + num_params;
   436	        state->update_pointers();
   437	
   438	        return proto->get_chunk().get_code();
   439	    }
   440	
   441	    // --- JUMP COMPARE LOGIC (UPDATED TO RELATIVE) ---
   442	
   443	    #define IMPL_CMP_JUMP(OP_NAME, OP_ENUM, OPERATOR) \
   444	    [[gnu::always_inline]] \
   445	    inline static const uint8_t* impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
   446	        const auto& args = *reinterpret_cast<const JumpCompArgs*>(ip); \
   447	        Value& left = regs[args.lhs]; \
   448	        Value& right = regs[args.rhs]; \
   449	        bool condition = false; \
   450	        if (left.holds_both<int_t>(right)) [[likely]] { condition = (left.as_int() OPERATOR right.as_int()); } \
   451	        else if (left.holds_both<float_t>(right)) { condition = (left.as_float() OPERATOR right.as_float()); } \
   452	        else [[unlikely]] { \
   453	            Value res = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
   454	            condition = meow::to_bool(res); \
   455	        } \
   456	        if (condition) return ip + sizeof(JumpCompArgs) + args.offset; \
   457	        return ip + sizeof(JumpCompArgs); \
   458	    }
   459	
   460	    #define IMPL_CMP_JUMP_B(OP_NAME, OP_ENUM, OPERATOR) \
   461	    [[gnu::always_inline]] \
   462	    inline static const uint8_t* impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
   463	        const auto& args = *reinterpret_cast<const JumpCompArgsB*>(ip); \
   464	        Value& left = regs[args.lhs]; \
   465	        Value& right = regs[args.rhs]; \
   466	        bool condition = false; \
   467	        if (left.holds_both<int_t>(right)) [[likely]] { condition = (left.as_int() OPERATOR right.as_int()); } \
   468	        else if (left.holds_both<float_t>(right)) { condition = (left.as_float() OPERATOR right.as_float()); } \
   469	        else [[unlikely]] { \
   470	            Value res = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
   471	            condition = meow::to_bool(res); \
   472	        } \
   473	        if (condition) return ip + sizeof(JumpCompArgsB) + args.offset; \
   474	        return ip + sizeof(JumpCompArgsB); \
   475	    }
   476	
   477	    IMPL_CMP_JUMP(JUMP_IF_EQ, EQ, ==)
   478	    IMPL_CMP_JUMP(JUMP_IF_NEQ, NEQ, !=)
   479	    IMPL_CMP_JUMP(JUMP_IF_LT, LT, <)
   480	    IMPL_CMP_JUMP(JUMP_IF_LE, LE, <=)
   481	    IMPL_CMP_JUMP(JUMP_IF_GT, GT, >)
   482	    IMPL_CMP_JUMP(JUMP_IF_GE, GE, >=)
   483	
   484	    IMPL_CMP_JUMP_B(JUMP_IF_EQ, EQ, ==)
   485	    IMPL_CMP_JUMP_B(JUMP_IF_NEQ, NEQ, !=)
   486	    IMPL_CMP_JUMP_B(JUMP_IF_LT, LT, <)
   487	    IMPL_CMP_JUMP_B(JUMP_IF_LE, LE, <=)
   488	    IMPL_CMP_JUMP_B(JUMP_IF_GT, GT, >)
   489	    IMPL_CMP_JUMP_B(JUMP_IF_GE, GE, >=)
   490	
   491	    #undef IMPL_CMP_JUMP
   492	    #undef IMPL_CMP_JUMP_B
   493	
   494	} // namespace meow::handlers


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
     9	struct UnaryArgs { uint16_t dst; uint16_t src; } __attribute__((packed));
    10	struct UnaryArgsB { uint8_t dst; uint8_t src; } __attribute__((packed));
    11	
    12	// --- MACROS ---
    13	#define BINARY_OP_IMPL(NAME, OP_ENUM) \
    14	    HOT_HANDLER impl_##NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    15	        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
    16	        Value& left  = regs[args.r1]; \
    17	        Value& right = regs[args.r2]; \
    18	        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    19	        return ip + sizeof(BinaryArgs); \
    20	    }
    21	
    22	#define BINARY_OP_B_IMPL(NAME, OP_ENUM) \
    23	    HOT_HANDLER impl_##NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    24	        const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip); \
    25	        Value& left  = regs[args.r1]; \
    26	        Value& right = regs[args.r2]; \
    27	        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    28	        return ip + sizeof(BinaryArgsB); \
    29	    }
    30	
    31	// --- ADD (Arithmetic) ---
    32	HOT_HANDLER impl_ADD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    33	    const auto& args = *reinterpret_cast<const BinaryArgs*>(ip);
    34	    Value& left = regs[args.r1];
    35	    Value& right = regs[args.r2];
    36	    if (left.holds_both<int_t>(right)) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    37	    else if (left.holds_both<float_t>(right)) regs[args.dst] = Value(left.as_float() + right.as_float());
    38	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    39	    return ip + sizeof(BinaryArgs);
    40	}
    41	
    42	HOT_HANDLER impl_ADD_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    43	    const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip);
    44	    Value& left = regs[args.r1];
    45	    Value& right = regs[args.r2];
    46	    if (left.holds_both<int_t>(right)) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    47	    else if (left.holds_both<float_t>(right)) regs[args.dst] = Value(left.as_float() + right.as_float());
    48	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    49	    return ip + sizeof(BinaryArgsB);
    50	}
    51	
    52	// --- Arithmetic Ops ---
    53	BINARY_OP_IMPL(SUB, SUB)
    54	BINARY_OP_IMPL(MUL, MUL)
    55	BINARY_OP_IMPL(DIV, DIV)
    56	BINARY_OP_IMPL(MOD, MOD)
    57	BINARY_OP_IMPL(POW, POW)
    58	
    59	BINARY_OP_B_IMPL(SUB, SUB)
    60	BINARY_OP_B_IMPL(MUL, MUL)
    61	BINARY_OP_B_IMPL(DIV, DIV)
    62	BINARY_OP_B_IMPL(MOD, MOD)
    63	
    64	// --- Bitwise Ops ---
    65	BINARY_OP_IMPL(BIT_AND, BIT_AND)
    66	BINARY_OP_IMPL(BIT_OR, BIT_OR)
    67	BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
    68	BINARY_OP_IMPL(LSHIFT, LSHIFT)
    69	BINARY_OP_IMPL(RSHIFT, RSHIFT)
    70	
    71	BINARY_OP_B_IMPL(BIT_AND, BIT_AND)
    72	BINARY_OP_B_IMPL(BIT_OR, BIT_OR)
    73	BINARY_OP_B_IMPL(BIT_XOR, BIT_XOR)
    74	BINARY_OP_B_IMPL(LSHIFT, LSHIFT)
    75	BINARY_OP_B_IMPL(RSHIFT, RSHIFT)
    76	
    77	// --- Comparison Ops ---
    78	#define CMP_FAST_IMPL(OP_NAME, OP_ENUM, OPERATOR) \
    79	    HOT_HANDLER impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    80	        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
    81	        Value& left = regs[args.r1]; \
    82	        Value& right = regs[args.r2]; \
    83	        if (left.holds_both<int_t>(right)) [[likely]] { \
    84	            regs[args.dst] = Value(left.as_int() OPERATOR right.as_int()); \
    85	        } else [[unlikely]] { \
    86	            regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    87	        } \
    88	        return ip + sizeof(BinaryArgs); \
    89	    } \
    90	    HOT_HANDLER impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    91	        const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip); \
    92	        Value& left = regs[args.r1]; \
    93	        Value& right = regs[args.r2]; \
    94	        if (left.holds_both<int_t>(right)) [[likely]] { \
    95	            regs[args.dst] = Value(left.as_int() OPERATOR right.as_int()); \
    96	        } else [[unlikely]] { \
    97	            regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
    98	        } \
    99	        return ip + sizeof(BinaryArgsB); \
   100	    }
   101	
   102	CMP_FAST_IMPL(EQ, EQ, ==)
   103	CMP_FAST_IMPL(NEQ, NEQ, !=)
   104	CMP_FAST_IMPL(GT, GT, >)
   105	CMP_FAST_IMPL(GE, GE, >=)
   106	CMP_FAST_IMPL(LT, LT, <)
   107	CMP_FAST_IMPL(LE, LE, <=)
   108	
   109	// --- Unary Ops ---
   110	
   111	// NEG
   112	HOT_HANDLER impl_NEG(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   113	    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
   114	    Value& val = regs[args.src];
   115	    if (val.is_int()) [[likely]] regs[args.dst] = Value(-val.as_int());
   116	    else if (val.is_float()) regs[args.dst] = Value(-val.as_float());
   117	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
   118	    return ip + sizeof(UnaryArgs);
   119	}
   120	
   121	HOT_HANDLER impl_NEG_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   122	    const auto& args = *reinterpret_cast<const UnaryArgsB*>(ip);
   123	    Value& val = regs[args.src];
   124	    if (val.is_int()) [[likely]] regs[args.dst] = Value(-val.as_int());
   125	    else if (val.is_float()) regs[args.dst] = Value(-val.as_float());
   126	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
   127	    return ip + sizeof(UnaryArgsB);
   128	}
   129	
   130	// NOT (!)
   131	HOT_HANDLER impl_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   132	    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
   133	    Value& val = regs[args.src];
   134	    if (val.is_bool()) [[likely]] regs[args.dst] = Value(!val.as_bool());
   135	    else if (val.is_int()) regs[args.dst] = Value(val.as_int() == 0);
   136	    else if (val.is_null()) regs[args.dst] = Value(true);
   137	    else [[unlikely]] regs[args.dst] = Value(!to_bool(val));
   138	    return ip + sizeof(UnaryArgs);
   139	}
   140	
   141	HOT_HANDLER impl_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   142	    const auto& args = *reinterpret_cast<const UnaryArgsB*>(ip);
   143	    Value& val = regs[args.src];
   144	    if (val.is_bool()) [[likely]] regs[args.dst] = Value(!val.as_bool());
   145	    else if (val.is_int()) regs[args.dst] = Value(val.as_int() == 0);
   146	    else if (val.is_null()) regs[args.dst] = Value(true);
   147	    else [[unlikely]] regs[args.dst] = Value(!to_bool(val));
   148	    return ip + sizeof(UnaryArgsB);
   149	}
   150	
   151	// BIT_NOT (~)
   152	HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   153	    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
   154	    Value& val = regs[args.src];
   155	    if (val.is_int()) [[likely]] regs[args.dst] = Value(~val.as_int());
   156	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
   157	    return ip + sizeof(UnaryArgs);
   158	}
   159	
   160	HOT_HANDLER impl_BIT_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   161	    const auto& args = *reinterpret_cast<const UnaryArgsB*>(ip);
   162	    Value& val = regs[args.src];
   163	    if (val.is_int()) [[likely]] regs[args.dst] = Value(~val.as_int());
   164	    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
   165	    return ip + sizeof(UnaryArgsB);
   166	}
   167	
   168	// INC / DEC (Toán hạng 1 register)
   169	// Bản 16-bit
   170	[[gnu::always_inline]] static const uint8_t* impl_INC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   171	    uint16_t reg_idx = read_u16(ip);
   172	    Value& val = regs[reg_idx];
   173	    if (val.is_int()) [[likely]] val = Value(val.as_int() + 1);
   174	    else if (val.is_float()) val = Value(val.as_float() + 1.0);
   175	    else [[unlikely]] { state->error("INC requires Number.", ip); return nullptr; }
   176	    return ip;
   177	}
   178	[[gnu::always_inline]] static const uint8_t* impl_DEC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   179	    uint16_t reg_idx = read_u16(ip);
   180	    Value& val = regs[reg_idx];
   181	    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
   182	    else if (val.is_float()) val = Value(val.as_float() - 1.0);
   183	    else [[unlikely]] { state->error("DEC requires Number.", ip); return nullptr; }
   184	    return ip;
   185	}
   186	
   187	// Bản 8-bit
   188	[[gnu::always_inline, gnu::hot]] 
   189	static const uint8_t* impl_INC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   190	    uint8_t reg_idx = *ip++;
   191	    
   192	    Value& val = regs[reg_idx];
   193	
   194	    if (val.holds<int64_t>()) [[likely]] {
   195	        val.unsafe_set<int64_t>(val.unsafe_get<int64_t>() + 1);
   196	    } else if (val.holds<double>()) {
   197	        val.unsafe_set<double>(val.unsafe_get<double>() + 1.0);
   198	    } else [[unlikely]] {
   199	        state->error("Runtime Error: Operand must be a number for increment.", ip - 1);
   200	        return nullptr;
   201	    }
   202	
   203	    return ip;
   204	}
   205	[[gnu::always_inline]] static const uint8_t* impl_DEC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   206	    uint8_t reg_idx = *ip++;
   207	    Value& val = regs[reg_idx];
   208	    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
   209	    else if (val.is_float()) val = Value(val.as_float() - 1.0);
   210	    else [[unlikely]] { state->error("DEC requires Number.", ip); return nullptr; }
   211	    return ip;
   212	}
   213	
   214	#undef BINARY_OP_IMPL
   215	#undef BINARY_OP_B_IMPL
   216	#undef CMP_FAST_IMPL
   217	
   218	} // namespace meow::handlers


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
    65	        state->error("CLOSURE: Constant index " + std::to_string(proto_idx) + " is not a Proto", ip);
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
    32	        state->error("GET_EXPORT: Toán hạng không phải là Module.", ip);
    33	        return impl_PANIC(ip, regs, constants, state);
    34	    }
    35	    
    36	    module_t mod = mod_val.as_module();
    37	    if (!mod->has_export(name)) [[unlikely]] {
    38	        state->error("Module không export '" + std::string(name->c_str()) + "'.", ip);
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
    56	        state->error("IMPORT_ALL: Register không chứa Module.", ip);
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
    70	    auto load_result = state->modules.load_module(path, importer_path);
    71	
    72	    if (load_result.failed()) {
    73	        auto err = load_result.error();
    74	        state->error(std::format("Runtime Error: Cannot import module '{}'. Error Code: {}", 
    75	                                 path->c_str(), static_cast<int>(err.code())), ip);
    76	        return impl_PANIC(ip, regs, constants, state);
    77	    }
    78	
    79	    module_t mod = load_result.value();
    80	    regs[dst] = Value(mod);
    81	
    82	    if (mod->is_executed() || mod->is_executing()) {
    83	        return ip;
    84	    }
    85	    
    86	    if (!mod->is_has_main()) {
    87	        mod->set_executed();
    88	        return ip;
    89	    }
    90	
    91	    mod->set_execution();
    92	    
    93	    proto_t main_proto = mod->get_main_proto();
    94	    function_t main_closure = state->heap.new_function(main_proto);
    95	    
    96	    size_t num_regs = main_proto->get_num_registers();
    97	
    98	    if (!state->ctx.check_frame_overflow()) [[unlikely]] {
    99	        state->error("Call Stack Overflow (too many imports)!", ip);
   100	        return impl_PANIC(ip, regs, constants, state);
   101	    }
   102	    if (!state->ctx.check_overflow(num_regs)) [[unlikely]] {
   103	        state->error("Register Stack Overflow at import!", ip);
   104	        return impl_PANIC(ip, regs, constants, state);
   105	    }
   106	    
   107	    Value* new_base = state->ctx.stack_top_;
   108	    state->ctx.frame_ptr_++; 
   109	    *state->ctx.frame_ptr_ = CallFrame(
   110	        main_closure,
   111	        new_base,
   112	        nullptr,
   113	        ip
   114	    );
   115	    
   116	    state->ctx.current_regs_ = new_base;
   117	    state->ctx.stack_top_ += num_regs;
   118	    state->ctx.current_frame_ = state->ctx.frame_ptr_;
   119	    
   120	    state->update_pointers();
   121	    
   122	    return main_proto->get_chunk().get_code(); 
   123	}
   124	
   125	} // namespace meow::handlers


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
    10	
    11	namespace meow::handlers {
    12	
    13	static constexpr int IC_CAPACITY = 4;
    14	
    15	struct PrimitiveShapes {
    16	    static inline const Shape* ARRAY  = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x1));
    17	    static inline const Shape* STRING = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x2));
    18	    static inline const Shape* OBJECT = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x3));
    19	};
    20	
    21	struct InlineCacheEntry {
    22	    const Shape* shape;
    23	    const Shape* transition;
    24	    uint32_t offset;
    25	} __attribute__((packed));
    26	
    27	struct InlineCache {
    28	    InlineCacheEntry entries[IC_CAPACITY];
    29	} __attribute__((packed));
    30	
    31	
    32	[[gnu::always_inline]]
    33	inline static InlineCache* get_inline_cache(const uint8_t*& ip) {
    34	    auto* ic = reinterpret_cast<InlineCache*>(const_cast<uint8_t*>(ip));
    35	    ip += sizeof(InlineCache); 
    36	    return ic;
    37	}
    38	
    39	inline static void update_inline_cache(InlineCache* ic, const Shape* shape, const Shape* transition, uint32_t offset) {
    40	    for (int i = 0; i < IC_CAPACITY; ++i) {
    41	        if (ic->entries[i].shape == shape) {
    42	            if (i > 0) {
    43	                InlineCacheEntry temp = ic->entries[i];
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
    55	    std::memmove(&ic->entries[1], &ic->entries[0], (IC_CAPACITY - 1) * sizeof(InlineCacheEntry));
    56	    ic->entries[0].shape = shape;
    57	    ic->entries[0].transition = transition;
    58	    ic->entries[0].offset = offset;
    59	}
    60	
    61	enum class CoreModType { ARRAY, STRING, OBJECT };
    62	
    63	[[gnu::always_inline]]
    64	static inline module_t get_core_module(VMState* state, CoreModType type) {
    65	    static module_t mod_array = nullptr;
    66	    static module_t mod_string = nullptr;
    67	    static module_t mod_object = nullptr;
    68	
    69	    switch (type) {
    70	        case CoreModType::ARRAY:
    71	            if (!mod_array) [[unlikely]] {
    72	                auto res = state->modules.load_module(state->heap.new_string("array"), nullptr);
    73	                if (res.ok()) mod_array = res.value();
    74	            }
    75	            return mod_array;
    76	        case CoreModType::STRING:
    77	            if (!mod_string) [[unlikely]] {
    78	                auto res = state->modules.load_module(state->heap.new_string("string"), nullptr);
    79	                if (res.ok()) mod_string = res.value();
    80	            }
    81	            return mod_string;
    82	        case CoreModType::OBJECT:
    83	            if (!mod_object) [[unlikely]] {
    84	                auto res = state->modules.load_module(state->heap.new_string("object"), nullptr);
    85	                if (res.ok()) mod_object = res.value();
    86	            }
    87	            return mod_object;
    88	    }
    89	    return nullptr;
    90	}
    91	
    92	static inline Value find_primitive_method_slow(VMState* state, const Value& obj, string_t name, int32_t* out_index = nullptr) {
    93	    module_t mod = nullptr;
    94	
    95	    if (obj.is_array()) mod = get_core_module(state, CoreModType::ARRAY);
    96	    else if (obj.is_string()) mod = get_core_module(state, CoreModType::STRING);
    97	    else if (obj.is_hash_table()) mod = get_core_module(state, CoreModType::OBJECT);
    98	
    99	    if (mod) {
   100	        int32_t idx = mod->resolve_export_index(name);
   101	        if (idx != -1) {
   102	            if (out_index) *out_index = idx;
   103	            return mod->get_export_by_index(static_cast<uint32_t>(idx));
   104	        }
   105	    }
   106	    return Value(null_t{});
   107	}
   108	
   109	// --- HANDLERS ---
   110	
   111	[[gnu::always_inline]] 
   112	static const uint8_t* impl_INVOKE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   113	    // 1. Setup IP
   114	    const uint8_t* next_ip = ip + 79;
   115	    const uint8_t* start_ip = ip - 1; 
   116	
   117	    // 2. Decode Arguments
   118	    uint16_t dst = read_u16(ip);
   119	    uint16_t obj_reg = read_u16(ip);
   120	    uint16_t name_idx = read_u16(ip);
   121	    uint16_t arg_start = read_u16(ip);
   122	    uint16_t argc = read_u16(ip);
   123	    
   124	    // 3. Inline Cache 
   125	    InlineCache* ic = get_inline_cache(ip); 
   126	
   127	    Value& receiver = regs[obj_reg];
   128	    string_t name = constants[name_idx].as_string();
   129	
   130	    // 4. Instance Method Call (Fast Path)
   131	    if (receiver.is_instance()) [[likely]] {
   132	        instance_t inst = receiver.as_instance();
   133	        class_t k = inst->get_class();
   134	                
   135	        while (k) {
   136	            if (k->has_method(name)) {
   137	                Value method = k->get_method(name);
   138	                
   139	                if (method.is_function()) {
   140	                    const uint8_t* jump_target = push_call_frame(
   141	                        state, method.as_function(), argc, &regs[arg_start], &receiver,
   142	                        (dst == 0xFFFF) ? nullptr : &regs[dst], next_ip, start_ip
   143	                    );
   144	                    if (!jump_target) return impl_PANIC(start_ip, regs, constants, state);
   145	                    return jump_target;
   146	                }
   147	                else if (method.is_native()) {
   148	                    constexpr size_t MAX_NATIVE_ARGS = 64;
   149	                    Value arg_buffer[MAX_NATIVE_ARGS];
   150	                    arg_buffer[0] = receiver;
   151	                    size_t copy_count = std::min(static_cast<size_t>(argc), MAX_NATIVE_ARGS - 1);
   152	                    if (copy_count > 0) std::copy_n(regs + arg_start, copy_count, arg_buffer + 1);
   153	
   154	                    Value result = method.as_native()(&state->machine, copy_count + 1, arg_buffer);
   155	                    if (state->machine.has_error()) return impl_PANIC(start_ip, regs, constants, state);
   156	                    if (dst != 0xFFFF) regs[dst] = result;
   157	                    return next_ip;
   158	                }
   159	                break;
   160	            }
   161	            k = k->get_super();
   162	        }
   163	    }
   164	
   165	    // 5. Primitive Method Call (Optimized Stack Allocation)
   166	    Value method_val = find_primitive_method_slow(state, receiver, name); 
   167	    
   168	    if (!method_val.is_null()) [[likely]] {         
   169	         if (method_val.is_native()) {
   170	             // TỐI ƯU: Buffer trên Stack, không heap allocation
   171	             constexpr size_t MAX_NATIVE_ARGS = 64;
   172	             Value arg_buffer[MAX_NATIVE_ARGS];
   173	
   174	             arg_buffer[0] = receiver; 
   175	             size_t copy_count = std::min(static_cast<size_t>(argc), MAX_NATIVE_ARGS - 1);
   176	             if (copy_count > 0) std::copy_n(regs + arg_start, copy_count, arg_buffer + 1);
   177	
   178	             Value result = method_val.as_native()(&state->machine, copy_count + 1, arg_buffer);
   179	
   180	             if (state->machine.has_error()) return impl_PANIC(start_ip, regs, constants, state);
   181	             if (dst != 0xFFFF) regs[dst] = result;
   182	             return next_ip;
   183	         } else {
   184	             state->error("INVOKE: Primitive method must be native.", ip);
   185	             return impl_PANIC(start_ip, regs, constants, state);
   186	         }
   187	    }
   188	
   189	    state->error(std::format("INVOKE: Method '{}' not found on object '{}'.", name->c_str(), to_string(receiver)), ip);
   190	    return impl_PANIC(start_ip, regs, constants, state);
   191	}
   192	
   193	[[gnu::always_inline]] 
   194	static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   195	    uint16_t dst = read_u16(ip);
   196	    uint16_t name_idx = read_u16(ip);
   197	    regs[dst] = Value(state->heap.new_class(constants[name_idx].as_string()));
   198	    return ip;
   199	}
   200	
   201	[[gnu::always_inline]] 
   202	static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   203	    uint16_t dst = read_u16(ip);
   204	    uint16_t class_reg = read_u16(ip);
   205	    Value& class_val = regs[class_reg];
   206	    
   207	    if (!class_val.is_class()) [[unlikely]] {
   208	        state->error("NEW_INSTANCE: Toán hạng không phải là Class.", ip);
   209	        return impl_PANIC(ip, regs, constants, state);
   210	    }
   211	    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
   212	    return ip;
   213	}
   214	
   215	// [[gnu::always_inline]] 
   216	static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   217	    const uint8_t* start_ip = ip - 1;
   218	
   219	    uint16_t dst = read_u16(ip);
   220	    uint16_t obj_reg = read_u16(ip);
   221	    uint16_t name_idx = read_u16(ip);
   222	    
   223	    InlineCache* ic = get_inline_cache(ip); 
   224	    Value& obj = regs[obj_reg];
   225	    string_t name = constants[name_idx].as_string();
   226	
   227	    // 1. Magic Prop: length (Xử lý trước vì rất phổ biến)
   228	    static string_t str_length = nullptr;
   229	    if (!str_length) [[unlikely]] str_length = state->heap.new_string("length");
   230	
   231	    if (name == str_length) {
   232	        if (obj.is_array()) {
   233	            regs[dst] = Value(static_cast<int64_t>(obj.as_array()->size()));
   234	            return ip;
   235	        }
   236	        if (obj.is_string()) {
   237	            regs[dst] = Value(static_cast<int64_t>(obj.as_string()->size()));
   238	            return ip;
   239	        }
   240	    }
   241	
   242	    // 2. Instance Access (IC Optimized)
   243	    if (obj.is_instance()) [[likely]] {
   244	        instance_t inst = obj.as_instance();
   245	        Shape* current_shape = inst->get_shape();
   246	
   247	        // IC Hit
   248	        if (ic->entries[0].shape == current_shape) {
   249	            regs[dst] = inst->get_field_at(ic->entries[0].offset);
   250	            return ip;
   251	        }
   252	        // Move-to-front check
   253	        if (ic->entries[1].shape == current_shape) {
   254	            InlineCacheEntry temp = ic->entries[1];
   255	            ic->entries[1] = ic->entries[0];
   256	            ic->entries[0] = temp;
   257	            regs[dst] = inst->get_field_at(temp.offset);
   258	            return ip;
   259	        }
   260	        
   261	        // IC Miss
   262	        int offset = current_shape->get_offset(name);
   263	        if (offset != -1) {
   264	            update_inline_cache(ic, current_shape, nullptr, static_cast<uint32_t>(offset));
   265	            regs[dst] = inst->get_field_at(offset);
   266	            return ip;
   267	        }
   268	        
   269	        // Lookup Class Methods
   270	        class_t k = inst->get_class();
   271	        while (k) {
   272	            if (k->has_method(name)) {
   273	                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
   274	                return ip;
   275	            }
   276	            k = k->get_super();
   277	        }
   278	    }
   279	    // 3. Array/String Primitive Access (IC Optimized - NEW!)
   280	    else if (obj.is_array() || obj.is_string()) {
   281	        const Shape* sentinel = obj.is_array() ? PrimitiveShapes::ARRAY : PrimitiveShapes::STRING;
   282	        
   283	        // A. Check Cache
   284	        if (ic->entries[0].shape == sentinel) {
   285	            module_t mod = get_core_module(state, obj.is_array() ? CoreModType::ARRAY : CoreModType::STRING);
   286	            
   287	            // Fast Access via Index (O(1))
   288	            Value method = mod->get_export_by_index(ic->entries[0].offset);
   289	            regs[dst] = Value(state->heap.new_bound_method(obj, method));
   290	            return ip;
   291	        }
   292	
   293	        // B. Cache Miss
   294	        int32_t idx = -1;
   295	        Value method = find_primitive_method_slow(state, obj, name, &idx);
   296	        
   297	        if (!method.is_null()) {
   298	            if (idx != -1) {
   299	                // Update Cache: Lưu index vào offset và Sentinel Shape
   300	                update_inline_cache(ic, sentinel, nullptr, static_cast<uint32_t>(idx));
   301	            }
   302	            regs[dst] = Value(state->heap.new_bound_method(obj, method));
   303	            return ip;
   304	        }
   305	    }
   306	    // 4. Hash Table & Module
   307	    else if (obj.is_hash_table()) {
   308	        hash_table_t hash = obj.as_hash_table();
   309	        if (hash->get(name, &regs[dst])) return ip;
   310	        
   311	        // Check "object" module methods
   312	        int32_t idx = -1;
   313	        Value method = find_primitive_method_slow(state, obj, name, &idx);
   314	        if (!method.is_null()) {
   315	            regs[dst] = Value(state->heap.new_bound_method(obj, method));
   316	            return ip;
   317	        }
   318	        regs[dst] = Value(null_t{});
   319	        return ip;
   320	    }
   321	    else if (obj.is_module()) {
   322	        module_t mod = obj.as_module();
   323	        // Có thể tối ưu IC ở đây tương tự như Array nếu muốn
   324	        if (mod->has_export(name)) {
   325	            regs[dst] = mod->get_export(name);
   326	            return ip;
   327	        }
   328	    }
   329	    else if (obj.is_class()) {
   330	        class_t k = obj.as_class();
   331	        if (k->has_method(name)) {
   332	            regs[dst] = k->get_method(name); 
   333	            return ip;
   334	        }
   335	    }
   336	    else if (obj.is_null()) [[unlikely]] {
   337	        state->ctx.current_frame_->ip_ = start_ip;
   338	        state->error(std::format("Runtime Error: Cannot read property '{}' of null.", name->c_str()), ip);
   339	        return impl_PANIC(ip, regs, constants, state);
   340	    }
   341	
   342	    state->ctx.current_frame_->ip_ = start_ip;
   343	    state->error(std::format("Runtime Error: Property '{}' not found on type '{}'.", 
   344	        name->c_str(), to_string(obj)));
   345	    return impl_PANIC(ip, regs, constants, state);
   346	}
   347	
   348	[[gnu::always_inline]] 
   349	static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   350	    const uint8_t* start_ip = ip - 1;
   351	
   352	    uint16_t obj_reg = read_u16(ip);
   353	    uint16_t name_idx = read_u16(ip);
   354	    uint16_t val_reg = read_u16(ip);
   355	    
   356	    InlineCache* ic = get_inline_cache(ip);
   357	    Value& obj = regs[obj_reg];
   358	    Value& val = regs[val_reg];
   359	    
   360	    if (obj.is_instance()) [[likely]] {
   361	        instance_t inst = obj.as_instance();
   362	        Shape* current_shape = inst->get_shape();
   363	
   364	        // IC Check
   365	        for (int i = 0; i < IC_CAPACITY; ++i) {
   366	            if (ic->entries[i].shape == current_shape) {
   367	                if (ic->entries[i].transition) { // Transition
   368	                    inst->set_shape(const_cast<Shape*>(ic->entries[i].transition));
   369	                    inst->add_field(val); 
   370	                    state->heap.write_barrier(inst, val);
   371	                    return ip;
   372	                }
   373	                // Update
   374	                inst->set_field_at(ic->entries[i].offset, val);
   375	                state->heap.write_barrier(inst, val);
   376	                return ip;
   377	            }
   378	        }
   379	
   380	        string_t name = constants[name_idx].as_string();
   381	        int offset = current_shape->get_offset(name);
   382	
   383	        if (offset != -1) { // Update existing
   384	            update_inline_cache(ic, current_shape, nullptr, static_cast<uint32_t>(offset));
   385	            inst->set_field_at(offset, val);
   386	            state->heap.write_barrier(inst, val);
   387	        } 
   388	        else { // Transition new
   389	            Shape* next_shape = current_shape->get_transition(name);
   390	            if (!next_shape) next_shape = current_shape->add_transition(name, &state->heap);
   391	            
   392	            uint32_t new_offset = static_cast<uint32_t>(inst->get_field_count());
   393	            update_inline_cache(ic, current_shape, next_shape, new_offset);
   394	
   395	            inst->set_shape(next_shape);
   396	            inst->add_field(val);
   397	            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape))); 
   398	            state->heap.write_barrier(inst, val);
   399	        }
   400	        return ip;
   401	    }
   402	    else if (obj.is_hash_table()) {
   403	        string_t name = constants[name_idx].as_string();
   404	        obj.as_hash_table()->set(name, val);
   405	        state->heap.write_barrier(obj.as_object(), val);
   406	    }
   407	    else {
   408	        state->ctx.current_frame_->ip_ = start_ip;
   409	        state->error(std::format("SET_PROP: Cannot set property '{}' on type '{}'.", 
   410	            constants[name_idx].as_string()->c_str(), to_string(obj)), ip);
   411	        return impl_PANIC(ip, regs, constants, state);
   412	    }
   413	    return ip;
   414	}
   415	
   416	[[gnu::always_inline]] 
   417	static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   418	    uint16_t class_reg = read_u16(ip);
   419	    uint16_t name_idx = read_u16(ip);
   420	    uint16_t method_reg = read_u16(ip);
   421	    
   422	    Value& class_val = regs[class_reg];
   423	    Value& method_val = regs[method_reg];
   424	    
   425	    if (!class_val.is_class()) [[unlikely]] return impl_PANIC(ip, regs, constants, state); // Error simplified
   426	    
   427	    class_val.as_class()->set_method(constants[name_idx].as_string(), method_val);
   428	    state->heap.write_barrier(class_val.as_class(), method_val);
   429	    return ip;
   430	}
   431	
   432	[[gnu::always_inline]] 
   433	static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   434	    uint16_t sub_reg = read_u16(ip);
   435	    uint16_t super_reg = read_u16(ip);
   436	    
   437	    Value& sub_val = regs[sub_reg];
   438	    Value& super_val = regs[super_reg];
   439	    
   440	    if (!sub_val.is_class() || !super_val.is_class()) [[unlikely]] {
   441	        state->error("INHERIT: Both operands must be Classes.", ip);
   442	        return impl_PANIC(ip, regs, constants, state);
   443	    }
   444	    
   445	    sub_val.as_class()->set_super(super_val.as_class());
   446	    return ip;
   447	}
   448	
   449	[[gnu::always_inline]] 
   450	static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   451	    const uint8_t* start_ip = ip - 1;
   452	    uint16_t dst = read_u16(ip);
   453	    uint16_t name_idx = read_u16(ip);
   454	    string_t name = constants[name_idx].as_string();
   455	    
   456	    Value& receiver_val = regs[0]; 
   457	    if (!receiver_val.is_instance()) [[unlikely]] return impl_PANIC(ip, regs, constants, state);
   458	    
   459	    instance_t receiver = receiver_val.as_instance();
   460	    class_t super = receiver->get_class()->get_super();
   461	    
   462	    if (!super) {
   463	        state->error("GET_SUPER: Class has no superclass.", ip);
   464	        return impl_PANIC(ip, regs, constants, state);
   465	    }
   466	    
   467	    class_t k = super;
   468	    while (k) {
   469	        if (k->has_method(name)) {
   470	            regs[dst] = Value(state->heap.new_bound_method(receiver, k->get_method(name).as_function()));
   471	            return ip;
   472	        }
   473	        k = k->get_super();
   474	    }
   475	    
   476	    state->ctx.current_frame_->ip_ = start_ip;
   477	    state->error(std::format("GET_SUPER: Method '{}' not found in superclass.", name->c_str()), ip);
   478	    return impl_PANIC(ip, regs, constants, state);
   479	}
   480	
   481	} // namespace meow::handlers


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


