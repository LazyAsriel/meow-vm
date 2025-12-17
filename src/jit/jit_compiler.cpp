#include "jit/jit_compiler.h"
#include "jit/jit_config.h"
#include "jit/x64/code_generator.h"

#include <iostream>
#include <cstring>

// Platform specific headers for memory management
#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

namespace meow::jit {

JitCompiler& JitCompiler::instance() {
    static JitCompiler instance;
    return instance;
}

// Bộ nhớ JIT: Cần quyền Read + Write (lúc ghi code) và Execute (lúc chạy)
// Vì lý do bảo mật (W^X), thường ta không nên để cả Write và Execute cùng lúc.
// Nhưng để đơn giản cho project này, ta sẽ set RWX (Read-Write-Execute).
// (Production grade thì nên: W -> mprotect -> X)

struct JitMemory {
    uint8_t* ptr = nullptr;
    size_t size = 0;
};

static JitMemory g_jit_mem;

void JitCompiler::initialize() {
    if (g_jit_mem.ptr) return; // Đã init

    size_t capacity = JIT_CACHE_SIZE;

#if defined(_WIN32)
    void* ptr = VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!ptr) {
        std::cerr << "[JIT] Failed to allocate executable memory (Windows)!" << std::endl;
        return;
    }
    g_jit_mem.ptr = static_cast<uint8_t*>(ptr);
#else
    void* ptr = mmap(nullptr, capacity, 
                     PROT_READ | PROT_WRITE | PROT_EXEC, 
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "[JIT] Failed to allocate executable memory (mmap)!" << std::endl;
        return;
    }
    g_jit_mem.ptr = static_cast<uint8_t*>(ptr);
#endif

    g_jit_mem.size = capacity;
    if (JIT_DEBUG_LOG) {
        std::cout << "[JIT] Initialized " << (capacity / 1024) << "KB executable memory at " 
                  << (void*)g_jit_mem.ptr << std::endl;
    }
}

void JitCompiler::shutdown() {
    if (!g_jit_mem.ptr) return;

#if defined(_WIN32)
    VirtualFree(g_jit_mem.ptr, 0, MEM_RELEASE);
#else
    munmap(g_jit_mem.ptr, g_jit_mem.size);
#endif
    g_jit_mem.ptr = nullptr;
    g_jit_mem.size = 0;
}

JitFunc JitCompiler::compile(const uint8_t* bytecode, size_t length) {
    if (!g_jit_mem.ptr) {
        initialize();
        if (!g_jit_mem.ptr) return nullptr;
    }

    // TODO: Quản lý bộ nhớ tốt hơn (Bump pointer allocator)
    // Hiện tại: Reset bộ đệm mỗi lần compile (Chỉ chạy được 1 hàm JIT tại 1 thời điểm)
    // Để chạy nhiều hàm, bạn cần một con trỏ `current_offset` toàn cục.
    static size_t current_offset = 0;

    // Align 16 bytes
    while (current_offset % 16 != 0) current_offset++;

    if (current_offset + length * 10 > g_jit_mem.size) { // Ước lượng size * 10
        std::cerr << "[JIT] Cache full! Resetting..." << std::endl;
        current_offset = 0; // Reset "ngây thơ"
    }

    uint8_t* buffer_start = g_jit_mem.ptr + current_offset;
    size_t remaining_size = g_jit_mem.size - current_offset;

    // Gọi Backend để sinh mã
    x64::CodeGenerator codegen(buffer_start, remaining_size);
    JitFunc fn = codegen.compile(bytecode, length);

    // Cập nhật offset (CodeGenerator đã ghi bao nhiêu bytes?)
    // Ta cần API lấy size từ CodeGenerator, nhưng hiện tại lấy qua con trỏ hàm trả về thì hơi khó.
    // Tốt nhất là CodeGenerator trả về struct { func_ptr, code_size }.
    // Tạm thời fix cứng: lấy current cursor từ codegen (cần sửa CodeGenerator public method).
    // Vì CodeGenerator::compile trả về void*, ta giả định nó ghi tiếp nối.
    // Hack tạm: truy cập vào asm bên trong (cần friend class hoặc getter).
    
    // Giả sử ta sửa CodeGenerator để trả về size hoặc tự tính:
    // CodeGenerator instance sẽ bị hủy, ta không query được size.
    // ==> Giải pháp: CodeGenerator::compile nên update 1 biến size tham chiếu.
    
    // (Để code chạy được ngay, ta cứ tăng offset một lượng an toàn hoặc để reset mỗi lần test)
    current_offset += (length * 10); // Hacky estimation

    if (JIT_DEBUG_LOG) {
        std::cout << "[JIT] Compiled bytecode len=" << length << " to addr=" << (void*)fn << std::endl;
    }

    return fn;
}

} // namespace meow::jit