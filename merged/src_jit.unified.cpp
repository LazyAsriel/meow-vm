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


