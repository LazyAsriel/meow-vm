#include "vm/machine.h"
#include "common/pch.h"
#include "memory/mark_sweep_gc.h"
#include "memory/memory_manager.h"
#include "module/module_manager.h"
#include "runtime/builtin_registry.h"
#include "runtime/execution_context.h"
#include "runtime/operator_dispatcher.h"
#include "debug/print.h"

using namespace meow;

Machine::Machine(const std::string& entry_point_directory, const std::string& entry_path, int argc, char* argv[]) {
    args_.entry_point_directory_ = entry_point_directory;
    args_.entry_path_ = entry_path;
    for (int i = 0; i < argc; ++i) {
        args_.command_line_arguments_.emplace_back(argv[i]);
    }

    context_ = std::make_unique<ExecutionContext>();

    auto gc = std::make_unique<MarkSweepGC>(context_.get());

    heap_ = std::make_unique<MemoryManager>(std::move(gc));

    mod_manager_ = std::make_unique<ModuleManager>(heap_.get(), this);
    op_dispatcher_ = std::make_unique<OperatorDispatcher>(heap_.get());

    printl("Machine initialized successfully!");
    printl("Detected size of value is: {} bytes", sizeof(value_t));
}

Machine::~Machine() noexcept {
    printl("Machine shutting down.");
}

void Machine::interpret() noexcept {
    try {
        prepare();
        run();
    } catch (const std::exception& e) {
        printl("An execption was threw: {}", e.what());
    }
}

void Machine::prepare() noexcept {
    std::filesystem::path full_path = std::filesystem::path(args_.entry_point_directory_) / args_.entry_path_;
    
    printl("Preparing execution for: {}", full_path.string());

    auto path_str = heap_->new_string(full_path.string());
    
    auto importer_str = heap_->new_string(""); 

    try {
        module_t main_module = mod_manager_->load_module(path_str, importer_str);

        if (!main_module) {
            throw_vm_error("Could not load entry module.");
        }

        proto_t main_proto = main_module->get_main_proto();
        function_t main_func = heap_->new_function(main_proto);

        context_->registers_.resize(main_proto->get_num_registers());

        context_->call_stack_.emplace_back(
            main_func, 
            main_module, 
            0,
            static_cast<size_t>(-1),
            main_proto->get_chunk().get_code()
        );

        context_->current_frame_ = &context_->call_stack_.back();
        context_->current_base_ = context_->current_frame_->start_reg_;
        
        printl("Module loaded successfully. Starting VM loop...");

    } catch (const std::exception& e) {
        printl("Fatal error during preparation: {}", e.what());
        exit(1); 
    }
}