#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

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

const int64_t LIMIT = 10'000'000; 
int64_t run_native_cpp(int64_t limit) {
    int64_t sum = 0;
    int64_t counter = 0;
    int64_t step = 1;
    
    while (counter < limit) {
        sum = sum + step;
        counter = counter + step;
        __asm__ __volatile__("" : "+r" (counter));
    }

    return sum;
}

Chunk create_vm_chunk(MemoryManager& /*heap*/) {
    Chunk chunk;
    std::vector<Value> constants; 
    
    // --- SETUP (Chạy 1 lần, giữ nguyên u16 để an toàn) ---
    // R0 = sum = 0
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(0); chunk.write_u64(0);
    // R1 = counter = 0
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(1); chunk.write_u64(0);
    // R2 = step = 1
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(2); chunk.write_u64(1);
    // R3 = limit = 10,000,000
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(3); chunk.write_u64(static_cast<uint64_t>(LIMIT)); 

    // --- LOOP START (Hot Path - Tối ưu nén lệnh) ---
    size_t loop_start = chunk.get_code_size();

    // 1. ADD_B R0, R0, R2 (sum += step)
    // Cấu trúc: [OP_ADD_B] [dst:1] [r1:1] [r2:1] -> Tổng 4 bytes
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD_B)); 
    chunk.write_byte(0); chunk.write_byte(0); chunk.write_byte(2);

    // 2. ADD_B R1, R1, R2 (counter += step)
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD_B)); 
    chunk.write_byte(1); chunk.write_byte(1); chunk.write_byte(2);

    // 3. LT_B R4, R1, R3 (counter < limit ?)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LT_B));  
    chunk.write_byte(4); chunk.write_byte(1); chunk.write_byte(3);

    // 4. JUMP_IF_TRUE_B R4, loop_start
    // Cấu trúc: [OP_JUMP_B] [cond:1] [offset:2] -> Tổng 4 bytes
    chunk.write_byte(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE_B)); 
    chunk.write_byte(4); 
    chunk.write_u16(static_cast<uint16_t>(loop_start));

    // HALT
    chunk.write_byte(static_cast<uint8_t>(OpCode::HALT));

    Chunk final_chunk(std::vector<uint8_t>(chunk.get_code(), chunk.get_code() + chunk.get_code_size()), std::move(constants));
    return final_chunk;
}
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

    int64_t limit = (argc > 100) ? 1 : LIMIT;

    std::cout << "\n🏁 === VM vs NATIVE: SPEED BATTLE === 🏁\n";
    std::cout << "Iterations: " << LIMIT << "\n\n";

    char* fake_argv[] = { (char*)"meow", (char*)"bench" };
    Machine machine(".", "bench", 2, fake_argv);
    
    Chunk code = create_vm_chunk(*machine.heap_);
    auto proto = machine.heap_->new_proto(5, 0, machine.heap_->new_string("bench"), std::move(code));
    auto func = machine.heap_->new_function(proto);
    auto mod = machine.heap_->new_module(machine.heap_->new_string("bench"), machine.heap_->new_string("bench.meow"));

    run_native_cpp(limit); 
    
    double t_native = measure("Native C++ (Hardcoded)", [&]() {
        auto result = run_native_cpp(limit);
        if (result == 12345) std::cout << "";
    });

    double t_vm = measure("MeowVM (Interpreter)", [&]() {
        machine.context_->reset();
        
        // Setup Frame 0 manually
        Value* base = machine.context_->stack_;
        *machine.context_->frame_ptr_ = CallFrame(
            func, mod, base, nullptr, proto->get_chunk().get_code()
        );
        
        // Setup Pointers
        machine.context_->current_regs_ = base;
        machine.context_->stack_top_ += 5; 
        machine.context_->current_frame_ = machine.context_->frame_ptr_;

        const uint8_t* code_base = proto->get_chunk().get_code();

        VMState state{
            machine,
            *machine.context_,
            *machine.heap_,
            *machine.mod_manager_,
            machine.context_->current_regs_, 
            nullptr,
            code_base, 
            "", false
        };

        Interpreter::run(state);
    });

    std::cout << "\n--------------------------------------------------\n";
    std::cout << "📊 KẾT QUẢ:\n";
    
    // if (t_native < 0.001) t_native = 0.001;
    double ratio = t_vm / t_native;
    
    std::cout << "Native C++ : 1x (Baseline)\n";
    std::cout << "MeowVM     : " << std::fixed << std::setprecision(2) << ratio << "x slower\n";
    
    return 0;
}
