#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#define private public
#define protected public
#include <meow/machine.h>
#include "vm/interpreter.h"
#undef private
#undef protected
#pragma clang diagnostic pop

#include <meow/memory/memory_manager.h>
#include <meow/compiler/chunk.h>
#include <meow/compiler/op_codes.h>
#include "jit/jit_compiler.h"
#include "make_chunk.h"
#include <meow/config.h> // [CHANGE]

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

    std::cout << "\n🏁 === VM vs NATIVE: SPEED BATTLE (v" << MEOW_VERSION_STR << ") === 🏁\n";
    std::cout << "Iterations: " << LIMIT << "\n\n";

    char* fake_argv[] = { (char*)"meow", (char*)"bench" };
    Machine machine(".", "bench", 2, fake_argv);
    
    Chunk code = create_vm_chunk(LIMIT);
    auto proto = machine.heap_->new_proto(5, 0, machine.heap_->new_string("bench"), std::move(code));
    auto func = machine.heap_->new_function(proto);

    run_native_cpp(limit); // Warmup cache
    
    double t_native = measure("Native C++ (Hardcoded)", [&]() {
        run_native_cpp(limit);
    });

    double t_vm = measure("MeowVM (Interpreter)", [&]() {
        // [CHANGE] Code cũ dài dòng đã bay màu!
        machine.execute(func);
    });

    meow::jit::Compiler jit;
    const uint8_t* bytecode_ptr = proto->get_chunk().get_code();
    size_t bytecode_len = proto->get_chunk().get_code_size();
    
    auto jit_func = jit.compile(bytecode_ptr, bytecode_len);

    double t_jit = measure("MeowVM (JIT x64)", [&]() {
        machine.context_->reset();
        Value* regs = machine.context_->stack_;
        
        jit_func(regs);
    });

    std::cout << "\n--------------------------------------------------\n";
    std::cout << "📊 KẾT QUẢ:\n";
    
    double vm_ratio = t_vm / t_native;
    double jit_ratio = t_jit / t_native;

    std::cout << "Native C++ : 1x (Baseline)\n";
    std::cout << "MeowVM     : " << std::fixed << std::setprecision(2) << vm_ratio << "x slower\n";
    std::cout << "MeowJIT    : " << std::fixed << std::setprecision(2) << jit_ratio << "x slower\n";

    return 0;
}