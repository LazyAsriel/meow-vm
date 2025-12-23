// =============================================================================
//  FILE PATH: include/meow/bytecode/chunk.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <vector>
     5	#include <string>
     6	#include <memory>
     7	#include <meow/common.h>
     8	#include <meow/value.h>
     9	
    10	namespace meow {
    11	class Chunk {
    12	public:
    13	    Chunk() = default;
    14	    Chunk(std::vector<uint8_t>&& code, std::vector<Value>&& constants) noexcept : code_(std::move(code)), constant_pool_(std::move(constants)) {}
    15	
    16	    // --- Modifiers ---
    17	    inline void write_byte(uint8_t byte) {
    18	        code_.push_back(byte);
    19	    }
    20	
    21	    inline void write_u16(uint16_t value) {
    22	        code_.push_back(static_cast<uint8_t>(value & 0xFF));
    23	        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    24	    }
    25	
    26	    inline void write_u32(uint32_t value) {
    27	        code_.push_back(static_cast<uint8_t>(value & 0xFF));
    28	        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    29	        code_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    30	        code_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    31	    }
    32	
    33	    inline void write_u64(uint64_t value) {
    34	        code_.push_back(static_cast<uint8_t>(value & 0xFF));
    35	        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    36	        code_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    37	        code_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    38	        code_.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    39	        code_.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    40	        code_.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    41	        code_.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    42	    }
    43	
    44	    inline void write_f64(double value) {
    45	        write_u64(std::bit_cast<uint64_t>(value));
    46	    }
    47	    // --- Code buffer ---
    48	    inline const uint8_t* get_code() const noexcept {
    49	        return code_.data();
    50	    }
    51	    inline size_t get_code_size() const noexcept {
    52	        return code_.size();
    53	    }
    54	    inline bool is_code_empty() const noexcept {
    55	        return code_.empty();
    56	    }
    57	
    58	    // --- Constant pool ---
    59	    inline size_t get_pool_size() const noexcept {
    60	        return constant_pool_.size();
    61	    }
    62	    inline bool is_pool_empty() const noexcept {
    63	        return constant_pool_.empty();
    64	    }
    65	    inline size_t add_constant(param_t value) {
    66	        constant_pool_.push_back(value);
    67	        return constant_pool_.size() - 1;
    68	    }
    69	    inline return_t get_constant(size_t index) const noexcept {
    70	        return constant_pool_[index];
    71	    }
    72	    inline value_t& get_constant_ref(size_t index) noexcept {
    73	        return constant_pool_[index];
    74	    }
    75	    inline const Value* get_constants_raw() const noexcept {
    76	        return constant_pool_.data();
    77	    }
    78	
    79	    inline bool patch_u16(size_t offset, uint16_t value) noexcept {
    80	        if (offset + 1 >= code_.size()) return false;
    81	
    82	        code_[offset] = static_cast<uint8_t>(value & 0xFF);
    83	        code_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    84	
    85	        return true;
    86	    }
    87	private:
    88	    std::vector<uint8_t> code_;
    89	    std::vector<Value> constant_pool_;
    90	};
    91	}


// =============================================================================
//  FILE PATH: include/meow/bytecode/disassemble.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <string>
     5	#include <utility>
     6	
     7	namespace meow { 
     8	    class Chunk; 
     9	}
    10	
    11	namespace meow {
    12	
    13	    /**
    14	     * @brief Disassemble toàn bộ chunk thành chuỗi (phiên bản tối ưu).
    15	     */
    16	    std::string disassemble_chunk(const Chunk& chunk, const char* name = nullptr) noexcept;
    17	
    18	    /**
    19	     * @brief Disassemble một instruction.
    20	     * @return Pair: {Chuỗi kết quả, Offset của lệnh tiếp theo}
    21	     */
    22	    std::pair<std::string, size_t> disassemble_instruction(const Chunk& chunk, size_t offset) noexcept;
    23	
    24	    /**
    25	     * @brief Context Disassembly: In ra vùng code xung quanh IP để debug lỗi crash.
    26	     * Tự động căn chỉnh (align) instruction để không in rác.
    27	     */
    28	    std::string disassemble_around(const Chunk& chunk, size_t ip, int context_lines = 10) noexcept;
    29	}


// =============================================================================
//  FILE PATH: include/meow/bytecode/op_codes.h
// =============================================================================

     1	/**
     2	 * @file op_codes.h
     3	 * @author LazyPaws
     4	 * @brief Declaration of operating code in TrangMeo
     5	 * @copyright Copyright (c) 2025 LazyPaws
     6	 * @license All rights reserved. Unauthorized copying of this file, in any form
     7	 * or medium, is strictly prohibited
     8	 */
     9	
    10	#pragma once
    11	
    12	namespace meow {
    13	enum class OpCode : unsigned char {
    14	    LOAD_CONST, LOAD_NULL, LOAD_TRUE, LOAD_FALSE, LOAD_INT, LOAD_FLOAT, MOVE,
    15	    INC, DEC,
    16	    __BEGIN_OPERATOR__,
    17	    ADD, SUB, MUL, DIV, MOD, POW,
    18	    EQ, NEQ, GT, GE, LT, LE,
    19	    NEG, NOT,
    20	    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, LSHIFT, RSHIFT,
    21	    __END_OPERATOR__,
    22	    GET_GLOBAL, SET_GLOBAL,
    23	    GET_UPVALUE, SET_UPVALUE,
    24	    CLOSURE, CLOSE_UPVALUES,
    25	    JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE,
    26	    CALL, CALL_VOID, RETURN, HALT,
    27	    NEW_ARRAY, NEW_HASH, GET_INDEX, SET_INDEX, GET_KEYS, GET_VALUES,
    28	    NEW_CLASS, NEW_INSTANCE, GET_PROP, SET_PROP,
    29	    SET_METHOD, INHERIT, GET_SUPER,
    30	    INVOKE,
    31	    THROW, SETUP_TRY, POP_TRY,
    32	    IMPORT_MODULE, EXPORT, GET_EXPORT, IMPORT_ALL,
    33	
    34	    TAIL_CALL,
    35	
    36	    // Optimized Byte-operand instructions
    37	    ADD_B, SUB_B, MUL_B, DIV_B, MOD_B,
    38	    EQ_B, NEQ_B, GT_B, GE_B, LT_B, LE_B,
    39	    JUMP_IF_TRUE_B, JUMP_IF_FALSE_B,
    40	    MOVE_B, LOAD_INT_B,
    41	    BIT_AND_B, BIT_OR_B, BIT_XOR_B, 
    42	    LSHIFT_B, RSHIFT_B,
    43	
    44	    TOTAL_OPCODES
    45	};
    46	}


// =============================================================================
//  FILE PATH: include/meow/cast.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <vector>
     5	#include <string>
     6	#include <memory>
     7	#include <charconv>
     8	#include <system_error>
     9	#include <cmath>
    10	#include <algorithm>
    11	#include <cstdlib>
    12	#include <iostream>
    13	
    14	#include <meow/common.h>
    15	#include <meow/core/objects.h>
    16	#include <meow/value.h>
    17	#include "meow/bytecode/disassemble.h"
    18	
    19	namespace meow {
    20	
    21	inline constexpr bool is_space(char c) noexcept {
    22	    return c == ' ' || (c >= '\t' && c <= '\r');
    23	}
    24	
    25	inline constexpr char to_lower(char c) noexcept {
    26	    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    27	}
    28	
    29	inline constexpr std::string_view trim_whitespace(std::string_view sv) noexcept {
    30	    while (!sv.empty() && is_space(sv.front())) {
    31	        sv.remove_prefix(1);
    32	    }
    33	    while (!sv.empty() && is_space(sv.back())) {
    34	        sv.remove_suffix(1);
    35	    }
    36	    return sv;
    37	}
    38	
    39	inline int64_t to_int(param_t value) noexcept {
    40	    using i64_limits = std::numeric_limits<int64_t>;
    41	    
    42	    return value.visit(
    43	        [](null_t) -> int64_t { return 0; },
    44	        [](int_t i) -> int64_t { return i; },
    45	        [](float_t r) -> int64_t {
    46	            if (std::isnan(r)) return 0;
    47	            if (std::isinf(r)) [[unlikely]] return (r > 0) ? i64_limits::max() : i64_limits::min();
    48	            if (r >= static_cast<double>(i64_limits::max())) [[unlikely]] return i64_limits::max();
    49	            if (r <= static_cast<double>(i64_limits::min())) [[unlikely]] return i64_limits::min();
    50	            return static_cast<int64_t>(r);
    51	        },
    52	        [](bool_t b) -> int64_t { return b ? 1 : 0; },
    53	        
    54	        [](object_t obj) -> int64_t {
    55	            if (!obj) return 0;
    56	            
    57	            if (obj->get_type() == ObjectType::STRING) {
    58	                string_t s = reinterpret_cast<string_t>(obj);
    59	                const char* ptr = s->c_str();
    60	
    61	                while (is_space(*ptr)) ptr++;
    62	                
    63	                if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
    64	                    char* end;
    65	                    return static_cast<int64_t>(std::strtoll(ptr, &end, 16));
    66	                }
    67	
    68	                int64_t res = 0;
    69	                int sign = 1;
    70	                
    71	                if (*ptr == '-') { sign = -1; ptr++; } 
    72	                else if (*ptr == '+') { ptr++; }
    73	                
    74	                bool found_digit = false;
    75	                while (*ptr >= '0' && *ptr <= '9') {
    76	                    res = res * 10 + (*ptr - '0');
    77	                    ptr++;
    78	                    found_digit = true;
    79	                }
    80	                
    81	                if (found_digit) return res * sign;
    82	                return 0;
    83	            }
    84	            return 0;
    85	        },
    86	        [](auto&&) -> int64_t { return 0; }
    87	    );
    88	}
    89	
    90	inline double to_float(param_t value) noexcept {
    91	    return value.visit(
    92	        [](null_t) -> double { return 0.0; },
    93	        [](int_t i) -> double { return static_cast<double>(i); },
    94	        [](float_t f) -> double { return f; },
    95	        [](bool_t b) -> double { return b ? 1.0 : 0.0; },
    96	        
    97	        [](object_t obj) -> double {
    98	            if (!obj) return 0.0;
    99	
   100	            if (obj->get_type() == ObjectType::STRING) {
   101	                string_t s = reinterpret_cast<string_t>(obj);
   102	                std::string_view sv = s->c_str();
   103	                sv = trim_whitespace(sv);
   104	                if (sv.empty()) return 0.0;
   105	                
   106	                if (sv == "NaN" || sv == "nan") return std::numeric_limits<double>::quiet_NaN();
   107	                if (sv == "Infinity" || sv == "inf") return std::numeric_limits<double>::infinity();
   108	                if (sv == "-Infinity" || sv == "-inf") return -std::numeric_limits<double>::infinity();
   109	
   110	                double result = 0.0;
   111	                auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
   112	                
   113	                if (ec == std::errc::result_out_of_range) {
   114	                    return (sv.front() == '-') ? -std::numeric_limits<double>::infinity() 
   115	                                               : std::numeric_limits<double>::infinity();
   116	                }
   117	                if (ec == std::errc::invalid_argument) {
   118	                    char* end;
   119	                    result = std::strtod(s->c_str(), &end);
   120	                }
   121	                return result;
   122	            }
   123	            return 0.0;
   124	        },
   125	        [](auto&&) -> double { return 0.0; }
   126	    );
   127	}
   128	
   129	inline bool to_bool(param_t value) noexcept {
   130	    return value.visit(
   131	        [](null_t) -> bool { return false; },
   132	        [](int_t i) -> bool { return i != 0; },
   133	        [](float_t f) -> bool { return f != 0.0 && !std::isnan(f); },
   134	        [](bool_t b) -> bool { return b; },
   135	        
   136	        [](object_t obj) -> bool {
   137	            if (!obj) return false;
   138	
   139	            switch(obj->get_type()) {
   140	                case ObjectType::STRING:
   141	                    return !reinterpret_cast<string_t>(obj)->empty();
   142	                case ObjectType::ARRAY:
   143	                    return !reinterpret_cast<array_t>(obj)->empty();
   144	                case ObjectType::HASH_TABLE:
   145	                    return !reinterpret_cast<hash_table_t>(obj)->empty();
   146	                default:
   147	                    return true;
   148	            }
   149	        },
   150	        [](auto&&) -> bool { return true; }
   151	    );
   152	}
   153	
   154	inline std::string to_string(param_t value) noexcept;
   155	
   156	namespace detail {
   157	inline void object_to_string(object_t obj, std::string& out) noexcept {
   158	    if (obj == nullptr) {
   159	        out += "<null_object_ptr>";
   160	        return;
   161	    }
   162	    
   163	    switch (obj->get_type()) {
   164	        case ObjectType::STRING:
   165	            out += reinterpret_cast<string_t>(obj)->c_str();
   166	            break;
   167	
   168	        case ObjectType::ARRAY: {
   169	            array_t arr = reinterpret_cast<array_t>(obj);
   170	            out.push_back('[');
   171	            for (size_t i = 0; i < arr->size(); ++i) {
   172	                if (i > 0) out += ", ";
   173	                out += meow::to_string(arr->get(i));
   174	            }
   175	            out.push_back(']');
   176	            break;
   177	        }
   178	
   179	        case ObjectType::HASH_TABLE: {
   180	            hash_table_t hash = reinterpret_cast<hash_table_t>(obj);
   181	            out.push_back('{');
   182	            bool first = true;
   183	            for (const auto& [key, val] : *hash) {
   184	                if (!first) out += ", ";
   185	                std::format_to(std::back_inserter(out), "{}: {}", key->c_str(), meow::to_string(val));
   186	                first = false;
   187	            }
   188	            out.push_back('}');
   189	            break;
   190	        }
   191	
   192	        case ObjectType::CLASS: {
   193	            auto name = reinterpret_cast<class_t>(obj)->get_name();
   194	            std::format_to(std::back_inserter(out), "<class '{}'>", (name ? name->c_str() : "??"));
   195	            break;
   196	        }
   197	
   198	        case ObjectType::INSTANCE: {
   199	            auto name = reinterpret_cast<instance_t>(obj)->get_class()->get_name();
   200	            std::format_to(std::back_inserter(out), "<{} instance>", (name ? name->c_str() : "??"));
   201	            break;
   202	        }
   203	
   204	        case ObjectType::BOUND_METHOD:
   205	            out += "<bound_method>";
   206	            break;
   207	
   208	        case ObjectType::MODULE: {
   209	            auto name = reinterpret_cast<module_t>(obj)->get_file_name();
   210	            std::format_to(std::back_inserter(out), "<module '{}'>", (name ? name->c_str() : "??"));
   211	            break;
   212	        }
   213	
   214	        case ObjectType::FUNCTION: {
   215	            auto name = reinterpret_cast<function_t>(obj)->get_proto()->get_name();
   216	            std::format_to(std::back_inserter(out), "<function '{}'>", (name ? name->c_str() : "??"));
   217	            break;
   218	        }
   219	
   220	        case ObjectType::PROTO: {
   221	            auto proto = reinterpret_cast<proto_t>(obj);
   222	            auto name = proto->get_name();
   223	            std::format_to(std::back_inserter(out), 
   224	                "<proto '{}'>\n  - registers: {}\n  - upvalues:  {}\n  - constants: {}\n{}",
   225	                (name ? name->c_str() : "??"),
   226	                proto->get_num_registers(),
   227	                proto->get_num_upvalues(),
   228	                proto->get_chunk().get_pool_size(),
   229	                disassemble_chunk(proto->get_chunk())
   230	            );
   231	            break;
   232	        }
   233	
   234	        case ObjectType::UPVALUE:
   235	            out += "<upvalue>";
   236	            break;
   237	
   238	        default:
   239	            std::unreachable();
   240	            break;
   241	    }
   242	}
   243	} // namespace detail
   244	
   245	inline std::string to_string(param_t value) noexcept {
   246	    return value.visit(
   247	        [](null_t) -> std::string { return "null"; },
   248	        [](int_t val) -> std::string { return std::to_string(val); },
   249	        [](float_t val) -> std::string {
   250	            if (std::isnan(val)) return "NaN";
   251	            if (std::isinf(val)) return (val > 0) ? "Infinity" : "-Infinity";
   252	            if (val == 0.0 && std::signbit(val)) return "-0.0";
   253	            return std::format("{}", val); 
   254	        },
   255	        [](bool_t val) -> std::string { return val ? "true" : "false"; },
   256	        [](native_t) -> std::string { return "<native_fn>"; },
   257	        [](object_t obj) -> std::string {
   258	            std::string out;
   259	            out.reserve(64); 
   260	            detail::object_to_string(obj, out);
   261	            return out;
   262	        }
   263	    );
   264	}
   265	
   266	} // namespace meow


// =============================================================================
//  FILE PATH: include/meow/common.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <vector>
     5	#include <string>
     6	#include <memory>
     7	#include "meow_variant.h"
     8	
     9	namespace meow {
    10	struct MeowObject;
    11	class Machine;
    12	class Value;
    13	class ObjString;
    14	class ObjArray;
    15	class ObjHashTable;
    16	class ObjClass;
    17	class ObjInstance;
    18	class ObjBoundMethod;
    19	class ObjUpvalue;
    20	class ObjFunctionProto;
    21	class ObjNativeFunction;
    22	class ObjClosure;
    23	class ObjModule;
    24	class Shape;
    25	}
    26	
    27	namespace meow {    
    28	using value_t = Value;
    29	using param_t = value_t;
    30	using return_t = value_t;
    31	using mutable_t = value_t&;
    32	using arguments_t = std::vector<value_t>&;
    33	    
    34	using null_t = std::monostate;
    35	using bool_t = bool;
    36	using int_t = int64_t;
    37	using float_t = double;
    38	using native_t = value_t (*)(Machine* engine, int argc, value_t* argv);
    39	using pointer_t = void*;
    40	using object_t = MeowObject*;
    41	
    42	using array_t = ObjArray*;
    43	using string_t = ObjString*;
    44	using hash_table_t = ObjHashTable*;
    45	using instance_t = ObjInstance*;
    46	using class_t = ObjClass*;
    47	using bound_method_t = ObjBoundMethod*;
    48	using upvalue_t = ObjUpvalue*;
    49	using proto_t = ObjFunctionProto*;
    50	using function_t = ObjClosure*;
    51	using module_t = ObjModule*;
    52	using shape_t = Shape*;
    53	
    54	using base_t = meow::variant<null_t, bool_t, int_t, float_t, native_t, pointer_t, object_t>;
    55	
    56	enum class ValueType : uint8_t {
    57	    Null,
    58	    Bool,
    59	    Int,
    60	    Float,
    61	    NativeFn,
    62	    Object,
    63	
    64	    Array,        // 1  — ARRAY
    65	    String,       // 2  — STRING
    66	    HashTable,    // 3  — HASH_TABLE
    67	    Instance,     // 4  — INSTANCE
    68	    Class,        // 5  — CLASS
    69	    BoundMethod,  // 6  — BOUND_METHOD
    70	    Upvalue,      // 7  — UPVALUE
    71	    Proto,        // 8  — PROTO
    72	    Function,     // 9  — FUNCTION
    73	    Module,       // 10 — MODULE
    74	    Shape,        // 11 - SHAPE
    75	
    76	    TotalValueTypes
    77	};
    78	}



// =============================================================================
//  FILE PATH: include/meow/core/array.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <meow/core/meow_object.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/gc_visitor.h>
     6	#include <meow/memory/memory_manager.h>
     7	#include <cstdint>
     8	#include <vector>
     9	
    10	namespace meow {
    11	class ObjArray : public ObjBase<ObjectType::ARRAY> {
    12	public:
    13	    using container_t = std::vector<value_t>;
    14	private:
    15	    using visitor_t = GCVisitor;
    16	    container_t elements_;
    17	public:
    18	    explicit ObjArray() = default;
    19	
    20	    ObjArray(const std::vector<value_t>& elements)  : elements_(elements) {}
    21	
    22	    ObjArray(container_t&& elements) noexcept : elements_(std::move(elements)) {}
    23	
    24	    ObjArray(const ObjArray&) = delete;
    25	    ObjArray(ObjArray&&) = default;
    26	    ObjArray& operator=(const ObjArray&) = delete;
    27	    ObjArray& operator=(ObjArray&&) = delete;
    28	    ~ObjArray() override = default;
    29	
    30	    using iterator = container_t::iterator;
    31	    using const_iterator = container_t::const_iterator;
    32	    using reverse_iterator = container_t::reverse_iterator;
    33	    using const_reverse_iterator = container_t::const_reverse_iterator;
    34	    
    35	    template <typename Self>
    36	    inline decltype(auto) get(this Self&& self, size_t index) noexcept {
    37	        return std::forward<Self>(self).elements_[index]; 
    38	    }
    39	
    40	    template <typename Self>
    41	    inline decltype(auto) at(this Self&& self, size_t index) {
    42	        return std::forward<Self>(self).elements_.at(index);
    43	    }
    44	
    45	    template <typename Self>
    46	    inline decltype(auto) operator[](this Self&& self, size_t index) noexcept {
    47	        return std::forward<Self>(self).elements_[index];
    48	    }
    49	
    50	    template <typename Self>
    51	    inline decltype(auto) front(this Self&& self) noexcept {
    52	        return std::forward<Self>(self).elements_.front();
    53	    }
    54	
    55	    template <typename Self>
    56	    inline decltype(auto) back(this Self&& self) noexcept {
    57	        return std::forward<Self>(self).elements_.back();
    58	    }
    59	
    60	    template <typename T>
    61	    inline void set(size_t index, T&& value) noexcept {
    62	        elements_[index] = std::forward<T>(value);
    63	    }
    64	
    65	    inline size_t size() const noexcept { return elements_.size(); }
    66	    inline bool empty() const noexcept { return elements_.empty(); }
    67	    inline size_t capacity() const noexcept { return elements_.capacity(); }
    68	
    69	    template <typename T>
    70	    inline void push(T&& value) {
    71	        elements_.emplace_back(std::forward<T>(value));
    72	    }
    73	    inline void pop() noexcept { elements_.pop_back(); }
    74	    
    75	    template <typename... Args>
    76	    inline void emplace(Args&&... args) { elements_.emplace_back(std::forward<Args>(args)...); }
    77	    
    78	    inline void resize(size_t size) { elements_.resize(size); }
    79	    inline void reserve(size_t capacity) { elements_.reserve(capacity); }
    80	    inline void shrink() { elements_.shrink_to_fit(); }
    81	    inline void clear() { elements_.clear(); }
    82	
    83	    template <typename Self>
    84	    inline auto begin(this Self&& self) noexcept { return std::forward<Self>(self).elements_.begin(); }
    85	    
    86	    template <typename Self>
    87	    inline auto end(this Self&& self) noexcept { return std::forward<Self>(self).elements_.end(); }
    88	
    89	    template <typename Self>
    90	    inline auto rbegin(this Self&& self) noexcept { return std::forward<Self>(self).elements_.rbegin(); }
    91	
    92	    template <typename Self>
    93	    inline auto rend(this Self&& self) noexcept { return std::forward<Self>(self).elements_.rend(); }
    94	
    95	    void trace(GCVisitor& visitor) const noexcept override;
    96	};
    97	}


// =============================================================================
//  FILE PATH: include/meow/core/function.h
// =============================================================================

     1	/**
     2	 * @file function.h
     3	 * @author LazyPaws
     4	 * @brief Core definition of Upvalue, Proto, Function in TrangMeo
     5	 * @copyright Copyright (c) 2025 LazyPaws
     6	 * @license All rights reserved. Unauthorized copying of this file, in any form
     7	 * or medium, is strictly prohibited
     8	 */
     9	
    10	#pragma once
    11	
    12	#include <cstdint>
    13	#include <vector>
    14	#include <string>
    15	#include <memory>
    16	#include <meow/common.h>
    17	#include <meow/core/meow_object.h>
    18	#include <meow/common.h>
    19	#include <meow/value.h>
    20	#include <meow/memory/gc_visitor.h>
    21	#include "meow/bytecode/chunk.h"
    22	
    23	namespace meow {
    24	struct UpvalueDesc {
    25	    bool is_local_;
    26	    size_t index_;
    27	    UpvalueDesc(bool is_local = false, size_t index = 0) noexcept : is_local_(is_local), index_(index) {
    28	    }
    29	};
    30	
    31	class ObjUpvalue : public ObjBase<ObjectType::UPVALUE> {
    32	   private:
    33	    using visitor_t = GCVisitor;
    34	
    35	    enum class State { OPEN, CLOSED };
    36	    State state_ = State::OPEN;
    37	    size_t index_ = 0;
    38	    Value closed_ = null_t{};
    39	
    40	   public:
    41	    explicit ObjUpvalue(size_t index = 0) noexcept : index_(index) {
    42	    }
    43	    inline void close(param_t value) noexcept {
    44	        closed_ = value;
    45	        state_ = State::CLOSED;
    46	    }
    47	    inline bool is_closed() const noexcept {
    48	        return state_ == State::CLOSED;
    49	    }
    50	    inline return_t get_value() const noexcept {
    51	        return closed_;
    52	    }
    53	    inline size_t get_index() const noexcept {
    54	        return index_;
    55	    }
    56	
    57	    void trace(visitor_t& visitor) const noexcept override;
    58	};
    59	
    60	class ObjFunctionProto : public ObjBase<ObjectType::PROTO> {
    61	private:
    62	    using chunk_t = Chunk;
    63	    using string_t = string_t;
    64	    using visitor_t = GCVisitor;
    65	
    66	    size_t num_registers_;
    67	    size_t num_upvalues_;
    68	    string_t name_;
    69	    chunk_t chunk_;
    70	    module_t module_ = nullptr;
    71	    std::vector<UpvalueDesc> upvalue_descs_;
    72	
    73	public:
    74	    explicit ObjFunctionProto(size_t registers, size_t upvalues, string_t name, chunk_t&& chunk) noexcept : num_registers_(registers), num_upvalues_(upvalues), name_(name), chunk_(std::move(chunk)) {
    75	    }
    76	    explicit ObjFunctionProto(size_t registers, size_t upvalues, string_t name, chunk_t&& chunk, std::vector<UpvalueDesc>&& descs) noexcept
    77	        : num_registers_(registers), num_upvalues_(upvalues), name_(name), chunk_(std::move(chunk)), upvalue_descs_(std::move(descs)) {
    78	    }
    79	
    80	    inline void set_module(module_t mod) noexcept { module_ = mod; }
    81	    inline module_t get_module() const noexcept { return module_; }
    82	
    83	    /// @brief Unchecked upvalue desc access. For performance-critical code
    84	    inline const UpvalueDesc& get_desc(size_t index) const noexcept {
    85	        return upvalue_descs_[index];
    86	    }
    87	    /// @brief Checked upvalue desc access. For performence-critical code
    88	    inline const UpvalueDesc& at_desc(size_t index) const {
    89	        return upvalue_descs_.at(index);
    90	    }
    91	    inline size_t get_num_registers() const noexcept {
    92	        return num_registers_;
    93	    }
    94	    inline size_t get_num_upvalues() const noexcept {
    95	        return num_upvalues_;
    96	    }
    97	    inline string_t get_name() const noexcept {
    98	        return name_;
    99	    }
   100	    inline const chunk_t& get_chunk() const noexcept {
   101	        return chunk_;
   102	    }
   103	    inline size_t desc_size() const noexcept {
   104	        return upvalue_descs_.size();
   105	    }
   106	
   107	    void trace(visitor_t& visitor) const noexcept override;
   108	};
   109	
   110	class ObjClosure : public ObjBase<ObjectType::FUNCTION> {
   111	   private:
   112	    using proto_t = proto_t;
   113	    using upvalue_t = upvalue_t;
   114	    using visitor_t = GCVisitor;
   115	
   116	    proto_t proto_;
   117	    std::vector<upvalue_t> upvalues_;
   118	
   119	   public:
   120	    explicit ObjClosure(proto_t proto = nullptr) : proto_(proto), upvalues_(proto ? proto->get_num_upvalues() : 0) {}
   121	
   122	    inline proto_t get_proto() const noexcept {
   123	        return proto_;
   124	    }
   125	    /// @brief Unchecked upvalue access. For performance-critical code
   126	    inline upvalue_t get_upvalue(size_t index) const noexcept {
   127	        return upvalues_[index];
   128	    }
   129	    /// @brief Unchecked upvalue modification. For performance-critical code
   130	    inline void set_upvalue(size_t index, upvalue_t upvalue) noexcept {
   131	        upvalues_[index] = upvalue;
   132	    }
   133	    /// @brief Checked upvalue access. Throws if index is OOB
   134	    inline upvalue_t at_upvalue(size_t index) const {
   135	        return upvalues_.at(index);
   136	    }
   137	
   138	    void trace(visitor_t& visitor) const noexcept override;
   139	};
   140	}


// =============================================================================
//  FILE PATH: include/meow/core/hash_table.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <cstring>
     5	#include <bit> 
     6	#include <meow/common.h>
     7	#include <meow/core/meow_object.h>
     8	#include <meow/value.h>
     9	#include <meow/memory/gc_visitor.h>
    10	#include <meow/core/string.h>
    11	#include <meow_allocator.h> 
    12	
    13	namespace meow {
    14	
    15	struct Entry {
    16	    string_t first = nullptr;
    17	    Value second;
    18	};
    19	
    20	class ObjHashTable : public ObjBase<ObjectType::HASH_TABLE> {
    21	public:
    22	    using Allocator = meow::allocator<Entry>;
    23	private:
    24	    Entry* entries_ = nullptr;
    25	    uint32_t count_ = 0;
    26	    uint32_t capacity_ = 0;
    27	    uint32_t mask_ = 0;
    28	    
    29	    [[no_unique_address]] Allocator allocator_;
    30	
    31	    static constexpr double MAX_LOAD_FACTOR = 0.75;
    32	    static constexpr uint32_t MIN_CAPACITY = 8;
    33	public:
    34	    explicit ObjHashTable(Allocator allocator, uint32_t capacity = 0) : allocator_(allocator) {
    35	        if (capacity > 0) allocate(capacity);
    36	    }
    37	
    38	    ~ObjHashTable() noexcept override {
    39	        if (entries_) {
    40	            allocator_.deallocate(entries_, capacity_);
    41	        }
    42	    }
    43	
    44	    [[gnu::always_inline]] 
    45	    inline Entry* find_entry(Entry* entries, uint32_t mask, string_t key) const noexcept {
    46	        uint32_t index = key->hash() & mask;
    47	        for (;;) {
    48	            Entry* entry = &entries[index];
    49	            if (entry->first == key || entry->first == nullptr) [[likely]] {
    50	                return entry;
    51	            }
    52	            index = (index + 1) & mask;
    53	        }
    54	    }
    55	
    56	    [[gnu::always_inline]] 
    57	    inline bool set(string_t key, Value value) noexcept {
    58	        if (count_ + 1 > (capacity_ * MAX_LOAD_FACTOR)) [[unlikely]] {
    59	            grow();
    60	        }
    61	
    62	        Entry* entry = find_entry(entries_, mask_, key);
    63	        bool is_new = (entry->first == nullptr);
    64	        
    65	        if (is_new) [[likely]] {
    66	            count_++;
    67	            entry->first = key;
    68	        }
    69	        entry->second = value;
    70	        return is_new;
    71	    }
    72	
    73	    [[gnu::always_inline]] 
    74	    inline bool get(string_t key, Value* result) const noexcept {
    75	        if (count_ == 0) [[unlikely]] return false;
    76	        Entry* entry = find_entry(entries_, mask_, key);
    77	        if (entry->first == nullptr) return false;
    78	        *result = entry->second;
    79	        return true;
    80	    }
    81	
    82	    [[gnu::always_inline]]
    83	    inline Value get(string_t key) const noexcept {
    84	        if (count_ == 0) return Value(null_t{});
    85	        Entry* entry = find_entry(entries_, mask_, key);
    86	        if (entry->first == nullptr) return Value(null_t{});
    87	        return entry->second;
    88	    }
    89	
    90	    [[gnu::always_inline]] 
    91	    inline bool has(string_t key) const noexcept {
    92	        if (count_ == 0) return false;
    93	        return find_entry(entries_, mask_, key)->first != nullptr;
    94	    }
    95	
    96	    bool remove(string_t key) noexcept {
    97	        if (count_ == 0) return false;
    98	        Entry* entry = find_entry(entries_, mask_, key);
    99	        if (entry->first == nullptr) return false;
   100	
   101	        entry->first = nullptr;
   102	        entry->second = Value(null_t{});
   103	        count_--;
   104	
   105	        uint32_t index = (uint32_t)(entry - entries_);
   106	        uint32_t next_index = index;
   107	
   108	        for (;;) {
   109	            next_index = (next_index + 1) & mask_;
   110	            Entry* next = &entries_[next_index];
   111	            if (next->first == nullptr) break;
   112	
   113	            uint32_t ideal = next->first->hash() & mask_;
   114	            bool shift = false;
   115	            if (index < next_index) {
   116	                if (ideal <= index || ideal > next_index) shift = true;
   117	            } else {
   118	                if (ideal <= index && ideal > next_index) shift = true;
   119	            }
   120	
   121	            if (shift) {
   122	                entries_[index] = *next;
   123	                entries_[next_index].first = nullptr;
   124	                entries_[next_index].second = Value(null_t{});
   125	                index = next_index;
   126	            }
   127	        }
   128	        return true;
   129	    }
   130	
   131	    // --- Capacity ---
   132	    inline uint32_t size() const noexcept { return count_; }
   133	    inline bool empty() const noexcept { return count_ == 0; }
   134	    inline uint32_t capacity() const noexcept { return capacity_; }
   135	
   136	    class Iterator {
   137	        Entry* ptr_; Entry* end_;
   138	    public:
   139	        Iterator(Entry* ptr, Entry* end) : ptr_(ptr), end_(end) {
   140	            while (ptr_ < end_ && ptr_->first == nullptr) ptr_++;
   141	        }
   142	        Iterator& operator++() {
   143	            do { ptr_++; } while (ptr_ < end_ && ptr_->first == nullptr);
   144	            return *this;
   145	        }
   146	        bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }
   147	        
   148	        Entry& operator*() const { return *ptr_; }
   149	        Entry* operator->() const { return ptr_; }
   150	    };
   151	
   152	    inline Iterator begin() { return Iterator(entries_, entries_ + capacity_); }
   153	    inline Iterator end() { return Iterator(entries_ + capacity_, entries_ + capacity_); }
   154	
   155	    void trace(GCVisitor& visitor) const noexcept override {
   156	        for (uint32_t i = 0; i < capacity_; i++) {
   157	            if (entries_[i].first) {
   158	                visitor.visit_object(entries_[i].first);
   159	                visitor.visit_value(entries_[i].second);
   160	            }
   161	        }
   162	    }
   163	
   164	private:
   165	    void allocate(uint32_t capacity) {
   166	        capacity_ = (capacity < MIN_CAPACITY) ? MIN_CAPACITY : std::bit_ceil(capacity);
   167	        mask_ = capacity_ - 1;
   168	        
   169	        entries_ = allocator_.allocate(capacity_);
   170	        std::memset(static_cast<void*>(entries_), 0, sizeof(Entry) * capacity_);
   171	    }
   172	
   173	    void grow() {
   174	        uint32_t old_cap = capacity_;
   175	        Entry* old_entries = entries_;
   176	
   177	        allocate(old_cap == 0 ? MIN_CAPACITY : old_cap * 2);
   178	        count_ = 0;
   179	
   180	        if (old_entries) {
   181	            for (uint32_t i = 0; i < old_cap; i++) {
   182	                if (old_entries[i].first != nullptr) {
   183	                    Entry* dest = find_entry(entries_, mask_, old_entries[i].first);
   184	                    dest->first = old_entries[i].first;
   185	                    dest->second = old_entries[i].second;
   186	                    count_++;
   187	                }
   188	            }
   189	            allocator_.deallocate(old_entries, old_cap);
   190	        }
   191	    }
   192	};
   193	
   194	} // namespace meow


// =============================================================================
//  FILE PATH: include/meow/core/meow_object.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <meow/common.h>
     4	#include <cstdint>
     5	
     6	namespace meow {
     7	struct GCVisitor;
     8	
     9	enum class GCState : uint8_t {
    10	    UNMARKED = 0, MARKED = 1, OLD = 2
    11	};
    12	
    13	enum class ObjectType : uint8_t {
    14	    ARRAY = base_t::index_of<object_t>() + 1,
    15	    STRING, HASH_TABLE, INSTANCE, CLASS,
    16	    BOUND_METHOD, UPVALUE, PROTO, FUNCTION, MODULE, SHAPE
    17	};
    18	
    19	struct MeowObject {
    20	    const ObjectType type;
    21	    GCState gc_state = GCState::UNMARKED;
    22	
    23	    explicit MeowObject(ObjectType type_tag) noexcept : type(type_tag) {}
    24	    virtual ~MeowObject() = default;
    25	    
    26	    virtual void trace(GCVisitor& visitor) const noexcept = 0;
    27	    
    28	    inline ObjectType get_type() const noexcept { return type; }
    29	    inline bool is_marked() const noexcept { return gc_state != GCState::UNMARKED; }
    30	    inline void mark() noexcept { if (gc_state == GCState::UNMARKED) gc_state = GCState::MARKED; }
    31	    inline void unmark() noexcept { if (gc_state != GCState::OLD) gc_state = GCState::UNMARKED; }
    32	};
    33	
    34	template <ObjectType type_tag>
    35	struct ObjBase : public MeowObject {
    36	    ObjBase() noexcept : MeowObject(type_tag) {}
    37	};
    38	}


// =============================================================================
//  FILE PATH: include/meow/core/module.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <vector>
     5	#include <string>
     6	#include <memory>
     7	#include <meow/common.h>
     8	#include <meow/core/meow_object.h>
     9	#include <meow/common.h>
    10	#include <meow/value.h>
    11	#include <meow/memory/gc_visitor.h>
    12	#include <meow_flat_map.h>
    13	
    14	namespace meow {
    15	class ObjModule : public ObjBase<ObjectType::MODULE> {
    16	private:
    17	    using string_t = string_t;
    18	    using proto_t = proto_t;
    19	    using visitor_t = GCVisitor;
    20	
    21	    enum class State { INITIAL, EXECUTING, EXECUTED };
    22	
    23	    std::vector<Value> globals_store_;
    24	    
    25	    using GlobalNameMap = meow::flat_map<string_t, uint32_t>;
    26	    using ExportMap = meow::flat_map<string_t, value_t>;
    27	
    28	    GlobalNameMap global_names_;
    29	    ExportMap exports_;
    30	    
    31	    string_t file_name_;
    32	    string_t file_path_;
    33	    proto_t main_proto_;
    34	
    35	    State state;
    36	
    37	public:
    38	    explicit ObjModule(string_t file_name, string_t file_path, proto_t main_proto = nullptr) noexcept 
    39	        : file_name_(file_name), file_path_(file_path), main_proto_(main_proto), state(State::INITIAL) {}
    40	    
    41	    [[gnu::always_inline]]
    42	    inline return_t get_global_by_index(uint32_t index) const noexcept {
    43	        return globals_store_[index];
    44	    }
    45	
    46	    [[gnu::always_inline]]
    47	    inline void set_global_by_index(uint32_t index, param_t value) noexcept {
    48	        globals_store_[index] = value;
    49	    }
    50	
    51	    uint32_t intern_global(string_t name) {
    52	        uint32_t next_idx = static_cast<uint32_t>(globals_store_.size());
    53	        auto [ptr, inserted] = global_names_.try_emplace(name, next_idx);
    54	        
    55	        if (inserted) {
    56	            globals_store_.push_back(Value(null_t{}));
    57	        }
    58	        
    59	        return *ptr;
    60	    }
    61	
    62	    inline bool has_global(string_t name) {
    63	        return global_names_.contains(name);
    64	    }
    65	
    66	    inline return_t get_global(string_t name) noexcept {
    67	        if (auto* idx_ptr = global_names_.find(name)) {
    68	            return globals_store_[*idx_ptr];
    69	        }
    70	        return Value(null_t{});
    71	    }
    72	
    73	    inline void set_global(string_t name, param_t value) noexcept {
    74	        uint32_t idx = intern_global(name);
    75	        globals_store_[idx] = value;
    76	    }
    77	
    78	    inline void import_all_global(const module_t other) noexcept {
    79	        const auto& other_keys = other->global_names_.keys();
    80	        const auto& other_vals = other->global_names_.values();
    81	        
    82	        for (size_t i = 0; i < other_keys.size(); ++i) {
    83	            Value val = other->globals_store_[other_vals[i]];
    84	            set_global(other_keys[i], val);
    85	        }
    86	    }
    87	
    88	    // --- Exports ---
    89	    inline return_t get_export(string_t name) noexcept {
    90	        if (auto* val_ptr = exports_.find(name)) {
    91	            return *val_ptr;
    92	        }
    93	        return Value(null_t{});
    94	    }
    95	    
    96	    inline void set_export(string_t name, param_t value) noexcept {
    97	        exports_[name] = value; 
    98	    }
    99	    
   100	    inline bool has_export(string_t name) {
   101	        return exports_.contains(name);
   102	    }
   103	    
   104	    inline void import_all_export(const module_t other) noexcept {
   105	        const auto& other_keys = other->exports_.keys();
   106	        const auto& other_vals = other->exports_.values();
   107	        
   108	        for (size_t i = 0; i < other_keys.size(); ++i) {
   109	            exports_.try_emplace(other_keys[i], other_vals[i]);
   110	        }
   111	    }
   112	
   113	    inline string_t get_file_name() const noexcept { return file_name_; }
   114	    inline string_t get_file_path() const noexcept { return file_path_; }
   115	    inline proto_t get_main_proto() const noexcept { return main_proto_; }
   116	    inline void set_main_proto(proto_t proto) noexcept { main_proto_ = proto; }
   117	    inline bool is_has_main() const noexcept { return main_proto_ != nullptr; }
   118	
   119	    inline void set_execution() noexcept { state = State::EXECUTING; }
   120	    inline void set_executed() noexcept { state = State::EXECUTED; }
   121	    inline bool is_executing() const noexcept { return state == State::EXECUTING; }
   122	    inline bool is_executed() const noexcept { return state == State::EXECUTED; }
   123	
   124	    friend void obj_module_trace(const ObjModule* mod, visitor_t& visitor);
   125	    void trace(visitor_t& visitor) const noexcept override;
   126	    
   127	    const auto& get_global_names_raw() const { return global_names_; }
   128	    const auto& get_exports_raw() const { return exports_; }
   129	};
   130	}


// =============================================================================
//  FILE PATH: include/meow/core/objects.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <meow/core/array.h>
     4	#include <meow/core/function.h>
     5	#include <meow/core/hash_table.h>
     6	#include <meow/core/module.h>
     7	#include <meow/core/oop.h>
     8	#include <meow/core/string.h>
     9	#include <meow/core/shape.h>
    10	#include <meow/memory/gc_visitor.h>


// =============================================================================
//  FILE PATH: include/meow/core/oop.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <meow/common.h>
     4	#include <meow/core/meow_object.h>
     5	#include <meow/common.h>
     6	#include <meow/value.h>
     7	#include <meow/memory/gc_visitor.h>
     8	#include <meow/core/shape.h>
     9	#include <meow_flat_map.h>
    10	#include <cstdint>
    11	#include <vector>
    12	#include <string>
    13	#include <memory>
    14	
    15	namespace meow {
    16	class ObjClass : public ObjBase<ObjectType::CLASS> {
    17	private:
    18	    using method_map = meow::flat_map<string_t, value_t>;
    19	
    20	    string_t name_;
    21	    class_t superclass_;
    22	    method_map methods_;
    23	
    24	public:
    25	    explicit ObjClass(string_t name = nullptr) noexcept : name_(name) {}
    26	
    27	    inline string_t get_name() const noexcept {
    28	        return name_;
    29	    }
    30	    inline class_t get_super() const noexcept {
    31	        return superclass_;
    32	    }
    33	    inline void set_super(class_t super) noexcept {
    34	        superclass_ = super;
    35	    }
    36	
    37	    inline bool has_method(string_t name) const noexcept {
    38	        return methods_.contains(name);
    39	    }
    40	    
    41	    inline return_t get_method(string_t name) noexcept {
    42	        if (auto* val_ptr = methods_.find(name)) {
    43	            return *val_ptr;
    44	        }
    45	        return Value(null_t{});
    46	    }
    47	    
    48	    inline void set_method(string_t name, param_t value) noexcept {
    49	        methods_[name] = value;
    50	    }
    51	
    52	    void trace(GCVisitor& visitor) const noexcept override;
    53	};
    54	
    55	class ObjInstance : public ObjBase<ObjectType::INSTANCE> {
    56	private:
    57	    class_t klass_;
    58	    Shape* shape_;              
    59	    std::vector<Value> fields_; 
    60	public:
    61	    explicit ObjInstance(class_t k, Shape* empty_shape) noexcept 
    62	        : klass_(k), shape_(empty_shape) {
    63	    }
    64	
    65	    inline class_t get_class() const noexcept { return klass_; }
    66	    inline void set_class(class_t klass) noexcept { klass_ = klass; }
    67	
    68	    inline Shape* get_shape() const noexcept { return shape_; }
    69	    inline void set_shape(Shape* s) noexcept { shape_ = s; }
    70	
    71	    inline Value get_field_at(int offset) const noexcept {
    72	        return fields_[offset];
    73	    }
    74	    
    75	    inline void set_field_at(int offset, Value value) noexcept {
    76	        fields_[offset] = value;
    77	    }
    78	    
    79	    inline void add_field(param_t value) noexcept {
    80	        fields_.push_back(value);
    81	    }
    82	
    83	    inline bool has_field(string_t name) const noexcept {
    84	        return shape_->get_offset(name) != -1;
    85	    }
    86	    
    87	    inline Value get_field(string_t name) const noexcept {
    88	        int offset = shape_->get_offset(name);
    89	        if (offset != -1) return fields_[offset];
    90	        return Value(null_t{});
    91	    }
    92	
    93	    inline void trace(GCVisitor& visitor) const noexcept override {
    94	        visitor.visit_object(klass_);
    95	        visitor.visit_object(shape_);
    96	        for (const auto& val : fields_) {
    97	            visitor.visit_value(val);
    98	        }
    99	    }
   100	};
   101	
   102	class ObjBoundMethod : public ObjBase<ObjectType::BOUND_METHOD> {
   103	private:
   104	    Value receiver_; 
   105	    Value method_;   
   106	public:
   107	    explicit ObjBoundMethod(Value receiver, Value method) noexcept 
   108	        : receiver_(receiver), method_(method) {}
   109	
   110	    inline Value get_receiver() const noexcept { return receiver_; }
   111	    inline Value get_method() const noexcept { return method_; }
   112	
   113	    inline void trace(GCVisitor& visitor) const noexcept override {
   114	        visitor.visit_value(receiver_);
   115	        visitor.visit_value(method_);
   116	    }
   117	};
   118	}


// =============================================================================
//  FILE PATH: include/meow/core/shape.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <vector>
     5	#include <string>
     6	#include <memory>
     7	#include <meow/common.h>
     8	#include <meow/core/meow_object.h>
     9	#include <meow/memory/gc_visitor.h>
    10	#include <meow/core/string.h>
    11	#include <meow_flat_map.h>
    12	
    13	namespace meow {
    14	
    15	class MemoryManager; 
    16	
    17	class Shape : public ObjBase<ObjectType::SHAPE> {
    18	public:
    19	    using TransitionMap = meow::flat_map<string_t, Shape*>;
    20	    using PropertyMap = meow::flat_map<string_t, uint32_t>;
    21	private:
    22	    PropertyMap property_offsets_;
    23	    TransitionMap transitions_;     
    24	    uint32_t num_fields_ = 0;       
    25	public:
    26	    explicit Shape() = default;
    27	
    28	    int get_offset(string_t name) const;
    29	
    30	    Shape* get_transition(string_t name) const;
    31	
    32	    Shape* add_transition(string_t name, MemoryManager* heap);
    33	
    34	    inline uint32_t count() const { return num_fields_; }
    35	    
    36	    void copy_from(const Shape* other) {
    37	        property_offsets_ = other->property_offsets_;
    38	        num_fields_ = other->num_fields_;
    39	    }
    40	    
    41	    void add_property(string_t name) {
    42	        property_offsets_[name] = num_fields_++;
    43	    }
    44	
    45	    void trace(GCVisitor& visitor) const noexcept override;
    46	};
    47	
    48	}


// =============================================================================
//  FILE PATH: include/meow/core/string.h
// =============================================================================

     1	#pragma once
     2	#include <cstdint>
     3	#include <cstring>
     4	#include <string> 
     5	#include <meow/core/meow_object.h>
     6	
     7	namespace meow {
     8	
     9	class ObjString : public ObjBase<ObjectType::STRING> {
    10	private:
    11	    size_t length_;
    12	    size_t hash_;
    13	    char chars_[1]; 
    14	
    15	    friend class MemoryManager;
    16	    friend class heap; 
    17	    
    18	    ObjString(const char* chars, size_t length, size_t hash) 
    19	        : length_(length), hash_(hash) {
    20	        std::memcpy(chars_, chars, length);
    21	        chars_[length] = '\0'; 
    22	    }
    23	
    24	public:
    25	    ObjString() = delete; 
    26	    ObjString(const ObjString&) = delete;
    27	    
    28	    // --- Accessors ---
    29	    inline const char* c_str() const noexcept { return chars_; }
    30	    inline size_t size() const noexcept { return length_; }
    31	    inline bool empty() const noexcept { return length_ == 0; }
    32	    inline size_t hash() const noexcept { return hash_; }
    33	
    34	    inline char get(size_t index) const noexcept { return chars_[index]; }
    35	
    36	    inline void trace(GCVisitor&) const noexcept override {}
    37	};
    38	
    39	struct ObjStringHasher {
    40	    inline size_t operator()(string_t s) const noexcept {
    41	        return s->hash();
    42	    }
    43	};
    44	}


// =============================================================================
//  FILE PATH: include/meow/diagnostics/diagnostic.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <vector>
     5	#include <string>
     6	#include <memory>
     7	
     8	namespace meow {
     9	
    10	enum class Severity { Note, Warning, Error, Total };
    11	
    12	struct Span {
    13	    std::string file;
    14	    size_t start_line = 0, start_col = 0;
    15	    size_t end_line = 0, end_col = 0;
    16	};
    17	
    18	struct StackFrame {
    19	    std::string function;
    20	    std::string file;
    21	    size_t line = 0, col = 0;
    22	};
    23	
    24	struct Diagnostic;
    25	
    26	struct LocaleSource {
    27	    virtual ~LocaleSource() noexcept = default;
    28	    virtual std::optional<std::string> get_template(const std::string& message_id) = 0;
    29	};
    30	
    31	struct Diagnostic {
    32	    std::string code;
    33	
    34	    Severity severity = Severity::Error;
    35	    std::unordered_map<std::string, std::string> args;
    36	    std::vector<Span> spans;
    37	    std::vector<Diagnostic> notes;
    38	    std::vector<StackFrame> callstack;
    39	};
    40	
    41	struct RenderOptions {
    42	    bool enable_color = true;
    43	    size_t context_lines = 2;
    44	    size_t max_stack_frames = 10;
    45	};
    46	
    47	std::string render_to_human(const Diagnostic& diag, LocaleSource& locale, const RenderOptions& options);
    48	
    49	}


// =============================================================================
//  FILE PATH: include/meow/diagnostics/locale.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "diagnostics/diagnostic.h"
     4	
     5	namespace meow {
     6	
     7	struct SimpleLocaleSource final : public LocaleSource {
     8	    ~SimpleLocaleSource() noexcept override = default;
     9	    std::unordered_map<std::string, std::string> map;
    10	
    11	    bool load_file(const std::string& path) {
    12	        std::ifstream in(path);
    13	        if (!in) return false;
    14	
    15	        std::string line;
    16	        while (std::getline(in, line)) {
    17	            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
    18	
    19	            size_t i = 0;
    20	            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    21	
    22	            if (i >= line.size() || line[i] == '#') continue;
    23	            size_t pos = line.find('=', i);
    24	            if (pos == std::string::npos) continue;
    25	
    26	            std::string key = line.substr(i, pos - i);
    27	            std::string val = line.substr(pos + 1);
    28	
    29	            auto trim = [](std::string& s) {
    30	                size_t a = 0;
    31	                while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    32	                size_t b = s.size();
    33	                while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    34	                s = s.substr(a, b - a);
    35	            };
    36	            trim(key);
    37	            trim(val);
    38	
    39	            if (!key.empty()) map[key] = val;
    40	        }
    41	
    42	        return true;
    43	    }
    44	
    45	    std::optional<std::string> get_template(const std::string& message_id) override {
    46	        if (auto it = map.find(message_id); it != map.end()) return it->second;
    47	        return std::nullopt;
    48	    }
    49	};
    50	
    51	}



// =============================================================================
//  FILE PATH: include/meow/machine.h
// =============================================================================

     1	#include <meow/common.h>
     2	#pragma once
     3	
     4	#include <vector>
     5	#include <string>
     6	#include <memory>
     7	#include <filesystem>
     8	
     9	namespace meow {
    10	struct ExecutionContext;
    11	class MemoryManager;
    12	class ModuleManager;
    13	
    14	struct VMArgs {
    15	    std::vector<std::string> command_line_arguments_;
    16	    std::string entry_point_directory_;
    17	    std::string entry_path_;
    18	};
    19	
    20	class Machine {
    21	public:
    22	    // --- Constructors ---
    23	    explicit Machine(const std::string& entry_point_directory, const std::string& entry_path, int argc, char* argv[]);
    24	    Machine(const Machine&) = delete;
    25	    Machine(Machine&&) = delete;
    26	    Machine& operator=(const Machine&) = delete;
    27	    Machine& operator=(Machine&&) = delete;
    28	    ~Machine() noexcept;
    29	
    30	    // --- Public API ---
    31	    void interpret() noexcept;
    32	    
    33	    void execute(function_t func);
    34	    Value call_callable(Value callable, const std::vector<Value>& args) noexcept;
    35	
    36	    inline MemoryManager* get_heap() const noexcept { return heap_.get(); }
    37	    inline const VMArgs& get_args() const noexcept { return args_; }
    38	    
    39	    inline void error(std::string message) noexcept {
    40	        has_error_ = true;
    41	        error_message_ = std::move(message);
    42	    }
    43	
    44	
    45	    inline bool has_error() const noexcept { return has_error_; }
    46	    
    47	    inline std::string_view get_error_message() const noexcept { return error_message_; }
    48	    
    49	    inline void clear_error() noexcept { has_error_ = false; error_message_.clear(); }
    50	    
    51	private:
    52	    // --- Subsystems ---
    53	    std::unique_ptr<ExecutionContext> context_;
    54	    std::unique_ptr<MemoryManager> heap_;
    55	    std::unique_ptr<ModuleManager> mod_manager_;
    56	
    57	    // --- Runtime arguments ---
    58	    VMArgs args_;
    59	    bool has_error_ = false;
    60	    std::string error_message_;
    61	
    62	    // --- Execution internals ---
    63	    bool prepare() noexcept;
    64	    void run() noexcept;
    65	    void load_builtins();
    66	};
    67	}


// =============================================================================
//  FILE PATH: include/meow/memory/garbage_collector.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstddef>
     4	#include <meow/value.h>
     5	#include "meow_heap.h"
     6	
     7	namespace meow {
     8	struct MeowObject;
     9	
    10	namespace gc_flags {
    11	    static constexpr uint32_t GEN_YOUNG = 0;       // Bit 0 = 0
    12	    static constexpr uint32_t GEN_OLD   = 1 << 0;  // Bit 0 = 1
    13	    static constexpr uint32_t MARKED    = 1 << 1;  // Bit 1 = 1
    14	    static constexpr uint32_t PERMANENT = 1 << 2;  // Bit 2 = 1
    15	}
    16	
    17	class GarbageCollector {
    18	protected:
    19	    meow::heap* heap_ = nullptr;
    20	public:
    21	    virtual ~GarbageCollector() noexcept = default;
    22	    void set_heap(meow::heap* h) noexcept { heap_ = h; }
    23	
    24	    virtual void register_object(const MeowObject* object) = 0;
    25	    virtual void register_permanent(const MeowObject* object) = 0;
    26	    virtual size_t collect() noexcept = 0;
    27	    virtual void write_barrier(MeowObject*, Value) noexcept {}
    28	};
    29	}


// =============================================================================
//  FILE PATH: include/meow/memory/gc_disable_guard.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <meow/memory/memory_manager.h>
     4	
     5	namespace meow {
     6	class GCDisableGuard {
     7	   private:
     8	    MemoryManager* heap_;
     9	
    10	   public:
    11	    explicit GCDisableGuard(MemoryManager* heap) noexcept : heap_(heap) {
    12	        if (heap_) heap_->disable_gc();
    13	    }
    14	    GCDisableGuard(const GCDisableGuard&) = delete;
    15	    GCDisableGuard(GCDisableGuard&&) = default;
    16	    GCDisableGuard& operator=(const GCDisableGuard&) = delete;
    17	    GCDisableGuard& operator=(GCDisableGuard&&) = default;
    18	    ~GCDisableGuard() noexcept {
    19	        if (heap_) heap_->enable_gc();
    20	    }
    21	};
    22	}


// =============================================================================
//  FILE PATH: include/meow/memory/gc_visitor.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <meow/common.h>
     4	
     5	namespace meow {
     6	class Value;
     7	struct MeowObject;
     8	}
     9	
    10	namespace meow {
    11	struct GCVisitor {
    12	    virtual ~GCVisitor() = default;
    13	    virtual void visit_value(param_t value) noexcept = 0;
    14	    virtual void visit_object(const MeowObject* object) noexcept = 0;
    15	};
    16	}


// =============================================================================
//  FILE PATH: include/meow/memory/memory_manager.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <cstdint>
     4	#include <vector>
     5	#include <string>
     6	#include <unordered_set>
     7	#include <memory>
     8	#include <meow/core/objects.h>
     9	#include <meow/common.h>
    10	#include <meow/memory/garbage_collector.h>
    11	#include <meow/core/shape.h>
    12	#include <meow/core/string.h>
    13	#include <meow/core/function.h>
    14	
    15	#include "meow_heap.h"
    16	#include "meow_allocator.h"
    17	
    18	namespace meow {
    19	class MemoryManager {
    20	private:
    21	    static thread_local MemoryManager* current_;
    22	public:
    23	    explicit MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept;
    24	    ~MemoryManager() noexcept;
    25	
    26	    // --- Factory Methods ---
    27	    array_t new_array(const std::vector<Value>& elements = {});
    28	    string_t new_string(std::string_view str_view);
    29	    string_t new_string(const char* chars, size_t length);
    30	    hash_table_t new_hash(uint32_t capacity = 0);
    31	    upvalue_t new_upvalue(size_t index);
    32	    proto_t new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk);
    33	    proto_t new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<UpvalueDesc>&& descs);
    34	    function_t new_function(proto_t proto);
    35	    module_t new_module(string_t file_name, string_t file_path, proto_t main_proto = nullptr);
    36	    class_t new_class(string_t name = nullptr);
    37	    instance_t new_instance(class_t klass, Shape* shape);
    38	    bound_method_t new_bound_method(Value instance, Value function);
    39	    Shape* new_shape();
    40	
    41	    Shape* get_empty_shape() noexcept;
    42	
    43	    // --- GC Control ---
    44	    void enable_gc() noexcept { 
    45	        if (gc_pause_count_ > 0) gc_pause_count_--; 
    46	    }
    47	    
    48	    void disable_gc() noexcept { 
    49	        gc_pause_count_++; 
    50	    }
    51	    
    52	    void collect() noexcept { object_allocated_ = gc_->collect(); }
    53	
    54	    [[gnu::always_inline]]
    55	    void write_barrier(MeowObject* owner, Value value) noexcept {
    56	        if (gc_pause_count_ == 0) {
    57	            gc_->write_barrier(owner, value);
    58	        }
    59	    }
    60	
    61	    static void set_current(MemoryManager* instance) noexcept { current_ = instance; }
    62	    static MemoryManager* get_current() noexcept { return current_; }
    63	private:
    64	    struct StringPoolHash {
    65	        using is_transparent = void;
    66	        size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
    67	        size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
    68	        size_t operator()(string_t s) const { return s->hash(); }
    69	    };
    70	
    71	    struct StringPoolEq {
    72	        using is_transparent = void;
    73	        bool operator()(string_t a, string_t b) const { return a == b; }
    74	        bool operator()(string_t a, std::string_view b) const { return std::string_view(a->c_str(), a->size()) == b; }
    75	        bool operator()(std::string_view a, string_t b) const { return a == std::string_view(b->c_str(), b->size()); }
    76	    };
    77	
    78	    meow::arena arena_;
    79	    meow::heap heap_; 
    80	
    81	    std::unique_ptr<GarbageCollector> gc_;
    82	    std::unordered_set<string_t, StringPoolHash, StringPoolEq> string_pool_;
    83	    Shape* empty_shape_ = nullptr;
    84	
    85	    size_t gc_threshold_;
    86	    size_t object_allocated_;
    87	    size_t gc_pause_count_ = 0;
    88	
    89	    template <typename T, typename... Args>
    90	    T* new_object(Args&&... args) {
    91	        if (object_allocated_ >= gc_threshold_ && gc_pause_count_ == 0) {
    92	            collect();
    93	            gc_threshold_ = std::max(gc_threshold_ * 2, object_allocated_ * 2);
    94	        }
    95	        
    96	        T* obj = heap_.create<T>(std::forward<Args>(args)...);
    97	        
    98	        gc_->register_object(static_cast<MeowObject*>(obj));
    99	        ++object_allocated_;
   100	        return obj;
   101	    }
   102	};
   103	}


// =============================================================================
//  FILE PATH: include/meow/value.h
// =============================================================================

     1	#pragma once
     2	
     3	#include <utility>
     4	#include <cstdint>
     5	#include <cstddef>
     6	#include <type_traits>
     7	#include <meow/common.h>
     8	#include "meow_variant.h"
     9	#include <meow/core/meow_object.h>
    10	
    11	namespace meow {
    12	
    13	class Value {
    14	private:
    15	    base_t data_;
    16	
    17	    // --- Private Helpers ---
    18	
    19	    template <typename Self>
    20	    inline auto get_object_ptr(this Self&& self) noexcept -> object_t {
    21	        if (auto* val_ptr = self.data_.template get_if<object_t>()) {
    22	            return *val_ptr;
    23	        }
    24	        return nullptr;
    25	    }
    26	
    27	    inline bool check_obj_type(ObjectType type) const noexcept {
    28	        auto obj = get_object_ptr();
    29	        return (obj && obj->get_type() == type);
    30	    }
    31	
    32	    template <typename TargetType, ObjectType Type, typename Self>
    33	    inline auto get_obj_if(this Self&& self) noexcept {
    34	        if (auto obj = self.get_object_ptr()) {
    35	            if (obj->get_type() == Type) {
    36	                return reinterpret_cast<TargetType>(obj);
    37	            }
    38	        }
    39	        return static_cast<TargetType>(nullptr);
    40	    }
    41	
    42	public:
    43	    using layout_traits = base_t::layout_traits;
    44	
    45	    // --- Constructors & Assignments ---
    46	    
    47	    inline Value() noexcept : data_(null_t{}) {}
    48	    
    49	    inline Value(const Value&) noexcept = default;
    50	    inline Value(Value&&) noexcept = default;
    51	    inline Value& operator=(const Value&) noexcept = default;
    52	    inline Value& operator=(Value&&) noexcept = default;
    53	    inline ~Value() noexcept = default;
    54	
    55	    template <typename T>
    56	    requires (std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    57	    inline Value(T&& v) noexcept : data_(static_cast<MeowObject*>(v)) {}
    58	
    59	    template <typename T>
    60	    requires (std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    61	    inline Value& operator=(T&& v) noexcept {
    62	        data_ = static_cast<MeowObject*>(v);
    63	        return *this;
    64	    }
    65	
    66	    template <typename T>
    67	    requires (!std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    68	    inline Value(T&& v) noexcept : data_(std::forward<T>(v)) {}
    69	
    70	    template <typename T>
    71	    requires (!std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    72	    inline Value& operator=(T&& v) noexcept {
    73	        data_ = std::forward<T>(v);
    74	        return *this;
    75	    }
    76	
    77	    // --- Operators ---
    78	
    79	    inline bool operator==(const Value& other) const noexcept { return data_ == other.data_; }
    80	    inline bool operator!=(const Value& other) const noexcept { return data_ != other.data_; }
    81	
    82	    // --- Core Access ---
    83	
    84	    inline constexpr size_t index() const noexcept { return data_.index(); }
    85	    inline uint64_t raw_tag() const noexcept { return data_.raw_tag(); }
    86	    inline void set_raw(uint64_t bits) noexcept { data_.set_raw(bits); }
    87	    inline uint64_t raw() const noexcept { return data_.raw(); }
    88	
    89	    template <typename T>
    90	    inline bool holds_both(const Value& other) const noexcept {
    91	        return data_.template holds_both<T>(other.data_);
    92	    }
    93	
    94	    static inline Value from_raw(uint64_t bits) noexcept {
    95	        Value v; v.set_raw(bits); return v;
    96	    }
    97	
    98	    // === Type Checkers ===
    99	
   100	    inline bool is_null() const noexcept { return data_.holds<null_t>(); }
   101	    inline bool is_bool() const noexcept { return data_.holds<bool_t>(); }
   102	    inline bool is_int() const noexcept { return data_.holds<int_t>(); }
   103	    inline bool is_float() const noexcept { return data_.holds<float_t>(); }
   104	    inline bool is_native() const noexcept { return data_.holds<native_t>(); }
   105	    inline bool is_object() const noexcept { return get_object_ptr() != nullptr; }
   106	
   107	    inline bool is_array() const noexcept        { return check_obj_type(ObjectType::ARRAY); }
   108	    inline bool is_string() const noexcept       { return check_obj_type(ObjectType::STRING); }
   109	    inline bool is_hash_table() const noexcept   { return check_obj_type(ObjectType::HASH_TABLE); }
   110	    inline bool is_upvalue() const noexcept      { return check_obj_type(ObjectType::UPVALUE); }
   111	    inline bool is_proto() const noexcept        { return check_obj_type(ObjectType::PROTO); }
   112	    inline bool is_function() const noexcept     { return check_obj_type(ObjectType::FUNCTION); }
   113	    inline bool is_class() const noexcept        { return check_obj_type(ObjectType::CLASS); }
   114	    inline bool is_instance() const noexcept     { return check_obj_type(ObjectType::INSTANCE); }
   115	    inline bool is_bound_method() const noexcept { return check_obj_type(ObjectType::BOUND_METHOD); }
   116	    inline bool is_module() const noexcept       { return check_obj_type(ObjectType::MODULE); }
   117	
   118	    // === Unsafe Accessors ===
   119	
   120	    inline bool as_bool() const noexcept       { return data_.get<bool_t>(); }
   121	    inline int64_t as_int() const noexcept     { return data_.get<int_t>(); }
   122	    inline double as_float() const noexcept    { return data_.get<float_t>(); }
   123	    inline native_t as_native() const noexcept { return data_.get<native_t>(); }
   124	    
   125	    inline MeowObject* as_object() const noexcept { return data_.get<object_t>(); }
   126	
   127	    template <typename T>
   128	    inline T as_obj_unsafe() const noexcept { return reinterpret_cast<T>(as_object()); }
   129	
   130	    inline array_t as_array() const noexcept               { return as_obj_unsafe<array_t>(); }
   131	    inline string_t as_string() const noexcept             { return as_obj_unsafe<string_t>(); }
   132	    inline hash_table_t as_hash_table() const noexcept     { return as_obj_unsafe<hash_table_t>(); }
   133	    inline upvalue_t as_upvalue() const noexcept           { return as_obj_unsafe<upvalue_t>(); }
   134	    inline proto_t as_proto() const noexcept               { return as_obj_unsafe<proto_t>(); }
   135	    inline function_t as_function() const noexcept         { return as_obj_unsafe<function_t>(); }
   136	    inline class_t as_class() const noexcept               { return as_obj_unsafe<class_t>(); }
   137	    inline instance_t as_instance() const noexcept         { return as_obj_unsafe<instance_t>(); }
   138	    inline bound_method_t as_bound_method() const noexcept { return as_obj_unsafe<bound_method_t>(); }
   139	    inline module_t as_module() const noexcept             { return as_obj_unsafe<module_t>(); }
   140	
   141	    // === Safe Getters (Deducing 'this') ===
   142	
   143	    template <typename Self> auto as_if_bool(this Self&& self) noexcept   { return self.data_.template get_if<bool_t>(); }
   144	    template <typename Self> auto as_if_int(this Self&& self) noexcept    { return self.data_.template get_if<int_t>(); }
   145	    template <typename Self> auto as_if_float(this Self&& self) noexcept  { return self.data_.template get_if<float_t>(); }
   146	    template <typename Self> auto as_if_native(this Self&& self) noexcept { return self.data_.template get_if<native_t>(); }
   147	
   148	    // Objects
   149	    template <typename Self> auto as_if_array(this Self&& self) noexcept        { return self.template get_obj_if<array_t, ObjectType::ARRAY>(); }
   150	    template <typename Self> auto as_if_string(this Self&& self) noexcept       { return self.template get_obj_if<string_t, ObjectType::STRING>(); }
   151	    template <typename Self> auto as_if_hash_table(this Self&& self) noexcept   { return self.template get_obj_if<hash_table_t, ObjectType::HASH_TABLE>(); }
   152	    template <typename Self> auto as_if_upvalue(this Self&& self) noexcept      { return self.template get_obj_if<upvalue_t, ObjectType::UPVALUE>(); }
   153	    template <typename Self> auto as_if_proto(this Self&& self) noexcept        { return self.template get_obj_if<proto_t, ObjectType::PROTO>(); }
   154	    template <typename Self> auto as_if_function(this Self&& self) noexcept     { return self.template get_obj_if<function_t, ObjectType::FUNCTION>(); }
   155	    template <typename Self> auto as_if_class(this Self&& self) noexcept        { return self.template get_obj_if<class_t, ObjectType::CLASS>(); }
   156	    template <typename Self> auto as_if_instance(this Self&& self) noexcept     { return self.template get_obj_if<instance_t, ObjectType::INSTANCE>(); }
   157	    template <typename Self> auto as_if_bound_method(this Self&& self) noexcept { return self.template get_obj_if<bound_method_t, ObjectType::BOUND_METHOD>(); }
   158	    template <typename Self> auto as_if_module(this Self&& self) noexcept       { return self.template get_obj_if<module_t, ObjectType::MODULE>(); }
   159	
   160	    // === Visitor ===
   161	    template <typename... Fs>
   162	    decltype(auto) visit(Fs&&... fs) const { return data_.visit(std::forward<Fs>(fs)...); }
   163	    
   164	    template <typename... Fs>
   165	    decltype(auto) visit(Fs&&... fs) { return data_.visit(std::forward<Fs>(fs)...); }
   166	};
   167	
   168	}


