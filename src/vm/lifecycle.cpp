#include "vm/machine.h"
#include "common/pch.h"
#include "memory/mark_sweep_gc.h"
#include "memory/memory_manager.h"
#include "module/module_manager.h"
#include "runtime/execution_context.h"
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
    
    load_builtins(); 
}

Machine::~Machine() noexcept {
}

void Machine::interpret() noexcept {
    if (prepare()) {
        run();
    } else {
        if (has_error()) {
            std::println(stderr, "VM Init Error: {}", get_error_message());
        }
    }
}

bool Machine::prepare() noexcept {
    std::filesystem::path full_path = std::filesystem::path(args_.entry_point_directory_) / args_.entry_path_;
    
    auto path_str = heap_->new_string(full_path.string());
    auto importer_str = heap_->new_string(""); 

    try {
        module_t main_module = mod_manager_->load_module(path_str, importer_str);

        if (!main_module) {
            error("Could not load entry module (module is null).");
            return false;
        }

        auto native_name = heap_->new_string("native");
        module_t native_mod = mod_manager_->load_module(native_name, importer_str);
        
        if (native_mod) [[likely]] {
            main_module->import_all_global(native_mod);
        } else {
            printl("Warning: Could not inject 'native' module.");
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
        
        return true; 

    } catch (const std::exception& e) {
        error(std::format("Fatal error during preparation: {}", e.what()));
        return false;
    }
}