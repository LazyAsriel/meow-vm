#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <meow/definitions.h>
#include <meow/core/meow_object.h>
#include <meow/definitions.h>
#include <meow/value.h>
#include <meow/memory/gc_visitor.h>
#include <meow_flat_map.h>

namespace meow {
class ObjModule : public ObjBase<ObjectType::MODULE> {
private:
    using string_t = string_t;
    using proto_t = proto_t;
    using visitor_t = GCVisitor;

    enum class State { INITIAL, EXECUTING, EXECUTED };

    std::vector<Value> globals_store_;
    
    using GlobalNameMap = meow::flat_map<string_t, uint32_t>;
    using ExportMap = meow::flat_map<string_t, value_t>;

    GlobalNameMap global_names_;
    ExportMap exports_;
    
    string_t file_name_;
    string_t file_path_;
    proto_t main_proto_;

    State state;

public:
    explicit ObjModule(string_t file_name, string_t file_path, proto_t main_proto = nullptr) noexcept 
        : file_name_(file_name), file_path_(file_path), main_proto_(main_proto), state(State::INITIAL) {}
    
    [[gnu::always_inline]]
    inline return_t get_global_by_index(uint32_t index) const noexcept {
        return globals_store_[index];
    }

    [[gnu::always_inline]]
    inline void set_global_by_index(uint32_t index, param_t value) noexcept {
        globals_store_[index] = value;
    }

    uint32_t intern_global(string_t name) {
        uint32_t next_idx = static_cast<uint32_t>(globals_store_.size());
        auto [ptr, inserted] = global_names_.try_emplace(name, next_idx);
        
        if (inserted) {
            globals_store_.push_back(Value(null_t{}));
        }
        
        return *ptr;
    }

    inline bool has_global(string_t name) {
        return global_names_.contains(name);
    }

    inline return_t get_global(string_t name) noexcept {
        if (auto* idx_ptr = global_names_.find(name)) {
            return globals_store_[*idx_ptr];
        }
        return Value(null_t{});
    }

    inline void set_global(string_t name, param_t value) noexcept {
        uint32_t idx = intern_global(name);
        globals_store_[idx] = value;
    }

    inline void import_all_global(const module_t other) noexcept {
        const auto& other_keys = other->global_names_.keys();
        const auto& other_vals = other->global_names_.values();
        
        for (size_t i = 0; i < other_keys.size(); ++i) {
            Value val = other->globals_store_[other_vals[i]];
            set_global(other_keys[i], val);
        }
    }

    // --- Exports ---
    inline return_t get_export(string_t name) noexcept {
        if (auto* val_ptr = exports_.find(name)) {
            return *val_ptr;
        }
        return Value(null_t{});
    }
    
    inline void set_export(string_t name, param_t value) noexcept {
        exports_[name] = value; 
    }
    
    inline bool has_export(string_t name) {
        return exports_.contains(name);
    }
    
    inline void import_all_export(const module_t other) noexcept {
        const auto& other_keys = other->exports_.keys();
        const auto& other_vals = other->exports_.values();
        
        for (size_t i = 0; i < other_keys.size(); ++i) {
            exports_.try_emplace(other_keys[i], other_vals[i]);
        }
    }

    inline string_t get_file_name() const noexcept { return file_name_; }
    inline string_t get_file_path() const noexcept { return file_path_; }
    inline proto_t get_main_proto() const noexcept { return main_proto_; }
    inline void set_main_proto(proto_t proto) noexcept { main_proto_ = proto; }
    inline bool is_has_main() const noexcept { return main_proto_ != nullptr; }

    inline void set_execution() noexcept { state = State::EXECUTING; }
    inline void set_executed() noexcept { state = State::EXECUTED; }
    inline bool is_executing() const noexcept { return state == State::EXECUTING; }
    inline bool is_executed() const noexcept { return state == State::EXECUTED; }

    friend void obj_module_trace(const ObjModule* mod, visitor_t& visitor);
    void trace(visitor_t& visitor) const noexcept override;
    
    const auto& get_global_names_raw() const { return global_names_; }
    const auto& get_exports_raw() const { return exports_; }

    size_t obj_size() const noexcept override { return sizeof(ObjModule); }
};
}