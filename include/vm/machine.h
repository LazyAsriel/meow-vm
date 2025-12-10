#pragma once

#include "common/pch.h"
#include "vm/meow_engine.h"

namespace meow {
struct ExecutionContext;
class MemoryManager;
class ModuleManager;

struct VMArgs {
    std::vector<std::string> command_line_arguments_;
    std::string entry_point_directory_;
    std::string entry_path_;
};

class Machine : public MeowEngine {
public:
    // --- Constructors ---
    explicit Machine(const std::string& entry_point_directory, const std::string& entry_path, int argc, char* argv[]);
    Machine(const Machine&) = delete;
    Machine(Machine&&) = delete;
    Machine& operator=(const Machine&) = delete;
    Machine& operator=(Machine&&) = delete;
    ~Machine() noexcept;

    // --- Public API ---
    void interpret() noexcept;
    inline MemoryManager* get_heap() const noexcept { return heap_.get(); }
    inline void error(std::string message) noexcept {
        has_error_ = true;
        error_message_ = std::move(message);
    }

    inline bool has_error() const noexcept { return has_error_; }
    
    inline std::string_view get_error_message() const noexcept { return error_message_; }
    
    inline void clear_error() noexcept { has_error_ = false; error_message_.clear(); }
private:
    // --- Subsystems ---
    std::unique_ptr<ExecutionContext> context_;
    std::unique_ptr<MemoryManager> heap_;
    std::unique_ptr<ModuleManager> mod_manager_;

    // --- Runtime arguments ---
    VMArgs args_;
    bool has_error_ = false;
    std::string error_message_;

    // --- Execution internals ---
    bool prepare() noexcept;
    void run() noexcept;
    void load_builtins();
};
}