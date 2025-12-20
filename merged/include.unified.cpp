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
    39	using object_t = MeowObject*;
    40	
    41	using array_t = ObjArray*;
    42	using string_t = ObjString*;
    43	using hash_table_t = ObjHashTable*;
    44	using instance_t = ObjInstance*;
    45	using class_t = ObjClass*;
    46	using bound_method_t = ObjBoundMethod*;
    47	using upvalue_t = ObjUpvalue*;
    48	using proto_t = ObjFunctionProto*;
    49	using function_t = ObjClosure*;
    50	using module_t = ObjModule*;
    51	using shape_t = Shape*;
    52	
    53	using base_t = meow::variant<null_t, bool_t, int_t, float_t, native_t, object_t>;
    54	
    55	enum class ValueType : uint8_t {
    56	    Null,
    57	    Bool,
    58	    Int,
    59	    Float,
    60	    NativeFn,
    61	    Object,
    62	
    63	    Array,        // 1  — ARRAY
    64	    String,       // 2  — STRING
    65	    HashTable,    // 3  — HASH_TABLE
    66	    Instance,     // 4  — INSTANCE
    67	    Class,        // 5  — CLASS
    68	    BoundMethod,  // 6  — BOUND_METHOD
    69	    Upvalue,      // 7  — UPVALUE
    70	    Proto,        // 8  — PROTO
    71	    Function,     // 9  — FUNCTION
    72	    Module,       // 10 — MODULE
    73	    Shape,        // 11 - SHAPE
    74	
    75	    TotalValueTypes
    76	};
    77	}



// =============================================================================
//  FILE PATH: include/meow/core/array.h
// =============================================================================

     1	/**
     2	 * @file array.h
     3	 * @author LazyPaws
     4	 * @brief Core definition of Array in TrangMeo
     5	 */
     6	
     7	#pragma once
     8	
     9	#include <cstdint>
    10	#include <vector>
    11	#include <meow/core/meow_object.h>
    12	#include <meow/value.h>
    13	#include <meow/memory/gc_visitor.h>
    14	#include <meow/memory/memory_manager.h>
    15	
    16	namespace meow {
    17	class ObjArray : public ObjBase<ObjectType::ARRAY> {
    18	public:
    19	    using container_t = std::vector<value_t>;
    20	    
    21	private:
    22	    using visitor_t = GCVisitor;
    23	    container_t elements_;
    24	
    25	public:
    26	    explicit ObjArray() = default;
    27	
    28	    ObjArray(const std::vector<value_t>& elements) 
    29	        : elements_(elements) {}
    30	
    31	    ObjArray(container_t&& elements) noexcept 
    32	        : elements_(std::move(elements)) {}
    33	
    34	    // --- Rule of 5 ---
    35	    ObjArray(const ObjArray&) = delete;
    36	    ObjArray(ObjArray&&) = default;
    37	    ObjArray& operator=(const ObjArray&) = delete;
    38	    ObjArray& operator=(ObjArray&&) = delete;
    39	    ~ObjArray() override = default;
    40	
    41	    // --- Size Override ---
    42	    size_t obj_size() const noexcept override { return sizeof(ObjArray); }
    43	
    44	    // --- Iterator types ---
    45	    using iterator = container_t::iterator;
    46	    using const_iterator = container_t::const_iterator;
    47	    using reverse_iterator = container_t::reverse_iterator;
    48	    using const_reverse_iterator = container_t::const_reverse_iterator;
    49	
    50	    // --- Accessors & Modifiers ---
    51	    
    52	    template <typename Self>
    53	    inline decltype(auto) get(this Self&& self, size_t index) noexcept {
    54	        return std::forward<Self>(self).elements_[index]; 
    55	    }
    56	
    57	    template <typename Self>
    58	    inline decltype(auto) at(this Self&& self, size_t index) {
    59	        return std::forward<Self>(self).elements_.at(index);
    60	    }
    61	
    62	    template <typename Self>
    63	    inline decltype(auto) operator[](this Self&& self, size_t index) noexcept {
    64	        return std::forward<Self>(self).elements_[index];
    65	    }
    66	
    67	    template <typename Self>
    68	    inline decltype(auto) front(this Self&& self) noexcept {
    69	        return std::forward<Self>(self).elements_.front();
    70	    }
    71	
    72	    template <typename Self>
    73	    inline decltype(auto) back(this Self&& self) noexcept {
    74	        return std::forward<Self>(self).elements_.back();
    75	    }
    76	
    77	    template <typename T>
    78	    inline void set(size_t index, T&& value) noexcept {
    79	        elements_[index] = std::forward<T>(value);
    80	    }
    81	
    82	    inline size_t size() const noexcept { return elements_.size(); }
    83	    inline bool empty() const noexcept { return elements_.empty(); }
    84	    inline size_t capacity() const noexcept { return elements_.capacity(); }
    85	
    86	    template <typename T>
    87	    inline void push(T&& value) {
    88	        elements_.emplace_back(std::forward<T>(value));
    89	    }
    90	    inline void pop() noexcept { elements_.pop_back(); }
    91	    
    92	    template <typename... Args>
    93	    inline void emplace(Args&&... args) { elements_.emplace_back(std::forward<Args>(args)...); }
    94	    
    95	    inline void resize(size_t size) { elements_.resize(size); }
    96	    inline void reserve(size_t capacity) { elements_.reserve(capacity); }
    97	    inline void shrink() { elements_.shrink_to_fit(); }
    98	    inline void clear() { elements_.clear(); }
    99	
   100	    template <typename Self>
   101	    inline auto begin(this Self&& self) noexcept { return std::forward<Self>(self).elements_.begin(); }
   102	    
   103	    template <typename Self>
   104	    inline auto end(this Self&& self) noexcept { return std::forward<Self>(self).elements_.end(); }
   105	
   106	    template <typename Self>
   107	    inline auto rbegin(this Self&& self) noexcept { return std::forward<Self>(self).elements_.rbegin(); }
   108	
   109	    template <typename Self>
   110	    inline auto rend(this Self&& self) noexcept { return std::forward<Self>(self).elements_.rend(); }
   111	
   112	    void trace(visitor_t& visitor) const noexcept override;
   113	};
   114	}


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
    57	    size_t obj_size() const noexcept override { return sizeof(ObjUpvalue); }
    58	
    59	    void trace(visitor_t& visitor) const noexcept override;
    60	};
    61	
    62	class ObjFunctionProto : public ObjBase<ObjectType::PROTO> {
    63	private:
    64	    using chunk_t = Chunk;
    65	    using string_t = string_t;
    66	    using visitor_t = GCVisitor;
    67	
    68	    size_t num_registers_;
    69	    size_t num_upvalues_;
    70	    string_t name_;
    71	    chunk_t chunk_;
    72	    module_t module_ = nullptr;
    73	    std::vector<UpvalueDesc> upvalue_descs_;
    74	
    75	public:
    76	    explicit ObjFunctionProto(size_t registers, size_t upvalues, string_t name, chunk_t&& chunk) noexcept : num_registers_(registers), num_upvalues_(upvalues), name_(name), chunk_(std::move(chunk)) {
    77	    }
    78	    explicit ObjFunctionProto(size_t registers, size_t upvalues, string_t name, chunk_t&& chunk, std::vector<UpvalueDesc>&& descs) noexcept
    79	        : num_registers_(registers), num_upvalues_(upvalues), name_(name), chunk_(std::move(chunk)), upvalue_descs_(std::move(descs)) {
    80	    }
    81	
    82	    inline void set_module(module_t mod) noexcept { module_ = mod; }
    83	    inline module_t get_module() const noexcept { return module_; }
    84	
    85	    /// @brief Unchecked upvalue desc access. For performance-critical code
    86	    inline const UpvalueDesc& get_desc(size_t index) const noexcept {
    87	        return upvalue_descs_[index];
    88	    }
    89	    /// @brief Checked upvalue desc access. For performence-critical code
    90	    inline const UpvalueDesc& at_desc(size_t index) const {
    91	        return upvalue_descs_.at(index);
    92	    }
    93	    inline size_t get_num_registers() const noexcept {
    94	        return num_registers_;
    95	    }
    96	    inline size_t get_num_upvalues() const noexcept {
    97	        return num_upvalues_;
    98	    }
    99	    inline string_t get_name() const noexcept {
   100	        return name_;
   101	    }
   102	    inline const chunk_t& get_chunk() const noexcept {
   103	        return chunk_;
   104	    }
   105	    inline size_t desc_size() const noexcept {
   106	        return upvalue_descs_.size();
   107	    }
   108	
   109	    size_t obj_size() const noexcept override { return sizeof(ObjFunctionProto); }
   110	
   111	    void trace(visitor_t& visitor) const noexcept override;
   112	};
   113	
   114	class ObjClosure : public ObjBase<ObjectType::FUNCTION> {
   115	   private:
   116	    using proto_t = proto_t;
   117	    using upvalue_t = upvalue_t;
   118	    using visitor_t = GCVisitor;
   119	
   120	    proto_t proto_;
   121	    std::vector<upvalue_t> upvalues_;
   122	
   123	   public:
   124	    explicit ObjClosure(proto_t proto = nullptr) : proto_(proto), upvalues_(proto ? proto->get_num_upvalues() : 0) {}
   125	
   126	    inline proto_t get_proto() const noexcept {
   127	        return proto_;
   128	    }
   129	    /// @brief Unchecked upvalue access. For performance-critical code
   130	    inline upvalue_t get_upvalue(size_t index) const noexcept {
   131	        return upvalues_[index];
   132	    }
   133	    /// @brief Unchecked upvalue modification. For performance-critical code
   134	    inline void set_upvalue(size_t index, upvalue_t upvalue) noexcept {
   135	        upvalues_[index] = upvalue;
   136	    }
   137	    /// @brief Checked upvalue access. Throws if index is OOB
   138	    inline upvalue_t at_upvalue(size_t index) const {
   139	        return upvalues_.at(index);
   140	    }
   141	
   142	    size_t obj_size() const noexcept override { return sizeof(ObjClosure); }
   143	
   144	    void trace(visitor_t& visitor) const noexcept override;
   145	};
   146	}


// =============================================================================
//  FILE PATH: include/meow/core/hash_table.h
// =============================================================================

     1	/**
     2	 * @file hash_table.h
     3	 * @brief Optimized Hash Table with State-ful Allocator (meow::allocator)
     4	 * Compatible with std::map semantics (first/second)
     5	 */
     6	
     7	#pragma once
     8	
     9	#include <cstdint>
    10	#include <cstring>
    11	#include <bit> 
    12	#include <meow/common.h>
    13	#include <meow/core/meow_object.h>
    14	#include <meow/value.h>
    15	#include <meow/memory/gc_visitor.h>
    16	#include <meow/core/string.h>
    17	#include <meow_allocator.h> 
    18	
    19	namespace meow {
    20	
    21	struct Entry {
    22	    string_t first = nullptr;
    23	    Value second;
    24	};
    25	
    26	class ObjHashTable : public ObjBase<ObjectType::HASH_TABLE> {
    27	public:
    28	    using Allocator = meow::allocator<Entry>;
    29	
    30	private:
    31	    Entry* entries_ = nullptr;
    32	    uint32_t count_ = 0;
    33	    uint32_t capacity_ = 0;
    34	    uint32_t mask_ = 0;
    35	    
    36	    [[no_unique_address]] Allocator alloc_;
    37	
    38	    static constexpr double MAX_LOAD_FACTOR = 0.75;
    39	    static constexpr uint32_t MIN_CAPACITY = 8;
    40	
    41	public:
    42	    explicit ObjHashTable(Allocator alloc, uint32_t cap = 0) 
    43	        : alloc_(alloc) {
    44	        if (cap > 0) allocate(cap);
    45	    }
    46	
    47	    ~ObjHashTable() override {
    48	        if (entries_) {
    49	            alloc_.deallocate(entries_, capacity_);
    50	        }
    51	    }
    52	
    53	    // --- Core Operations ---
    54	
    55	    [[gnu::always_inline]] 
    56	    inline Entry* find_entry(Entry* entries, uint32_t mask, string_t key) const {
    57	        uint32_t index = key->hash() & mask;
    58	        for (;;) {
    59	            Entry* entry = &entries[index];
    60	            if (entry->first == key || entry->first == nullptr) [[likely]] {
    61	                return entry;
    62	            }
    63	            index = (index + 1) & mask;
    64	        }
    65	    }
    66	
    67	    [[gnu::always_inline]] 
    68	    inline bool set(string_t key, Value value) {
    69	        if (count_ + 1 > (capacity_ * MAX_LOAD_FACTOR)) [[unlikely]] {
    70	            grow();
    71	        }
    72	
    73	        Entry* entry = find_entry(entries_, mask_, key);
    74	        bool is_new = (entry->first == nullptr);
    75	        
    76	        if (is_new) [[likely]] {
    77	            count_++;
    78	            entry->first = key;
    79	        }
    80	        entry->second = value;
    81	        return is_new;
    82	    }
    83	
    84	    [[gnu::always_inline]] 
    85	    inline bool get(string_t key, Value* result) const {
    86	        if (count_ == 0) [[unlikely]] return false;
    87	        Entry* entry = find_entry(entries_, mask_, key);
    88	        if (entry->first == nullptr) return false;
    89	        *result = entry->second;
    90	        return true;
    91	    }
    92	
    93	    [[gnu::always_inline]]
    94	    inline Value get(string_t key) const {
    95	        if (count_ == 0) return Value(null_t{});
    96	        Entry* entry = find_entry(entries_, mask_, key);
    97	        if (entry->first == nullptr) return Value(null_t{});
    98	        return entry->second;
    99	    }
   100	
   101	    [[gnu::always_inline]] 
   102	    inline bool has(string_t key) const {
   103	        if (count_ == 0) return false;
   104	        return find_entry(entries_, mask_, key)->first != nullptr;
   105	    }
   106	
   107	    bool remove(string_t key) {
   108	        if (count_ == 0) return false;
   109	        Entry* entry = find_entry(entries_, mask_, key);
   110	        if (entry->first == nullptr) return false;
   111	
   112	        entry->first = nullptr;
   113	        entry->second = Value(null_t{});
   114	        count_--;
   115	
   116	        uint32_t index = (uint32_t)(entry - entries_);
   117	        uint32_t next_index = index;
   118	
   119	        for (;;) {
   120	            next_index = (next_index + 1) & mask_;
   121	            Entry* next = &entries_[next_index];
   122	            if (next->first == nullptr) break;
   123	
   124	            uint32_t ideal = next->first->hash() & mask_;
   125	            bool shift = false;
   126	            if (index < next_index) {
   127	                if (ideal <= index || ideal > next_index) shift = true;
   128	            } else {
   129	                if (ideal <= index && ideal > next_index) shift = true;
   130	            }
   131	
   132	            if (shift) {
   133	                entries_[index] = *next;
   134	                entries_[next_index].first = nullptr;
   135	                entries_[next_index].second = Value(null_t{});
   136	                index = next_index;
   137	            }
   138	        }
   139	        return true;
   140	    }
   141	
   142	    // --- Capacity ---
   143	    inline uint32_t size() const noexcept { return count_; }
   144	    inline bool empty() const noexcept { return count_ == 0; }
   145	    inline uint32_t capacity() const noexcept { return capacity_; }
   146	
   147	    class Iterator {
   148	        Entry* ptr_; Entry* end_;
   149	    public:
   150	        Iterator(Entry* ptr, Entry* end) : ptr_(ptr), end_(end) {
   151	            while (ptr_ < end_ && ptr_->first == nullptr) ptr_++;
   152	        }
   153	        Iterator& operator++() {
   154	            do { ptr_++; } while (ptr_ < end_ && ptr_->first == nullptr);
   155	            return *this;
   156	        }
   157	        bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }
   158	        
   159	        Entry& operator*() const { return *ptr_; }
   160	        Entry* operator->() const { return ptr_; }
   161	    };
   162	
   163	    inline Iterator begin() { return Iterator(entries_, entries_ + capacity_); }
   164	    inline Iterator end() { return Iterator(entries_ + capacity_, entries_ + capacity_); }
   165	
   166	    size_t obj_size() const noexcept override {
   167	        return sizeof(ObjHashTable) + sizeof(Entry) * capacity_;
   168	    }
   169	
   170	    void trace(GCVisitor& visitor) const noexcept override {
   171	        for (uint32_t i = 0; i < capacity_; i++) {
   172	            if (entries_[i].first) {
   173	                visitor.visit_object(entries_[i].first);
   174	                visitor.visit_value(entries_[i].second);
   175	            }
   176	        }
   177	    }
   178	
   179	private:
   180	    void allocate(uint32_t capacity) {
   181	        capacity_ = (capacity < MIN_CAPACITY) ? MIN_CAPACITY : std::bit_ceil(capacity);
   182	        mask_ = capacity_ - 1;
   183	        
   184	        entries_ = alloc_.allocate(capacity_);
   185	        std::memset(static_cast<void*>(entries_), 0, sizeof(Entry) * capacity_);
   186	    }
   187	
   188	    void grow() {
   189	        uint32_t old_cap = capacity_;
   190	        Entry* old_entries = entries_;
   191	
   192	        allocate(old_cap == 0 ? MIN_CAPACITY : old_cap * 2);
   193	        count_ = 0;
   194	
   195	        if (old_entries) {
   196	            for (uint32_t i = 0; i < old_cap; i++) {
   197	                if (old_entries[i].first != nullptr) {
   198	                    Entry* dest = find_entry(entries_, mask_, old_entries[i].first);
   199	                    dest->first = old_entries[i].first;
   200	                    dest->second = old_entries[i].second;
   201	                    count_++;
   202	                }
   203	            }
   204	            alloc_.deallocate(old_entries, old_cap);
   205	        }
   206	    }
   207	};
   208	
   209	} // namespace meow


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
    14	    ARRAY = base_t::index_of<object_t>() + 1, STRING, HASH_TABLE, INSTANCE, CLASS,
    15	    BOUND_METHOD, UPVALUE, PROTO, FUNCTION, MODULE, SHAPE
    16	};
    17	
    18	struct MeowObject {
    19	    const ObjectType type;
    20	    GCState gc_state = GCState::UNMARKED;
    21	
    22	    explicit MeowObject(ObjectType type_tag) noexcept : type(type_tag) {}
    23	    virtual ~MeowObject() = default;
    24	    
    25	    virtual void trace(GCVisitor& visitor) const noexcept = 0;
    26	    
    27	    virtual size_t obj_size() const noexcept = 0;
    28	
    29	    inline ObjectType get_type() const noexcept { return type; }
    30	    inline bool is_marked() const noexcept { return gc_state != GCState::UNMARKED; }
    31	    inline void mark() noexcept { if (gc_state == GCState::UNMARKED) gc_state = GCState::MARKED; }
    32	    inline void unmark() noexcept { if (gc_state != GCState::OLD) gc_state = GCState::UNMARKED; }
    33	};
    34	
    35	template <ObjectType type_tag>
    36	struct ObjBase : public MeowObject {
    37	    ObjBase() noexcept : MeowObject(type_tag) {}
    38	};
    39	}


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
   129	
   130	    size_t obj_size() const noexcept override { return sizeof(ObjModule); }
   131	};
   132	}


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

     1	/**
     2	 * @file oop.h
     3	 * @author LazyPaws
     4	 * @brief Core definition of Class, Instance, BoundMethod in TrangMeo
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
    21	#include <meow/core/shape.h>
    22	#include <meow_flat_map.h>
    23	
    24	namespace meow {
    25	class ObjClass : public ObjBase<ObjectType::CLASS> {
    26	private:
    27	    using string_t = string_t;
    28	    using class_t = class_t;
    29	    using method_map = meow::flat_map<string_t, value_t>;
    30	    using visitor_t = GCVisitor;
    31	
    32	    string_t name_;
    33	    class_t superclass_;
    34	    method_map methods_;
    35	
    36	public:
    37	    explicit ObjClass(string_t name = nullptr) noexcept : name_(name) {}
    38	
    39	    // --- Metadata ---
    40	    inline string_t get_name() const noexcept {
    41	        return name_;
    42	    }
    43	    inline class_t get_super() const noexcept {
    44	        return superclass_;
    45	    }
    46	    inline void set_super(class_t super) noexcept {
    47	        superclass_ = super;
    48	    }
    49	
    50	    // --- Methods ---
    51	    inline bool has_method(string_t name) const noexcept {
    52	        return methods_.contains(name);
    53	    }
    54	    
    55	    inline return_t get_method(string_t name) noexcept {
    56	        if (auto* val_ptr = methods_.find(name)) {
    57	            return *val_ptr;
    58	        }
    59	        return Value(null_t{});
    60	    }
    61	    
    62	    inline void set_method(string_t name, param_t value) noexcept {
    63	        methods_[name] = value;
    64	    }
    65	
    66	    size_t obj_size() const noexcept override { return sizeof(ObjClass); }
    67	
    68	    void trace(visitor_t& visitor) const noexcept override;
    69	};
    70	
    71	class ObjInstance : public ObjBase<ObjectType::INSTANCE> {
    72	private:
    73	    using string_t = string_t;
    74	    using class_t = class_t;
    75	    using visitor_t = GCVisitor;
    76	
    77	    class_t klass_;
    78	    Shape* shape_;              
    79	    std::vector<Value> fields_; 
    80	public:
    81	    explicit ObjInstance(class_t k, Shape* empty_shape) noexcept 
    82	        : klass_(k), shape_(empty_shape) {
    83	    }
    84	
    85	    // --- Metadata ---
    86	    inline class_t get_class() const noexcept { return klass_; }
    87	    inline void set_class(class_t klass) noexcept { klass_ = klass; }
    88	
    89	    inline Shape* get_shape() const noexcept { return shape_; }
    90	    inline void set_shape(Shape* s) noexcept { shape_ = s; }
    91	
    92	    inline Value get_field_at(int offset) const noexcept {
    93	        return fields_[offset];
    94	    }
    95	    
    96	    inline void set_field_at(int offset, Value value) noexcept {
    97	        fields_[offset] = value;
    98	    }
    99	    
   100	    inline void add_field(param_t value) noexcept {
   101	        fields_.push_back(value);
   102	    }
   103	
   104	    inline bool has_field(string_t name) const {
   105	        return shape_->get_offset(name) != -1;
   106	    }
   107	    
   108	    inline Value get_field(string_t name) const {
   109	        int offset = shape_->get_offset(name);
   110	        if (offset != -1) return fields_[offset];
   111	        return Value(null_t{});
   112	    }
   113	
   114	    size_t obj_size() const noexcept override { return sizeof(ObjInstance); }
   115	
   116	    void trace(visitor_t& visitor) const noexcept override {
   117	        visitor.visit_object(klass_);
   118	        visitor.visit_object(shape_);
   119	        for (const auto& val : fields_) {
   120	            visitor.visit_value(val);
   121	        }
   122	    }
   123	};
   124	
   125	class ObjBoundMethod : public ObjBase<ObjectType::BOUND_METHOD> {
   126	private:
   127	    Value receiver_; 
   128	    Value method_;   
   129	
   130	    using visitor_t = GCVisitor;
   131	public:
   132	    explicit ObjBoundMethod(Value receiver, Value method) noexcept 
   133	        : receiver_(receiver), method_(method) {}
   134	
   135	    inline Value get_receiver() const noexcept { return receiver_; }
   136	    inline Value get_method() const noexcept { return method_; }
   137	
   138	    size_t obj_size() const noexcept override { return sizeof(ObjBoundMethod); }
   139	
   140	    void trace(visitor_t& visitor) const noexcept override {
   141	        visitor.visit_value(receiver_);
   142	        visitor.visit_value(method_);
   143	    }
   144	};
   145	}


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
    21	
    22	private:
    23	    PropertyMap property_offsets_;
    24	    TransitionMap transitions_;     
    25	    uint32_t num_fields_ = 0;       
    26	
    27	public:
    28	    explicit Shape() = default;
    29	
    30	    int get_offset(string_t name) const;
    31	
    32	    Shape* get_transition(string_t name) const;
    33	
    34	    Shape* add_transition(string_t name, MemoryManager* heap);
    35	
    36	    inline uint32_t count() const { return num_fields_; }
    37	    
    38	    void copy_from(const Shape* other) {
    39	        property_offsets_ = other->property_offsets_;
    40	        num_fields_ = other->num_fields_;
    41	    }
    42	    
    43	    void add_property(string_t name) {
    44	        property_offsets_[name] = num_fields_++;
    45	    }
    46	
    47	    size_t obj_size() const noexcept override { return sizeof(Shape); }
    48	
    49	    void trace(GCVisitor& visitor) const noexcept override;
    50	};
    51	
    52	}


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
    11	    using visitor_t = GCVisitor;
    12	    
    13	    size_t length_;
    14	    size_t hash_;
    15	    char chars_[1]; 
    16	
    17	    friend class MemoryManager;
    18	    friend class heap; 
    19	    
    20	    ObjString(const char* chars, size_t length, size_t hash) 
    21	        : length_(length), hash_(hash) {
    22	        std::memcpy(chars_, chars, length);
    23	        chars_[length] = '\0'; 
    24	    }
    25	
    26	public:
    27	    ObjString() = delete; 
    28	    ObjString(const ObjString&) = delete;
    29	    
    30	    // --- Accessors ---
    31	    inline const char* c_str() const noexcept { return chars_; }
    32	    inline size_t size() const noexcept { return length_; }
    33	    inline bool empty() const noexcept { return length_ == 0; }
    34	    inline size_t hash() const noexcept { return hash_; }
    35	
    36	    inline char get(size_t index) const noexcept { return chars_[index]; }
    37	
    38	    inline void trace(visitor_t&) const noexcept override {}
    39	    
    40	    size_t obj_size() const noexcept override { 
    41	        return sizeof(ObjString) + length_; 
    42	    }
    43	};
    44	
    45	struct ObjStringHasher {
    46	    inline size_t operator()(string_t s) const noexcept {
    47	        return s->hash();
    48	    }
    49	};
    50	}


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
     6	#include <meow/common.h>
     7	#include "meow_variant.h"
     8	#include <meow/core/meow_object.h>
     9	
    10	namespace meow {
    11	
    12	class Value {
    13	private:
    14	    base_t data_;
    15	
    16	    template <typename Self>
    17	    inline auto get_object_ptr(this Self&& self) noexcept -> object_t {
    18	        if (auto* val_ptr = self.data_.template get_if<object_t>()) {
    19	            return *val_ptr;
    20	        }
    21	        return nullptr;
    22	    }
    23	
    24	public:
    25	    using layout_traits = base_t::layout_traits;
    26	
    27	    // --- Constructors ---
    28	    inline Value() noexcept : data_(null_t{}) {}
    29	    inline Value(null_t v) noexcept : data_(std::move(v)) {}
    30	    inline Value(bool_t v) noexcept : data_(v) {}
    31	    inline Value(int_t v) noexcept : data_(v) {}
    32	    inline Value(float_t v) noexcept : data_(v) {}
    33	    inline Value(native_t v) noexcept : data_(v) {}
    34	    inline Value(object_t v) noexcept : data_(v) {}
    35	
    36	    // --- Rule of five ---
    37	    inline Value(const Value& other) noexcept : data_(other.data_) {}
    38	    inline Value(Value&& other) noexcept : data_(std::move(other.data_)) {}
    39	
    40	    inline Value& operator=(const Value& other) noexcept {
    41	        if (this == &other) return *this;
    42	        data_ = other.data_;
    43	        return *this;
    44	    }
    45	    inline Value& operator=(Value&& other) noexcept {
    46	        if (this == &other) return *this;
    47	        data_ = std::move(other.data_);
    48	        return *this;
    49	    }
    50	    inline ~Value() noexcept = default;
    51	
    52	    // --- Assignment operators ---
    53	    inline Value& operator=(null_t v) noexcept { data_ = std::move(v); return *this; }
    54	    inline Value& operator=(bool_t v) noexcept { data_ = v; return *this; }
    55	    inline Value& operator=(int_t v) noexcept { data_ = v; return *this; }
    56	    inline Value& operator=(float_t v) noexcept { data_ = v; return *this; }
    57	    inline Value& operator=(native_t v) noexcept { data_ = v; return *this; }
    58	    inline Value& operator=(object_t v) noexcept { data_ = v; return *this; }
    59	
    60	    inline bool operator==(const Value& other) const noexcept {
    61	        return data_ == other.data_;
    62	    }
    63	    inline bool operator!=(const Value& other) const noexcept {
    64	        return data_ != other.data_;
    65	    }
    66	
    67	    inline constexpr size_t index() const noexcept { return data_.index(); }
    68	    inline uint64_t raw_tag() const noexcept { return data_.raw_tag(); }
    69	    inline void set_raw(uint64_t bits) noexcept { data_.set_raw(bits); }
    70	    
    71	    template <typename T>
    72	    inline bool holds_both(const Value& other) const noexcept {
    73	        return data_.template holds_both<T>(other.data_);
    74	    }
    75	
    76	    inline uint64_t raw() const noexcept { 
    77	        return data_.raw(); 
    78	    }
    79	
    80	    static inline Value from_raw(uint64_t bits) noexcept {
    81	        Value v;
    82	        v.set_raw(bits);
    83	        return v;
    84	    }
    85	
    86	    // === Type Checkers ===
    87	
    88	    // --- Primary type ---
    89	    inline bool is_null() const noexcept { return data_.holds<null_t>(); }
    90	    inline bool is_bool() const noexcept { return data_.holds<bool_t>(); }
    91	    inline bool is_int() const noexcept { return data_.holds<int_t>(); }
    92	    inline bool is_float() const noexcept { return data_.holds<float_t>(); }
    93	    inline bool is_native() const noexcept { return data_.holds<native_t>(); }
    94	
    95	    // --- Object type (generic) ---
    96	    inline bool is_object() const noexcept {
    97	        return get_object_ptr() != nullptr;
    98	    }
    99	
   100	    // --- Specific object type ---
   101	    inline bool is_array() const noexcept {
   102	        auto obj = get_object_ptr();
   103	        return (obj && obj->get_type() == ObjectType::ARRAY);
   104	    }
   105	    inline bool is_string() const noexcept {
   106	        auto obj = get_object_ptr();
   107	        return (obj && obj->get_type() == ObjectType::STRING);
   108	    }
   109	    inline bool is_hash_table() const noexcept {
   110	        auto obj = get_object_ptr();
   111	        return (obj && obj->get_type() == ObjectType::HASH_TABLE);
   112	    }
   113	    inline bool is_upvalue() const noexcept {
   114	        auto obj = get_object_ptr();
   115	        return (obj && obj->get_type() == ObjectType::UPVALUE);
   116	    }
   117	    inline bool is_proto() const noexcept {
   118	        auto obj = get_object_ptr();
   119	        return (obj && obj->get_type() == ObjectType::PROTO);
   120	    }
   121	    inline bool is_function() const noexcept {
   122	        auto obj = get_object_ptr();
   123	        return (obj && obj->get_type() == ObjectType::FUNCTION);
   124	    }
   125	    inline bool is_class() const noexcept {
   126	        auto obj = get_object_ptr();
   127	        return (obj && obj->get_type() == ObjectType::CLASS);
   128	    }
   129	    inline bool is_instance() const noexcept {
   130	        auto obj = get_object_ptr();
   131	        return (obj && obj->get_type() == ObjectType::INSTANCE);
   132	    }
   133	    inline bool is_bound_method() const noexcept {
   134	        auto obj = get_object_ptr();
   135	        return (obj && obj->get_type() == ObjectType::BOUND_METHOD);
   136	    }
   137	    inline bool is_module() const noexcept {
   138	        auto obj = get_object_ptr();
   139	        return (obj && obj->get_type() == ObjectType::MODULE);
   140	    }
   141	
   142	    // === Accessors (Unsafe / By Value) ===
   143	    inline bool as_bool() const noexcept { return data_.get<bool_t>(); }
   144	    inline int64_t as_int() const noexcept { return data_.get<int_t>(); }
   145	    inline double as_float() const noexcept { return data_.get<float_t>(); }
   146	    inline native_t as_native() const noexcept { return data_.get<native_t>(); }
   147	    
   148	    inline MeowObject* as_object() const noexcept {
   149	        return data_.get<object_t>();
   150	    }
   151	    inline array_t as_array() const noexcept {
   152	        return reinterpret_cast<array_t>(as_object());
   153	    }
   154	    inline string_t as_string() const noexcept {
   155	        return reinterpret_cast<string_t>(as_object());
   156	    }
   157	    inline hash_table_t as_hash_table() const noexcept {
   158	        return reinterpret_cast<hash_table_t>(as_object());
   159	    }
   160	    inline upvalue_t as_upvalue() const noexcept {
   161	        return reinterpret_cast<upvalue_t>(as_object());
   162	    }
   163	    inline proto_t as_proto() const noexcept {
   164	        return reinterpret_cast<proto_t>(as_object());
   165	    }
   166	    inline function_t as_function() const noexcept {
   167	        return reinterpret_cast<function_t>(as_object());
   168	    }
   169	    inline class_t as_class() const noexcept {
   170	        return reinterpret_cast<class_t>(as_object());
   171	    }
   172	    inline instance_t as_instance() const noexcept {
   173	        return reinterpret_cast<instance_t>(as_object());
   174	    }
   175	    inline bound_method_t as_bound_method() const noexcept {
   176	        return reinterpret_cast<bound_method_t>(as_object());
   177	    }
   178	    inline module_t as_module() const noexcept {
   179	        return reinterpret_cast<module_t>(as_object());
   180	    }
   181	
   182	    // === Safe Getters (Deducing 'this' - C++23) ===
   183	    
   184	    // --- Primitive Types ---
   185	    template <typename Self>
   186	    inline auto as_if_bool(this Self&& self) noexcept {
   187	        return self.data_.template get_if<bool_t>();
   188	    }
   189	
   190	    template <typename Self>
   191	    inline auto as_if_int(this Self&& self) noexcept {
   192	        return self.data_.template get_if<int_t>();
   193	    }
   194	
   195	    template <typename Self>
   196	    inline auto as_if_float(this Self&& self) noexcept {
   197	        return self.data_.template get_if<float_t>();
   198	    }
   199	
   200	    template <typename Self>
   201	    inline auto as_if_native(this Self&& self) noexcept {
   202	        return self.data_.template get_if<native_t>();
   203	    }
   204	
   205	    // --- Object Types ---
   206	    
   207	    // Array
   208	    template <typename Self>
   209	    inline auto as_if_array(this Self&& self) noexcept {
   210	        if (auto obj = self.get_object_ptr()) {
   211	            if (obj->get_type() == ObjectType::ARRAY) {
   212	                return reinterpret_cast<array_t>(obj);
   213	            }
   214	        }
   215	        return static_cast<array_t>(nullptr);
   216	    }
   217	
   218	    // String
   219	    template <typename Self>
   220	    inline auto as_if_string(this Self&& self) noexcept {
   221	        if (auto obj = self.get_object_ptr()) {
   222	            if (obj->get_type() == ObjectType::STRING) {
   223	                return reinterpret_cast<string_t>(obj);
   224	            }
   225	        }
   226	        return static_cast<string_t>(nullptr);
   227	    }
   228	
   229	    // Hash table
   230	    template <typename Self>
   231	    inline auto as_if_hash_table(this Self&& self) noexcept {
   232	        if (auto obj = self.get_object_ptr()) {
   233	            if (obj->get_type() == ObjectType::HASH_TABLE) {
   234	                return reinterpret_cast<hash_table_t>(obj);
   235	            }
   236	        }
   237	        return static_cast<hash_table_t>(nullptr);
   238	    }
   239	
   240	    // Upvalue
   241	    template <typename Self>
   242	    inline auto as_if_upvalue(this Self&& self) noexcept {
   243	        if (auto obj = self.get_object_ptr()) {
   244	            if (obj->get_type() == ObjectType::UPVALUE) {
   245	                return reinterpret_cast<upvalue_t>(obj);
   246	            }
   247	        }
   248	        return static_cast<upvalue_t>(nullptr);
   249	    }
   250	
   251	    // Proto
   252	    template <typename Self>
   253	    inline auto as_if_proto(this Self&& self) noexcept {
   254	        if (auto obj = self.get_object_ptr()) {
   255	            if (obj->get_type() == ObjectType::PROTO) {
   256	                return reinterpret_cast<proto_t>(obj);
   257	            }
   258	        }
   259	        return static_cast<proto_t>(nullptr);
   260	    }
   261	
   262	    // Function
   263	    template <typename Self>
   264	    inline auto as_if_function(this Self&& self) noexcept {
   265	        if (auto obj = self.get_object_ptr()) {
   266	            if (obj->get_type() == ObjectType::FUNCTION) {
   267	                return reinterpret_cast<function_t>(obj);
   268	            }
   269	        }
   270	        return static_cast<function_t>(nullptr);
   271	    }
   272	
   273	    // Class
   274	    template <typename Self>
   275	    inline auto as_if_class(this Self&& self) noexcept {
   276	        if (auto obj = self.get_object_ptr()) {
   277	            if (obj->get_type() == ObjectType::CLASS) {
   278	                return reinterpret_cast<class_t>(obj);
   279	            }
   280	        }
   281	        return static_cast<class_t>(nullptr);
   282	    }
   283	
   284	    // Instance
   285	    template <typename Self>
   286	    inline auto as_if_instance(this Self&& self) noexcept {
   287	        if (auto obj = self.get_object_ptr()) {
   288	            if (obj->get_type() == ObjectType::INSTANCE) {
   289	                return reinterpret_cast<instance_t>(obj);
   290	            }
   291	        }
   292	        return static_cast<instance_t>(nullptr);
   293	    }
   294	
   295	    // Bound method
   296	    template <typename Self>
   297	    inline auto as_if_bound_method(this Self&& self) noexcept {
   298	        if (auto obj = self.get_object_ptr()) {
   299	            if (obj->get_type() == ObjectType::BOUND_METHOD) {
   300	                return reinterpret_cast<bound_method_t>(obj);
   301	            }
   302	        }
   303	        return static_cast<bound_method_t>(nullptr);
   304	    }
   305	
   306	    // Module
   307	    template <typename Self>
   308	    inline auto as_if_module(this Self&& self) noexcept {
   309	        if (auto obj = self.get_object_ptr()) {
   310	            if (obj->get_type() == ObjectType::MODULE) {
   311	                return reinterpret_cast<module_t>(obj);
   312	            }
   313	        }
   314	        return static_cast<module_t>(nullptr);
   315	    }
   316	
   317	    // === Visitor ===
   318	    template <typename Visitor>
   319	    decltype(auto) visit(Visitor&& vis) {
   320	        return data_.visit(vis);
   321	    }
   322	    
   323	    template <typename Visitor>
   324	    decltype(auto) visit(Visitor&& vis) const {
   325	        return data_.visit(vis);
   326	    }
   327	
   328	    template <typename... Fs>
   329	    decltype(auto) visit(Fs&&... fs) {
   330	        return data_.visit(std::forward<Fs>(fs)...);
   331	    }
   332	
   333	    template <typename... Fs>
   334	    decltype(auto) visit(Fs&&... fs) const {
   335	        return data_.visit(std::forward<Fs>(fs)...);
   336	    }
   337	};
   338	
   339	}


