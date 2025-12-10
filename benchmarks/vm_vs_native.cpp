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
int64_t run_native_cpp() {
    volatile int64_t sum = 0;
    volatile int64_t counter = 0;
    int64_t step = 1;
    int64_t limit = LIMIT;
    
    do {
        sum = sum + step;
        counter = counter + step;
    } while (counter < limit);

    return sum;
}

Chunk create_vm_chunk(MemoryManager& /*heap*/) {
    Chunk chunk;
    std::vector<Value> constants; 
    // --- SETUP ---
    
    // LOAD_INT R0, 0 (sum = 0)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT));
    chunk.write_u16(0); // dst = R0
    chunk.write_u64(0); // value = 0

    // LOAD_INT R1, 0 (counter = 0)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT));
    chunk.write_u16(1); // dst = R1
    chunk.write_u64(0); // value = 0

    // LOAD_INT R2, 1 (step = 1)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT));
    chunk.write_u16(2); // dst = R2
    chunk.write_u64(1); // value = 1

    // LOAD_INT R3, LIMIT (limit)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT));
    chunk.write_u16(3); // dst = R3
    chunk.write_u64(static_cast<uint64_t>(LIMIT)); 

    // --- LOOP BODY ---
    size_t loop_start = chunk.get_code_size();

    // ADD R0, R0, R2 (sum += step)
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(0); chunk.write_u16(0); chunk.write_u16(2);

    // ADD R1, R1, R2 (counter += step)
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(1); chunk.write_u16(1); chunk.write_u16(2);

    // LT R4, R1, R3 (counter < limit ?)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LT));
    chunk.write_u16(4); chunk.write_u16(1); chunk.write_u16(3);

    // JUMP_IF_TRUE R4, loop_start
    chunk.write_byte(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE));
    chunk.write_u16(4); chunk.write_u16(static_cast<uint16_t>(loop_start));

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

    std::cout << "\n🏁 === VM vs NATIVE: SPEED BATTLE === 🏁\n";
    std::cout << "Iterations: " << LIMIT << "\n\n";

    char* fake_argv[] = { (char*)"meow", (char*)"bench" };
    Machine machine(".", "bench", 2, fake_argv);
    
    Chunk code = create_vm_chunk(*machine.heap_);
    auto proto = machine.heap_->new_proto(5, 0, machine.heap_->new_string("bench"), std::move(code));
    auto func = machine.heap_->new_function(proto);
    auto mod = machine.heap_->new_module(machine.heap_->new_string("bench"), machine.heap_->new_string("bench.meow"));

    run_native_cpp(); 
    
    double t_native = measure("Native C++ (Hardcoded)", [&]() {
        run_native_cpp();
    });

    double t_vm = measure("MeowVM (Interpreter)", [&]() {
        machine.context_->reset();
        machine.context_->registers_.resize(5);
        
        machine.context_->call_stack_.emplace_back(
            func, mod, 0, -1, 
            proto->get_chunk().get_code()
        );
        machine.context_->current_frame_ = &machine.context_->call_stack_.back();
        machine.context_->current_base_ = 0;

        VMState state{
            machine,
            *machine.context_,
            *machine.heap_,
            *machine.mod_manager_,
            "", false
        };
        Interpreter::run(state);
    });

    std::cout << "\n--------------------------------------------------\n";
    std::cout << "📊 KẾT QUẢ:\n";
    
    if (t_native < 0.001) t_native = 0.001;
    
    double ratio = t_vm / t_native;
    
    std::cout << "Native C++ : 1x (Baseline)\n";
    std::cout << "MeowVM     : " << std::fixed << std::setprecision(2) << ratio << "x slower\n";
    
    if (ratio < 50) std::cout << "Chậm\n";
    else if (ratio < 200) std::cout << "Rất chậm\n";
    else if (ratio < 1000) std::cout << "Rất rất chậm\n";
    else std::cout << "Siêu chậm\n";

    return 0;
}