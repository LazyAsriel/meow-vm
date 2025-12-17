/**
 * @file vm_vs_native.cpp
 * @brief Benchmark: Native C++ vs MeowVM Interpreter vs MeowVM JIT
 * @author Modified by Gemini (Teacher Mode)
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <string>
#include <functional>
#include <locale>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#define private public
#define protected public
#include <meow/machine.h>
#include "vm/interpreter.h"
#undef private
#undef protected
#pragma clang diagnostic pop
// --------------------------------------------------------------------------

#include <meow/memory/memory_manager.h>
#include <meow/bytecode/chunk.h>
#include <meow/bytecode/op_codes.h>
#include "jit/jit_compiler.h"
#include "make_chunk.h"
#include <meow/config.h>

using namespace meow;

// --- UTILS: Màu sắc và Log ---
namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string RED     = "\033[31m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string BLUE    = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN    = "\033[36m";
    const std::string BOLD    = "\033[1m";
}

struct BenchResult {
    double duration_ms;
    double mops;
    int64_t computed_value;
};

// Hàm đo thời gian tổng quát
template <typename Func>
BenchResult run_benchmark(const std::string& name, int64_t iterations, Func func) {
    std::cout << "👉 " << std::left << std::setw(30) << name << "... " << std::flush;
    
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double seconds = ms / 1000.0;
    double mops = (iterations / 1e6) / seconds;

    std::cout << Color::GREEN << std::fixed << std::setprecision(2) << ms << " ms" << Color::RESET;
    std::cout << " | " << Color::CYAN << std::setprecision(2) << mops << " Mops/s" << Color::RESET << "\n";

    return { ms, mops, 0 };
}

// Hàm format số (ví dụ: 1000000 -> 1,000,000)
struct ThousandsSeparator : std::numpunct<char> {
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\3"; }
};

// --- NATIVE C++ WORKLOAD ---
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

// --- MAIN ---
int main(int argc, char* argv[]) {
    // Setup format in số đẹp
    std::cout.imbue(std::locale(std::cout.getloc(), new ThousandsSeparator));

    // 1. Cấu hình số lần lặp
    int64_t limit = 10'000'000; 
    if (argc > 1) {
        try {
            limit = std::stoll(argv[1]);
        } catch (...) {
            std::cerr << Color::YELLOW << "⚠️ Invalid argument, using default limit.\n" << Color::RESET;
        }
    }

    std::cout << "\n" << Color::BOLD << Color::MAGENTA 
              << "🏁 === MEOW VM vs NATIVE C++: THE ULTIMATE SPEED BATTLE === 🏁" 
              << Color::RESET << "\n";
    std::cout << "VM Version: v" << MEOW_VERSION_STR << "\n";
    std::cout << "Iterations: " << limit << "\n";
    std::cout << "------------------------------------------------------------\n\n";

    // 2. Khởi tạo VM Environment
    char* fake_argv[] = { (char*)"meow", (char*)"bench" };
    Machine machine(".", "bench", 2, fake_argv);
    
    Chunk code = create_vm_chunk(limit); 
    
    auto proto = machine.heap_->new_proto(5, 0, machine.heap_->new_string("bench"), std::move(code));
    auto func = machine.heap_->new_function(proto);

    // -----------------------------------------------------------------------
    // ROUND 1: NATIVE C++
    // -----------------------------------------------------------------------
    int64_t native_result = 0;
    auto res_native = run_benchmark("Native C++ (Hardcoded)", limit, [&]() {
        native_result = run_native_cpp(limit);
    });
    res_native.computed_value = native_result;
    std::cout << "   ↳ Result: " << native_result << "\n\n";

    // -----------------------------------------------------------------------
    // ROUND 2: MEOW INTERPRETER
    // -----------------------------------------------------------------------
    auto res_vm = run_benchmark("MeowVM (Interpreter)", limit, [&]() {
        machine.execute(func);
    });
    int64_t vm_result = machine.context_->stack_[0].as_int(); 
    res_vm.computed_value = vm_result;
    std::cout << "   ↳ Result: " << vm_result << "\n\n";

    // -----------------------------------------------------------------------
    // ROUND 3: MEOW JIT (x64)
    // -----------------------------------------------------------------------
    std::cout << Color::YELLOW << "⚡ Preparing JIT Engine..." << Color::RESET << "\n";

    auto& jit = meow::jit::JitCompiler::instance();
    jit.initialize(); 

    const uint8_t* bytecode_ptr = proto->get_chunk().get_code();
    size_t bytecode_len = proto->get_chunk().get_code_size();

    // Compile
    auto jit_func = jit.compile(bytecode_ptr, bytecode_len);

    if (!jit_func) {
        std::cerr << Color::RED << "❌ JIT Compilation Failed! Skipping JIT benchmark.\n" << Color::RESET;
        return 1;
    }

    meow::Value regs[256];

    meow::VMState state {
        machine,
        *machine.context_,
        *machine.heap_,
        *machine.mod_manager_,
        regs
    };

    auto res_jit = run_benchmark("MeowVM (JIT x64)", limit, [&]() {
        machine.context_->reset(); // Reset stack/pc trước khi chạy
        jit_func(&state);
    });

    int64_t jit_result = machine.context_->stack_[0].as_int();
    res_jit.computed_value = jit_result;
    std::cout << "   ↳ Result: " << jit_result << "\n\n";

    // Dọn dẹp bộ nhớ JIT
    jit.shutdown();
    std::cout << "------------------------------------------------------------\n";
    std::cout << "📊 " << Color::BOLD << "SUMMARY REPORT" << Color::RESET << ":\n";

    bool correct = (native_result == vm_result) && (native_result == jit_result);
    if (correct) {
        std::cout << "✅ Logic Check: " << Color::GREEN << "PASSED" << Color::RESET << " (All results match)\n";
    } else {
        std::cout << "❌ Logic Check: " << Color::RED << "FAILED" << Color::RESET << "\n";
        std::cout << "   Native: " << native_result << "\n";
        std::cout << "   VM    : " << vm_result << "\n";
        std::cout << "   JIT   : " << jit_result << "\n";
    }

    double vm_slowdown = res_vm.duration_ms / res_native.duration_ms;
    double jit_slowdown = res_jit.duration_ms / res_native.duration_ms;
    double jit_speedup_vs_vm = res_vm.duration_ms / res_jit.duration_ms;

    std::cout << "\n📈 Performance Matrix (Baseline: Native C++):\n";
    std::cout << "   Native C++ : 1.00x \n";
    std::cout << "   Meow JIT   : " << std::fixed << std::setprecision(2) << jit_slowdown << "x slower ("
              << (jit_slowdown < vm_slowdown ? Color::GREEN : Color::RED) << "Better" << Color::RESET << ")\n";
    std::cout << "   Interpreter: " << std::fixed << std::setprecision(2) << vm_slowdown << "x slower\n";

    std::cout << "\n🚀 JIT Improvement:\n";
    std::cout << "   JIT is " << Color::BOLD << Color::GREEN 
              << std::fixed << std::setprecision(2) << jit_speedup_vs_vm 
              << "x FASTER" << Color::RESET << " than Interpreter!\n";

    std::cout << "------------------------------------------------------------\n";

    return 0;
}