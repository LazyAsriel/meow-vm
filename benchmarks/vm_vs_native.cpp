#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

// --- TÀ THUẬT: MỞ KHÓA PRIVATE ĐỂ BENCHMARK ---
// Cho phép truy cập vào ruột gan của Machine/Interpreter
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#define private public
#define protected public
#include "vm/machine.h"
#include "vm/interpreter.h"
#undef private
#undef protected
#pragma clang diagnostic pop

#include "memory/memory_manager.h"
#include "bytecode/chunk.h"
#include "bytecode/op_codes.h"

using namespace meow;

// Số lần lặp: 10 triệu
const int64_t LIMIT = 10'000'000; 

// --- 1. Logic Native C++ (Baseline) ---
// Hàm này cố gắng mô phỏng những gì VM làm: cộng, so sánh, loop.
// Sử dụng volatile để ngăn Compiler optimize quá mức (biến loop thành hằng số).
int64_t run_native_cpp() {
    volatile int64_t sum = 0;
    volatile int64_t counter = 0;
    int64_t step = 1;
    int64_t limit = LIMIT;

    // Logic tương đương:
    // loop:
    //   sum = sum + step
    //   counter = counter + step
    //   if counter < limit goto loop
    
    do {
        sum = sum + step;       // OpCode::ADD
        counter = counter + step; // OpCode::ADD
    } while (counter < limit);  // OpCode::LT + OpCode::JUMP_IF_TRUE

    return sum;
}

// --- 2. Logic Meow VM ---
// Tạo Chunk thủ công chứa bytecode tương đương logic trên
// R0 = sum, R1 = counter, R2 = step, R3 = limit, R4 = temp_comparison
Chunk create_vm_chunk(MemoryManager& /*heap*/) {
    Chunk chunk;
    std::vector<Value> constants;
    
    // Constants Pool
    constants.push_back(Value(static_cast<int64_t>(0)));     // 0: Init 0
    constants.push_back(Value(static_cast<int64_t>(1)));     // 1: Step 1
    constants.push_back(Value(static_cast<int64_t>(LIMIT))); // 2: Limit

    // --- SETUP (Chuẩn bị thanh ghi) ---
    // LOAD_CONST R0, 0 (sum = 0)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(0); chunk.write_u16(0);

    // LOAD_CONST R1, 0 (counter = 0)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(1); chunk.write_u16(0);

    // LOAD_CONST R2, 1 (step = 1)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(2); chunk.write_u16(1);

    // LOAD_CONST R3, 2 (limit)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(3); chunk.write_u16(2);

    // --- LOOP BODY (Offset bắt đầu từ đây) ---
    size_t loop_start = chunk.get_code_size();

    // ADD R0, R0, R2  (sum += step)
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(0); chunk.write_u16(0); chunk.write_u16(2);

    // ADD R1, R1, R2  (counter += step)
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(1); chunk.write_u16(1); chunk.write_u16(2);

    // LT R4, R1, R3   (check: counter < limit)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LT));
    chunk.write_u16(4); chunk.write_u16(1); chunk.write_u16(3);

    // JUMP_IF_TRUE R4, loop_start
    chunk.write_byte(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE));
    chunk.write_u16(4); chunk.write_u16(static_cast<uint16_t>(loop_start));

    // HALT
    chunk.write_byte(static_cast<uint8_t>(OpCode::HALT));

    // Reconstruct chunk with constants
    Chunk final_chunk(std::vector<uint8_t>(chunk.get_code(), chunk.get_code() + chunk.get_code_size()), 
                      std::move(constants));
    return final_chunk;
}

// Hàm đo thời gian tiện lợi
template <typename Func>
double measure(const std::string& name, Func func) {
    std::cout << "👉 " << std::left << std::setw(30) << name << "... " << std::flush;
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "\033[1;32m" << std::fixed << std::setprecision(2) << ms << " ms\033[0m\n";
    return ms;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    std::cout << "\n🏁 === VM vs NATIVE: SPEED BATTLE === 🏁\n";
    std::cout << "Iterations: " << LIMIT << "\n\n";

    // --- SETUP VM Environment ---
    // Giả lập arguments
    char* fake_argv[] = { (char*)"meow", (char*)"bench" };
    Machine machine(".", "bench", 2, fake_argv);
    
    // Tạo code và function giả
    Chunk code = create_vm_chunk(*machine.heap_);
    auto proto = machine.heap_->new_proto(5, 0, machine.heap_->new_string("bench"), std::move(code));
    auto func = machine.heap_->new_function(proto);
    auto mod = machine.heap_->new_module(machine.heap_->new_string("bench"), machine.heap_->new_string("bench.meow"));

    // --- ROUND 1: NATIVE C++ ---
    // Chạy 1 lần để warmup cache instruction (nếu cần), sau đó đo thật
    run_native_cpp(); 
    
    double t_native = measure("Native C++ (Hardcoded)", [&]() {
        run_native_cpp();
    });

    // --- ROUND 2: MeowVM Interpreter ---
    double t_vm = measure("MeowVM (Interpreter)", [&]() {
        // Reset context sạch sẽ trước khi chạy
        machine.context_->reset();
        machine.context_->registers_.resize(5);
        
        // Setup Call Stack thủ công
        machine.context_->call_stack_.emplace_back(
            func, mod, 0, -1, 
            proto->get_chunk().get_code() // IP Start
        );
        machine.context_->current_frame_ = &machine.context_->call_stack_.back();
        machine.context_->current_base_ = 0;

        // Run Interpreter Loop
        VMState state{
            machine,
            *machine.context_,
            *machine.heap_,
            *machine.mod_manager_,
            "", false
        };
        Interpreter::run(state);
    });

    // --- REPORT ---
    std::cout << "\n--------------------------------------------------\n";
    std::cout << "📊 KẾT QUẢ:\n";
    
    // Tránh chia cho 0 nếu Native quá nhanh (0ms)
    if (t_native < 0.001) t_native = 0.001;
    
    double ratio = t_vm / t_native;
    
    std::cout << "Native C++ : 1x (Baseline)\n";
    std::cout << "MeowVM     : " << std::fixed << std::setprecision(2) << ratio << "x slower\n";
    
    // Nhận xét vui vẻ
    if (ratio < 50) std::cout << "🚀 KINH HOÀNG! VM chạy nhanh gần bằng Native (chắc compiler optimize mất bytecode rồi)!\n";
    else if (ratio < 200) std::cout << "🏎️ RẤT TỐT! Đây là tốc độ của các top-tier interpreter (Lua, mRuby).\n";
    else if (ratio < 1000) std::cout << "🚗 ỔN! Mức trung bình của các VM stack-based (Python, PHP cũ).\n";
    else std::cout << "🐢 HƠI CHẬM! Có lẽ do `fallback_variant` hoặc cache miss. Thử Nanbox xem sao?\n";

    return 0;
}