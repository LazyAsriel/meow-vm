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
   172	    [[gnu::always_inline, gnu::hot]]
   173	    inline static const uint8_t* impl_JUMP_IF_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   174	        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
   175	        Value& cond = regs[args.cond];
   176	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   177	        if (truthy) return state->instruction_base + args.offset;
   178	        return ip + sizeof(JumpCondArgsB);
   179	    }
   180	
   181	    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   182	        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
   183	        Value& cond = regs[args.cond];
   184	        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
   185	        if (!truthy) return state->instruction_base + args.offset;
   186	        return ip + sizeof(JumpCondArgsB);
   187	    }
   188	
   189	    [[gnu::always_inline]]
   190	    inline static const uint8_t* impl_RETURN(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   191	        uint16_t ret_reg_idx = read_u16(ip);
   192	        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : regs[ret_reg_idx];
   193	
   194	        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
   195	        meow::close_upvalues(state->ctx, base_idx);
   196	
   197	        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) [[unlikely]] {
   198	            return nullptr; 
   199	        }
   200	
   201	        CallFrame* popped_frame = state->ctx.frame_ptr_;
   202	        
   203	        if (state->current_module) [[likely]] {
   204	             if (popped_frame->function_->get_proto() == state->current_module->get_main_proto()) [[unlikely]] {
   205	                 state->current_module->set_executed();
   206	             }
   207	        }
   208	
   209	        state->ctx.frame_ptr_--;
   210	        CallFrame* caller = state->ctx.frame_ptr_;
   211	        
   212	        state->ctx.stack_top_ = popped_frame->regs_base_;
   213	        state->ctx.current_regs_ = caller->regs_base_;
   214	        state->ctx.current_frame_ = caller; 
   215	        
   216	        state->update_pointers(); 
   217	
   218	        if (popped_frame->ret_dest_ != nullptr) {
   219	            *popped_frame->ret_dest_ = result;
   220	        }
   221	
   222	        return popped_frame->ip_; 
   223	    }
   224	
   225	    template <bool IsVoid>
   226	    [[gnu::always_inline]] 
   227	    static inline const uint8_t* do_call(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   228	        const uint8_t* start_ip = ip - 1; 
   229	
   230	        uint16_t dst = 0;
   231	        if constexpr (!IsVoid) dst = read_u16(ip);
   232	        uint16_t fn_reg    = read_u16(ip);
   233	        uint16_t arg_start = read_u16(ip);
   234	        uint16_t argc      = read_u16(ip);
   235	
   236	        CallIC* ic = get_call_ic(ip);
   237	        Value& callee = regs[fn_reg];
   238	
   239	        if (callee.is_function()) [[likely]] {
   240	            function_t closure = callee.as_function();
   241	            proto_t proto = closure->get_proto();
   242	
   243	            if (ic->check_tag == proto) [[likely]] {
   244	                size_t num_params = proto->get_num_registers();
   245	                
   246	                if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
   247	                    state->ctx.current_frame_->ip_ = start_ip;
   248	                    state->error("Stack Overflow!");
   249	                    return impl_PANIC(ip, regs, constants, state);
   250	                }
   251	
   252	                Value* new_base = state->ctx.stack_top_;
   253	                
   254	                size_t copy_count = (argc < num_params) ? argc : num_params;
   255	                for (size_t i = 0; i < copy_count; ++i) {
   256	                    new_base[i] = regs[arg_start + i];
   257	                }
   258	
   259	                for (size_t i = copy_count; i < num_params; ++i) {
   260	                    new_base[i] = Value(null_t{});
   261	                }
   262	
   263	                state->ctx.frame_ptr_++;
   264	                *state->ctx.frame_ptr_ = CallFrame(
   265	                    closure,
   266	                    new_base,
   267	                    IsVoid ? nullptr : &regs[dst], 
   268	                    ip 
   269	                );
   270	                
   271	                state->ctx.current_regs_ = new_base;
   272	                state->ctx.stack_top_ += num_params;
   273	                state->ctx.current_frame_ = state->ctx.frame_ptr_;
   274	                state->update_pointers(); 
   275	
   276	                return state->instruction_base;
   277	            }
   278	            
   279	            ic->check_tag = proto;
   280	        } 
   281	        
   282	        if (callee.is_native()) {
   283	            native_t fn = callee.as_native();
   284	            if (ic->check_tag != (void*)fn) {
   285	                ic->check_tag = (void*)fn;
   286	            }
   287	
   288	            Value result = fn(&state->machine, argc, &regs[arg_start]);
   289	            
   290	            if (state->machine.has_error()) [[unlikely]] {
   291	                state->error(std::string(state->machine.get_error_message()));
   292	                state->machine.clear_error();
   293	                return impl_PANIC(ip, regs, constants, state);
   294	            }
   295	
   296	            if constexpr (!IsVoid) {
   297	                if (dst != 0xFFFF) regs[dst] = result;
   298	            }
   299	            return ip;
   300	        }
   301	
   302	        Value* ret_dest_ptr = nullptr;
   303	        if constexpr (!IsVoid) {
   304	            if (dst != 0xFFFF) ret_dest_ptr = &regs[dst];
   305	        }
   306	
   307	        instance_t self = nullptr;
   308	        function_t closure = nullptr;
   309	        bool is_init = false;
   310	
   311	        if (callee.is_function()) {
   312	            closure = callee.as_function();
   313	        } 
   314	        else if (callee.is_bound_method()) {
   315	            bound_method_t bound = callee.as_bound_method();
   316	            Value receiver = bound->get_receiver();
   317	            Value method = bound->get_method();
   318	
   319	            if (method.is_native()) {
   320	                native_t fn = method.as_native();
   321	                
   322	                std::vector<Value> args;
   323	                args.reserve(argc + 1);
   324	                args.push_back(receiver);
   325	                
   326	                for (size_t i = 0; i < argc; ++i) {
   327	                    args.push_back(regs[arg_start + i]);
   328	                }
   329	
   330	                Value result = fn(&state->machine, static_cast<int>(args.size()), args.data());
   331	                
   332	                if (state->machine.has_error()) {
   333	                     return impl_PANIC(ip, regs, constants, state);
   334	                }
   335	
   336	                if constexpr (!IsVoid) regs[dst] = result;
   337	                return ip;
   338	            }
   339	            else if (method.is_function()) {
   340	                closure = method.as_function();
   341	                if (receiver.is_instance()) self = receiver.as_instance();
   342	            }
   343	        }
   344	        else if (callee.is_class()) {
   345	            class_t klass = callee.as_class();
   346	            self = state->heap.new_instance(klass, state->heap.get_empty_shape());
   347	            if (ret_dest_ptr) *ret_dest_ptr = Value(self);
   348	            
   349	            Value init_method = klass->get_method(state->heap.new_string("init"));
   350	            if (init_method.is_function()) {
   351	                closure = init_method.as_function();
   352	                is_init = true;
   353	            } else {
   354	                return ip; 
   355	            }
   356	        } 
   357	        else [[unlikely]] {
   358	            state->ctx.current_frame_->ip_ = start_ip;
   359	            state->error(std::format("Giá trị loại '{}' không thể gọi được (Not callable).", to_string(callee)));
   360	            return impl_PANIC(ip, regs, constants, state);
   361	        }
   362	
   363	        proto_t proto = closure->get_proto();
   364	        size_t num_params = proto->get_num_registers();
   365	
   366	        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
   367	            state->ctx.current_frame_->ip_ = start_ip;
   368	            state->error("Stack Overflow!");
   369	            return impl_PANIC(ip, regs, constants, state);
   370	        }
   371	
   372	        Value* new_base = state->ctx.stack_top_;
   373	        size_t arg_offset = 0;
   374	        
   375	        if (self != nullptr && num_params > 0) {
   376	            new_base[0] = Value(self);
   377	            arg_offset = 1;
   378	        }
   379	
   380	        for (size_t i = 0; i < argc; ++i) {
   381	            if (arg_offset + i < num_params) {
   382	                new_base[arg_offset + i] = regs[arg_start + i];
   383	            }
   384	        }
   385	
   386	        size_t filled_count = arg_offset + argc;
   387	        if (filled_count > num_params) filled_count = num_params;
   388	
   389	        for (size_t i = filled_count; i < num_params; ++i) {
   390	            new_base[i] = Value(null_t{});
   391	        }
   392	
   393	        state->ctx.frame_ptr_++;
   394	        *state->ctx.frame_ptr_ = CallFrame(
   395	            closure,
   396	            new_base,                          
   397	            is_init ? nullptr : ret_dest_ptr,  
   398	            ip                                 
   399	        );
   400	
   401	        state->ctx.current_regs_ = new_base;
   402	        state->ctx.stack_top_ += num_params;
   403	        state->ctx.current_frame_ = state->ctx.frame_ptr_;
   404	        state->update_pointers(); 
   405	
   406	        return state->instruction_base;
   407	    }
   408	
   409	    [[gnu::always_inline]] inline static const uint8_t* impl_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   410	        return do_call<false>(ip, regs, constants, state);
   411	    }
   412	
   413	    [[gnu::always_inline]] inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   414	        return do_call<true>(ip, regs, constants, state);
   415	    }
   416	
   417	[[gnu::always_inline]] 
   418	static const uint8_t* impl_TAIL_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   419	    const uint8_t* start_ip = ip - 1;
   420	
   421	    uint16_t dst = read_u16(ip); (void)dst;
   422	    uint16_t fn_reg = read_u16(ip);
   423	    uint16_t arg_start = read_u16(ip);
   424	    uint16_t argc = read_u16(ip);
   425	    
   426	    ip += 16;
   427	
   428	    Value& callee = regs[fn_reg];
   429	    if (!callee.is_function()) [[unlikely]] {
   430	        state->ctx.current_frame_->ip_ = start_ip;
   431	        state->error("TAIL_CALL: Target không phải là Function.");
   432	        return nullptr;
   433	    }
   434	
   435	    function_t closure = callee.as_function();
   436	    proto_t proto = closure->get_proto();
   437	    size_t num_params = proto->get_num_registers();
   438	
   439	    size_t current_base_idx = regs - state->ctx.stack_;
   440	    meow::close_upvalues(state->ctx, current_base_idx);
   441	
   442	    size_t copy_count = (argc < num_params) ? argc : num_params;
   443	
   444	    for (size_t i = 0; i < copy_count; ++i) {
   445	        regs[i] = regs[arg_start + i];
   446	    }
   447	
   448	    for (size_t i = copy_count; i < num_params; ++i) {
   449	        regs[i] = Value(null_t{});
   450	    }
   451	
   452	    CallFrame* current_frame = state->ctx.frame_ptr_;
   453	    current_frame->function_ = closure;
   454	    
   455	    state->ctx.stack_top_ = regs + num_params;
   456	    state->update_pointers();
   457	
   458	    return proto->get_chunk().get_code();
   459	}
   460	
   461	} // namespace meow::handlers


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


