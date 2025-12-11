#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

// --- TÀ THUẬT: MỞ KHÓA PRIVATE ĐỂ BENCHMARK ---
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

const int LIMIT = 10'000'000;

Chunk create_benchmark_chunk(MemoryManager& /*heap*/) {
    Chunk chunk;
    std::vector<Value> constants;
    
    constants.push_back(Value(static_cast<int64_t>(0)));
    constants.push_back(Value(static_cast<int64_t>(1)));
    constants.push_back(Value(static_cast<int64_t>(LIMIT)));

    // 0: LOAD_CONST R0, 0
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(0); chunk.write_u16(0);

    // 5: LOAD_CONST R1, 0
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(1); chunk.write_u16(0);

    // 10: LOAD_CONST R2, 1
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(2); chunk.write_u16(1);

    // 15: LOAD_CONST R3, 2
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(3); chunk.write_u16(2);

    // --- LOOP START (Offset 20) ---
    size_t loop_start = chunk.get_code_size();
    
    // 20: ADD R0, R0, R2
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(0); chunk.write_u16(0); chunk.write_u16(2);

    // 27: ADD R1, R1, R2
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(1); chunk.write_u16(1); chunk.write_u16(2);

    // 34: LT R4, R1, R3
    chunk.write_byte(static_cast<uint8_t>(OpCode::LT));
    chunk.write_u16(4); chunk.write_u16(1); chunk.write_u16(3);

    // 41: JUMP_IF_TRUE R4, loop_start
    chunk.write_byte(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE));
    chunk.write_u16(4); chunk.write_u16(static_cast<uint16_t>(loop_start));

    // 46: HALT
    chunk.write_byte(static_cast<uint8_t>(OpCode::HALT));

    Chunk final_chunk(std::vector<uint8_t>(chunk.get_code(), chunk.get_code() + chunk.get_code_size()), 
                      std::move(constants));
    return final_chunk;
}

template <typename Func>
double measure(const std::string& name, Func func) {
    std::cout << "Running " << name << "... " << std::flush;
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << ms << " ms\n";
    return ms;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    std::cout << "🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱\n";
    std::cout << "Scenario: Loop " << LIMIT << " iterations (ADD + LT + JUMP)\n\n";

    char* fake_argv[] = { (char*)"meow", (char*)"test" };
    Machine machine(".", "bench.meow", 2, fake_argv);
    
    Chunk code = create_benchmark_chunk(*machine.heap_);
    auto proto = machine.heap_->new_proto(5, 0, machine.heap_->new_string("bench"), std::move(code));
    auto func = machine.heap_->new_function(proto);
    auto mod = machine.heap_->new_module(machine.heap_->new_string("bench"), machine.heap_->new_string("bench.meow"));

    // --- BENCHMARK 1: MACHINE (COMPUTED GOTO) ---
    double t_machine = measure("Machine (Computed Goto)", [&]() {
        // [FIX] Reset context manually
        machine.context_->reset();
        
        // Setup Frame 0 manually
        Value* base = machine.context_->stack_;
        *machine.context_->frame_ptr_ = CallFrame(
            func, base, nullptr, proto->get_chunk().get_code()
        );
        
        // Setup Pointers
        machine.context_->current_regs_ = base;
        machine.context_->stack_top_ += 5; // Reserve 5 registers
        machine.context_->current_frame_ = machine.context_->frame_ptr_;

        machine.run();
    });

    // --- BENCHMARK 2: INTERPRETER (MUSTTAIL) ---
    double t_interp = measure("Interpreter (Musttail)", [&]() {
        // [FIX] Reset context manually
        machine.context_->reset();
        
        // Setup Frame 0 manually
        Value* base = machine.context_->stack_;
        *machine.context_->frame_ptr_ = CallFrame(
            func, base, nullptr, proto->get_chunk().get_code()
        );
        
        // Setup Pointers
        machine.context_->current_regs_ = base;
        machine.context_->stack_top_ += 5; 
        machine.context_->current_frame_ = machine.context_->frame_ptr_;

        // [FIX] Get instruction base pointer
        const uint8_t* code_base = proto->get_chunk().get_code();

        // Create State wrapper with correct constructor (Added instruction_base)
        VMState state{
            machine,
            *machine.context_,
            *machine.heap_,
            *machine.mod_manager_,
            machine.context_->current_regs_, 
            nullptr,         
            code_base,
            // [FIX] Thêm nullptr cho current_module
            nullptr,                       
            "", false
        };

        Interpreter::run(state);
    });

    std::cout << "\n📊 RESULT:\n";
    std::cout << "Machine (Computed Goto): " << t_machine << " ms\n";
    std::cout << "Interpreter (Musttail):  " << t_interp << " ms\n";
    
    double diff = t_interp / t_machine;
    std::cout << "Ratio: Interpreter is " << std::fixed << std::setprecision(2) << diff << "x time of Machine.\n";
    
    return 0;
}