#pragma once

#include "common/pch.h"
#include "core/objects/module.h"
#include "common/definitions.h"

namespace meow {
class MeowEngine;
class MemoryManager;
class ModuleManager {
   public:
    explicit ModuleManager(MemoryManager* heap, MeowEngine* engine) noexcept;
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager(ModuleManager&&) = default;
    ModuleManager& operator=(const ModuleManager&) = delete;
    ModuleManager& operator=(ModuleManager&&) = default;
    ModuleManager() = default;
    module_t load_module(string_t module_path, string_t importer_path);

    inline void reset_cache() noexcept {
        module_cache_.clear();
    }

    inline void add_cache(string_t name, const module_t& mod) {
        module_cache_[name] = mod;
    }

   private:
    std::unordered_map<string_t, module_t> module_cache_;
    string_t entry_path_;

    MemoryManager* heap_;
    MeowEngine* engine_;
};
}