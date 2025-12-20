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
   191	}
   192	
   193	} // namespace meow


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
   109	        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
   110	            state->error("Array index out of bounds.");
   111	            return impl_PANIC(ip, regs, constants, state);
   112	        }
   113	        regs[dst] = arr->get(idx);
   114	    } 
   115	    else if (src.is_hash_table()) {
   116	        hash_table_t hash = src.as_hash_table();
   117	        string_t k = nullptr;
   118	        
   119	        if (!key.is_string()) {
   120	            std::string s = to_string(key);
   121	            k = state->heap.new_string(s);
   122	        } else {
   123	            k = key.as_string();
   124	        }
   125	
   126	        if (hash->has(k)) {
   127	            regs[dst] = hash->get(k);
   128	        } else {
   129	            regs[dst] = Value(null_t{});
   130	        }
   131	    }
   132	    else if (src.is_string()) {
   133	        if (!key.is_int()) {
   134	            state->error("String index phải là số nguyên.");
   135	            return impl_PANIC(ip, regs, constants, state);
   136	        }
   137	        string_t str = src.as_string();
   138	        int64_t idx = key.as_int();
   139	        if (idx < 0 || static_cast<size_t>(idx) >= str->size()) {
   140	            state->error("String index out of bounds.");
   141	            return impl_PANIC(ip, regs, constants, state);
   142	        }
   143	        char c = str->get(idx);
   144	        regs[dst] = Value(state->heap.new_string(&c, 1));
   145	    }
   146	
   147	    else if (src.is_instance()) {
   148	        if (!key.is_string()) {
   149	            state->error("Instance index key phải là chuỗi (tên thuộc tính/phương thức).");
   150	            return impl_PANIC(ip, regs, constants, state);
   151	        }
   152	        
   153	        string_t name = key.as_string();
   154	        instance_t inst = src.as_instance();
   155	        
   156	        int offset = inst->get_shape()->get_offset(name);
   157	        if (offset != -1) {
   158	            regs[dst] = inst->get_field_at(offset);
   159	        } 
   160	        else {
   161	            class_t k = inst->get_class();
   162	            Value method = null_t{};
   163	            
   164	            while (k) {
   165	                if (k->has_method(name)) {
   166	                    method = k->get_method(name);
   167	                    break;
   168	                }
   169	                k = k->get_super();
   170	            }
   171	            
   172	            if (!method.is_null()) {
   173	                if (method.is_function() || method.is_native()) {
   174	                    auto bound = state->heap.new_bound_method(src, method);
   175	                    regs[dst] = Value(bound);
   176	                } else {
   177	                    regs[dst] = method;
   178	                }
   179	            } else {
   180	                regs[dst] = Value(null_t{});
   181	            }
   182	        }
   183	    }
   184	
   185	    else {
   186	        state->error("Không thể dùng toán tử index [] trên kiểu dữ liệu này.");
   187	        return impl_PANIC(ip, regs, constants, state);
   188	    }
   189	    return ip;
   190	}
   191	
   192	[[gnu::always_inline]] static const uint8_t* impl_SET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   193	    uint16_t src_reg = read_u16(ip);
   194	    uint16_t key_reg = read_u16(ip);
   195	    uint16_t val_reg = read_u16(ip);
   196	
   197	    Value& src = regs[src_reg];
   198	    Value& key = regs[key_reg];
   199	    Value& val = regs[val_reg];
   200	
   201	    if (src.is_array()) {
   202	        if (!key.is_int()) {
   203	            state->error("Array index phải là số nguyên.");
   204	            return impl_PANIC(ip, regs, constants, state);
   205	        }
   206	        array_t arr = src.as_array();
   207	        int64_t idx = key.as_int();
   208	        if (idx < 0) {
   209	            state->error("Array index không được âm.");
   210	            return impl_PANIC(ip, regs, constants, state);
   211	        }
   212	        if (static_cast<size_t>(idx) >= arr->size()) {
   213	            arr->resize(idx + 1);
   214	        }
   215	        
   216	        arr->set(idx, val);
   217	        state->heap.write_barrier(src.as_object(), val);
   218	    }
   219	    else if (src.is_hash_table()) {
   220	        hash_table_t hash = src.as_hash_table();
   221	        string_t k = nullptr;
   222	
   223	        if (!key.is_string()) {
   224	            std::string s = to_string(key);
   225	            k = state->heap.new_string(s);
   226	        } else {
   227	            k = key.as_string();
   228	        }
   229	
   230	        hash->set(k, val);
   231	        
   232	        state->heap.write_barrier(src.as_object(), val);
   233	    } 
   234	        else if (src.is_instance()) {
   235	        if (!key.is_string()) {
   236	            state->error("Instance set index key phải là chuỗi.");
   237	            return impl_PANIC(ip, regs, constants, state);
   238	        }
   239	        
   240	        string_t name = key.as_string();
   241	        instance_t inst = src.as_instance();
   242	        
   243	        int offset = inst->get_shape()->get_offset(name);
   244	        if (offset != -1) {
   245	            inst->set_field_at(offset, val);
   246	            state->heap.write_barrier(inst, val);
   247	        } else {
   248	            Shape* current_shape = inst->get_shape();
   249	            Shape* next_shape = current_shape->get_transition(name);
   250	            if (next_shape == nullptr) {
   251	                next_shape = current_shape->add_transition(name, &state->heap);
   252	            }
   253	            
   254	            inst->set_shape(next_shape);
   255	            
   256	            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape)));
   257	
   258	            inst->add_field(val);
   259	            state->heap.write_barrier(inst, val);
   260	        }
   261	    }
   262	    else {
   263	        state->error("Không thể gán index [] trên kiểu dữ liệu này.");
   264	        return impl_PANIC(ip, regs, constants, state);
   265	    }
   266	    return ip;
   267	}
   268	[[gnu::always_inline]] static const uint8_t* impl_GET_KEYS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   269	    uint16_t dst = read_u16(ip);
   270	    uint16_t src_reg = read_u16(ip);
   271	    Value& src = regs[src_reg];
   272	    
   273	    auto keys_array = state->heap.new_array();
   274	    
   275	    if (src.is_hash_table()) {
   276	        hash_table_t hash = src.as_hash_table();
   277	        keys_array->reserve(hash->size());
   278	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   279	            keys_array->push(Value(it->first));
   280	        }
   281	    } else if (src.is_array()) {
   282	        size_t sz = src.as_array()->size();
   283	        keys_array->reserve(sz);
   284	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   285	    } else if (src.is_string()) {
   286	        size_t sz = src.as_string()->size();
   287	        keys_array->reserve(sz);
   288	        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
   289	    }
   290	    
   291	    regs[dst] = Value(keys_array);
   292	    return ip;
   293	}
   294	
   295	[[gnu::always_inline]] static const uint8_t* impl_GET_VALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
   296	    uint16_t dst = read_u16(ip);
   297	    uint16_t src_reg = read_u16(ip);
   298	    Value& src = regs[src_reg];
   299	    
   300	    auto vals_array = state->heap.new_array();
   301	
   302	    if (src.is_hash_table()) {
   303	        hash_table_t hash = src.as_hash_table();
   304	        vals_array->reserve(hash->size());
   305	        for (auto it = hash->begin(); it != hash->end(); ++it) {
   306	            vals_array->push(it->second);
   307	        }
   308	    } else if (src.is_array()) {
   309	        array_t arr = src.as_array();
   310	        vals_array->reserve(arr->size());
   311	        for (size_t i = 0; i < arr->size(); ++i) vals_array->push(arr->get(i));
   312	    } else if (src.is_string()) {
   313	        string_t str = src.as_string();
   314	        vals_array->reserve(str->size());
   315	        for (size_t i = 0; i < str->size(); ++i) {
   316	            char c = str->get(i);
   317	            vals_array->push(Value(state->heap.new_string(&c, 1)));
   318	        }
   319	    }
   320	
   321	    regs[dst] = Value(vals_array);
   322	    return ip;
   323	}
   324	
   325	} // namespace meow::handlers


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
   196	    static bool is_ran = false;
   197	    if (!is_ran) {
   198	        std::println("Đang dùng NEW_CLASS OpCode (chỉ hiện log một lần)");
   199	        is_ran = true;
   200	    }
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
    23	}



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


