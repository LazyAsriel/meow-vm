#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

// --- TÀ THUẬT: MỞ KHÓA PRIVATE ĐỂ BENCHMARK ---
// Tắt warning của Clang về việc redefine macro private
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

// Tạo Chunk chứa vòng lặp 100 triệu lần
// R0 = sum, R1 = counter, R2 = step (1), R3 = limit
Chunk create_benchmark_chunk(MemoryManager& /*heap*/) {
    Chunk chunk;
    std::vector<Value> constants;
    
    // Constants - FIX: Ép kiểu tường minh sang int64_t để tránh lỗi ambiguous
    constants.push_back(Value(static_cast<int64_t>(0)));           // 0: Initial Sum/Counter
    constants.push_back(Value(static_cast<int64_t>(1)));           // 1: Step
    constants.push_back(Value(static_cast<int64_t>(LIMIT)));       // 2: Limit

    // Code
    // 0: LOAD_CONST R0, 0 (sum = 0)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(0); chunk.write_u16(0);

    // 5: LOAD_CONST R1, 0 (counter = 0)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(1); chunk.write_u16(0);

    // 10: LOAD_CONST R2, 1 (step = 1)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(2); chunk.write_u16(1);

    // 15: LOAD_CONST R3, 2 (limit = 100,000,000)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_CONST));
    chunk.write_u16(3); chunk.write_u16(2);

    // --- LOOP START (Offset 20) ---
    // 20: ADD R0, R0, R2 (sum += step)
    size_t loop_start = chunk.get_code_size();
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(0); chunk.write_u16(0); chunk.write_u16(2);

    // 27: ADD R1, R1, R2 (counter += step)
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD));
    chunk.write_u16(1); chunk.write_u16(1); chunk.write_u16(2);

    // 34: LT R4, R1, R3 (R4 = counter < limit)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LT));
    chunk.write_u16(4); chunk.write_u16(1); chunk.write_u16(3);

    // 41: JUMP_IF_TRUE R4, loop_start
    chunk.write_byte(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE));
    chunk.write_u16(4); chunk.write_u16(static_cast<uint16_t>(loop_start));

    // 46: HALT
    chunk.write_byte(static_cast<uint8_t>(OpCode::HALT));

    // Gán constant pool vào chunk thủ công
    Chunk final_chunk(std::vector<uint8_t>(chunk.get_code(), chunk.get_code() + chunk.get_code_size()), 
                      std::move(constants));
    return final_chunk;
}

// Hàm đo thời gian
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
    (void)argc; (void)argv; // Fix unused warning

    std::cout << "🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱\n";
    std::cout << "Scenario: Loop " << LIMIT << " iterations (ADD + LT + JUMP)\n\n";

    // 1. Setup Machine (Computed Goto)
    char* fake_argv[] = { (char*)"meow", (char*)"test" };
    Machine machine(".", "bench.meow", 2, fake_argv);
    
    // Tạo Chunk
    Chunk code = create_benchmark_chunk(*machine.heap_);
    
    // Tạo Proto giả
    auto proto = machine.heap_->new_proto(5, 0, machine.heap_->new_string("bench"), std::move(code));
    auto func = machine.heap_->new_function(proto);
    auto mod = machine.heap_->new_module(machine.heap_->new_string("bench"), machine.heap_->new_string("bench.meow"));

    // --- BENCHMARK 1: MACHINE (COMPUTED GOTO) ---
    double t_machine = measure("Machine (Computed Goto)", [&]() {
        // Reset context
        machine.context_->reset();
        machine.context_->registers_.resize(5);
        machine.context_->call_stack_.emplace_back(func, mod, 0, -1, proto->get_chunk().get_code());
        machine.context_->current_frame_ = &machine.context_->call_stack_.back();
        machine.context_->current_base_ = 0;

        machine.run();
    });

    // --- BENCHMARK 2: INTERPRETER (MUSTTAIL) ---
    // Chúng ta dùng lại Heap và Context của Machine cho công bằng
    double t_interp = measure("Interpreter (Musttail)", [&]() {
        // Reset context
        machine.context_->reset();
        machine.context_->registers_.resize(5);
        machine.context_->call_stack_.emplace_back(func, mod, 0, -1, proto->get_chunk().get_code());
        machine.context_->current_frame_ = &machine.context_->call_stack_.back();
        machine.context_->current_base_ = 0;

        // Tạo State wrapper
        VMState state{
            *machine.context_,
            *machine.heap_,
            *machine.mod_manager_,
            "", false
        };

        Interpreter::run(state);
    });

    std::cout << "\n📊 RESULT:\n";
    std::cout << "Machine (Computed Goto): " << t_machine << " ms\n";
    std::cout << "Interpreter (Musttail):  " << t_interp << " ms\n";
    
    double diff = t_interp / t_machine;
    std::cout << "Ratio: Interpreter is " << std::fixed << std::setprecision(2) << diff << "x time of Machine.\n";
    
    if (diff < 1.0) std::cout << "🏆 MUSTTAIL WINS! (Ảo thật đấy!)\n";
    else if (diff < 1.1) std::cout << "🤝 DRAW (Ngang ngửa nhau!)\n";
    else std::cout << "🏆 COMPUTED GOTO WINS! (Huyền thoại vẫn là huyền thoại)\n";

    return 0;
}