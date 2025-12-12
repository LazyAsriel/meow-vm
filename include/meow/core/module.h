/**
 * @file module.h
 * @author LazyPaws
 * @brief Core definition of Module in TrangMeo
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <variant>
#include <memory>
#include <meow/definitions.h>
#include <meow/core/meow_object.h>
#include <meow/definitions.h>
#include <meow/value.h>
#include <meow/memory/gc_visitor.h>

namespace meow {
class ObjModule : public ObjBase<ObjectType::MODULE> {
private:
    using string_t = string_t;
    using proto_t = proto_t;
    using visitor_t = GCVisitor;

    enum class State { EXECUTING, EXECUTED };

    std::vector<Value> globals_store_;
    std::unordered_map<string_t, uint32_t> global_names_;

    std::unordered_map<string_t, value_t> exports_;
    
    string_t file_name_;
    string_t file_path_;
    proto_t main_proto_;

    State state;

public:
    explicit ObjModule(string_t file_name, string_t file_path, proto_t main_proto = nullptr) noexcept 
        : file_name_(file_name), file_path_(file_path), main_proto_(main_proto), state(State::EXECUTING) {}

    // --- Globals (Optimized) ---
    
    [[gnu::always_inline]]
    inline return_t get_global_by_index(uint32_t index) const noexcept {
        return globals_store_[index];
    }

    [[gnu::always_inline]]
    inline void set_global_by_index(uint32_t index, param_t value) noexcept {
        globals_store_[index] = value;
    }

    uint32_t intern_global(string_t name) {
        if (auto it = global_names_.find(name); it != global_names_.end()) {
            return it->second;
        }
        uint32_t index = static_cast<uint32_t>(globals_store_.size());
        globals_store_.push_back(Value(null_t{}));
        global_names_[name] = index;
        return index;
    }

    inline bool has_global(string_t name) {
        return global_names_.contains(name);
    }

    inline return_t get_global(string_t name) noexcept {
        if (auto it = global_names_.find(name); it != global_names_.end()) {
            return globals_store_[it->second];
        }
        return Value(null_t{});
    }

    inline void set_global(string_t name, param_t value) noexcept {
        uint32_t idx = intern_global(name);
        globals_store_[idx] = value;
    }

    inline void import_all_global(const module_t other) noexcept {
        for (const auto& [name, idx] : other->global_names_) {
            set_global(name, other->globals_store_[idx]);
        }
    }

    // --- Exports ---
    inline return_t get_export(string_t name) noexcept {
        return exports_[name];
    }
    inline void set_export(string_t name, param_t value) noexcept {
        exports_[name] = value;
    }
    inline bool has_export(string_t name) {
        return exports_.contains(name);
    }
    inline void import_all_export(const module_t other) noexcept {
        for (const auto& [key, value] : other->exports_) {
            exports_[key] = value;
        }
    }

    // --- Accessors ---
    inline string_t get_file_name() const noexcept { return file_name_; }
    inline string_t get_file_path() const noexcept { return file_path_; }
    inline proto_t get_main_proto() const noexcept { return main_proto_; }
    inline void set_main_proto(proto_t proto) noexcept { main_proto_ = proto; }
    inline bool is_has_main() const noexcept { return main_proto_ != nullptr; }

    inline void set_execution() noexcept { state = State::EXECUTING; }
    inline void set_executed() noexcept { state = State::EXECUTED; }
    inline bool is_executing() const noexcept { return state == State::EXECUTING; }
    inline bool is_executed() const noexcept { return state == State::EXECUTED; }

    // [FIX] Chỉ khai báo, implementation nằm trong .cpp
    void trace(visitor_t& visitor) const noexcept override;
};
}