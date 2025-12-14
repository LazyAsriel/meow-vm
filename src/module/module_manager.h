#pragma once

#include "pch.h"
#include <meow/core/module.h>
#include <meow/definitions.h>

namespace meow {
    class Machine;
    class MemoryManager;
    struct GCVisitor;
}

namespace meow {
class ModuleManager {
public:
    explicit ModuleManager(MemoryManager* heap, Machine* vm) noexcept;
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager(ModuleManager&&) = default;
    ModuleManager& operator=(const ModuleManager&) = delete;
    ModuleManager& operator=(ModuleManager&&) = default;
    ModuleManager() = default;

    module_t load_module(string_t module_path, string_t importer_path);

    inline void reset_cache() noexcept {
        module_cache_.clear();
    }

    inline void add_cache(string_t name, module_t mod) {
        module_cache_[name] = mod;
    }

    void trace(GCVisitor& visitor) const noexcept;
private:
    std::unordered_map<string_t, module_t> module_cache_;
    MemoryManager* heap_;
    Machine* vm_;
    string_t entry_path_;
};
}