#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>

// --- 1. DYNAMIC TYPE SYSTEM (TAGGED POINTERS) ---
// Ta dùng 64-bit value. 
// Nếu bit cuối cùng là 1 -> Nó là Pointer (Object, String...).
// Nếu bit cuối cùng là 0 -> Nó là Small Integer (Shift đi 1 bit).
// Đây là kỹ thuật dùng trong OCaml, Ruby cũ, v.v.

using Value = uint64_t;

const uint64_t TAG_MASK = 1;
const uint64_t TAG_INT  = 0;
const uint64_t TAG_PTR  = 1;

// Helper macros (inline cực nhanh)
#define IS_INT(v)      (((v) & TAG_MASK) == TAG_INT)
#define AS_INT(v)      ((int64_t)(v) >> 1)
#define MAKE_INT(num)  (((int64_t)(num) << 1) | TAG_INT)

// Error handling giả lập
void runtime_error(const char* msg) {
    std::cerr << "Runtime Error: " << msg << std::endl;
    exit(1);
}

// --- 2. OPCODE ---
enum OpCode : uint8_t {
    OP_LOAD_CONST, // Load hằng số từ pool
    OP_LOAD_VAR,   // Load biến từ stack frame
    OP_STORE_VAR,  // Lưu biến vào stack frame
    OP_ADD,        // Cộng (có check type)
    OP_LT,         // So sánh < (có check type)
    OP_JUMP_FALSE, // Nhảy nếu false
    OP_JUMP,       // Nhảy không điều kiện
    OP_RETURN      // Kết thúc
};

// --- 3. VM ---
struct VM {
    std::vector<uint8_t> bytecode; // Giả lập đọc từ file .pyc / .meow
    std::vector<Value> constants;  // Constant pool
    Value stack[1024];             // Stack memory (hoặc Register file giả lập)
    Value globals[256];            // Biến toàn cục

    VM() {
        // Init globals với Garbage để đảm bảo không cheat
        memset(globals, 0, sizeof(globals));
    }

    void run() {
        // Caching các biến quan trọng vào thanh ghi CPU cục bộ
        uint8_t* ip = bytecode.data();
        Value* regs = globals; // Giả lập register-based cho biến cục bộ
        
        // Computed Goto Table
        static void* dispatch_table[] = {
            &&CASE_LOAD_CONST,
            &&CASE_LOAD_VAR,
            &&CASE_STORE_VAR,
            &&CASE_ADD,
            &&CASE_LT,
            &&CASE_JUMP_FALSE,
            &&CASE_JUMP,
            &&CASE_RETURN
        };

        #define READ_BYTE() (*ip++)
        #define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
        // Dispatch next instruction
        #define DISPATCH() goto *dispatch_table[READ_BYTE()]

        // Bắt đầu
        goto *dispatch_table[READ_BYTE()];

        // --- INSTRUCTION HANDLERS ---

        CASE_LOAD_CONST: {
            uint8_t idx = READ_BYTE();
            uint8_t reg = READ_BYTE();
            regs[reg] = constants[idx];
            DISPATCH();
        }

        CASE_LOAD_VAR: {
            // Logic load var phức tạp hơn
            DISPATCH();
        }
        
        CASE_STORE_VAR: {
             // Logic store
             DISPATCH();
        }

        CASE_ADD: {
            // Cú pháp: ADD dest, src1, src2
            uint8_t dest = READ_BYTE();
            Value v1 = regs[READ_BYTE()];
            Value v2 = regs[READ_BYTE()];

            // 🔥 TYPE CHECKING (Luật chơi công bằng) 🔥
            // Kiểm tra cả 2 có phải là INT không.
            // Sử dụng bitwise OR để check cả 2 cùng lúc cho nhanh.
            if (((v1 | v2) & TAG_MASK) == TAG_INT) [[likely]] {
                // Fast path: Cộng Integer
                // Cần trừ đi TAG_INT (là 0) nhưng vì dạng shift nên ta cộng trực tiếp
                // rồi xử lý lại bit tag nếu cần. 
                // Cách an toàn: decode -> add -> encode
                regs[dest] = MAKE_INT(AS_INT(v1) + AS_INT(v2));
            } else {
                // Slow path: Float, String concat, hoặc Object __add__
                runtime_error("Type mismatch or not implemented for Objects yet");
            }
            DISPATCH();
        }

        CASE_LT: {
            // LT dest, src1, src2 (Lưu 1 hoặc 0 vào dest)
            uint8_t dest = READ_BYTE();
            Value v1 = regs[READ_BYTE()];
            Value v2 = regs[READ_BYTE()];

            if (((v1 | v2) & TAG_MASK) == TAG_INT) [[likely]] {
                regs[dest] = (AS_INT(v1) < AS_INT(v2)) ? MAKE_INT(1) : MAKE_INT(0);
            } else {
                runtime_error("Comparison not supported for types");
            }
            DISPATCH();
        }

        CASE_JUMP_FALSE: {
            // JMP_FALSE reg, offset
            Value cond = regs[READ_BYTE()];
            uint16_t offset = READ_SHORT();
            
            // Check xem có phải là False (0) không
            if (cond == MAKE_INT(0)) {
                ip += offset; 
                // Cần goto ngay vì IP đã thay đổi
                goto *dispatch_table[READ_BYTE()];
            }
            DISPATCH();
        }

        CASE_JUMP: {
            uint16_t offset = READ_SHORT();
            ip -= offset; // Jump back (Loop)
            goto *dispatch_table[READ_BYTE()];
        }

        CASE_RETURN: {
            return;
        }
    }
};

int main() {
    VM vm;
    
    // Constant Pool
    vm.constants.push_back(MAKE_INT(0));          // idx 0
    vm.constants.push_back(MAKE_INT(10000000));   // idx 1 (10M)
    vm.constants.push_back(MAKE_INT(1));          // idx 2 (1)

    std::vector<uint8_t>& b = vm.bytecode;

    // --- SETUP (Offset 0 -> 12) ---
    // 4 lệnh LOAD_CONST, mỗi lệnh 3 bytes (1 Op + 1 Idx + 1 Reg)
    // 4 * 3 = 12 bytes.
    // IP bắt đầu Loop sẽ ở index 12.
    
    // LOAD_CONST 0 -> R0 (i=0)
    b.push_back(OP_LOAD_CONST); b.push_back(0); b.push_back(0);
    // LOAD_CONST 0 -> R1 (total=0)
    b.push_back(OP_LOAD_CONST); b.push_back(0); b.push_back(1);
    // LOAD_CONST 1 -> R2 (limit=10M)
    b.push_back(OP_LOAD_CONST); b.push_back(1); b.push_back(2);
    // LOAD_CONST 2 -> R3 (const=1)
    b.push_back(OP_LOAD_CONST); b.push_back(2); b.push_back(3);

    // --- LOOP START (Index 12) ---
    
    // [12] LT R4, R0, R2 (4 bytes) -> IP tăng lên 16
    b.push_back(OP_LT); b.push_back(4); b.push_back(0); b.push_back(2);
    
    // [16] JUMP_FALSE R4, offset (4 bytes: Op + Reg + Short) -> IP tăng lên 20
    // Ta cần nhảy tới EXIT (Index 31).
    // Offset = Target - Current_IP = 31 - 20 = 11.
    b.push_back(OP_JUMP_FALSE); b.push_back(4); 
    b.push_back(0); b.push_back(11); // Offset 11 (High 0, Low 11)

    // --- BODY ---
    
    // [20] ADD R1, R1, R0 (4 bytes) -> IP tăng lên 24
    b.push_back(OP_ADD); b.push_back(1); b.push_back(1); b.push_back(0);

    // [24] ADD R0, R0, R3 (4 bytes) -> IP tăng lên 28
    b.push_back(OP_ADD); b.push_back(0); b.push_back(0); b.push_back(3);

    // [28] JUMP offset (3 bytes: Op + Short) -> IP tăng lên 31
    // Ta cần nhảy về LOOP START (Index 12).
    // Offset = Current_IP - Target = 31 - 12 = 19.
    b.push_back(OP_JUMP); 
    b.push_back(0); b.push_back(19); // Offset 19 (High 0, Low 19)

    // --- EXIT (Index 31) ---
    b.push_back(OP_RETURN);

    std::cout << "🦁 Lion VM (Fixed Offsets) vs CPython" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    vm.run();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    // Tổng 0 -> 9,999,999 là 49999995000000
    std::cout << "Result: " << AS_INT(vm.globals[1]) << std::endl; 
    std::cout << "Time: " << diff.count() * 1000 << " ms" << std::endl;

    return 0;
}