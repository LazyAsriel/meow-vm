#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <meow/common.h>
#include <meow/core/meow_object.h>
#include <meow/common.h>
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

    // --- Globals Storage ---
    std::vector<Value> globals_store_;
    using GlobalNameMap = meow::flat_map<string_t, uint32_t>;
    GlobalNameMap global_names_;

    // --- Exports Storage (Đã tối ưu) ---
    std::vector<Value> exports_store_; 
    using ExportNameMap = meow::flat_map<string_t, uint32_t>; // Map tên -> index
    ExportNameMap export_names_;

    string_t file_name_;
    string_t file_path_;
    proto_t main_proto_;

    State state;

public:
    explicit ObjModule(string_t file_name, string_t file_path, proto_t main_proto = nullptr) noexcept 
        : file_name_(file_name), file_path_(file_path), main_proto_(main_proto), state(State::INITIAL) {}
    
    // --- Globals Logic (Giữ nguyên logic tốt của bạn) ---
    [[gnu::always_inline]]
    inline return_t get_global_by_index(uint32_t index) const noexcept {
        // Unsafe access for speed, caller must ensure index is valid
        return globals_store_[index];
    }

    [[gnu::always_inline]]
    inline void set_global_by_index(uint32_t index, param_t value) noexcept {
        globals_store_[index] = value;
    }

    uint32_t intern_global(string_t name) {
        if (auto* idx_ptr = global_names_.find(name)) {
            return *idx_ptr;
        }
        uint32_t next_idx = static_cast<uint32_t>(globals_store_.size());
        global_names_.try_emplace(name, next_idx);
        globals_store_.push_back(Value(null_t{}));
        return next_idx;
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
        uint32_t idx = intern_global(name); // Tự động tạo slot nếu chưa có
        globals_store_[idx] = value;
    }

    inline void import_all_global(const module_t other) noexcept {
        const auto& other_keys = other->global_names_.keys();
        const auto& other_vals = other->global_names_.values(); // Đây là index
        
        for (size_t i = 0; i < other_keys.size(); ++i) {
            Value val = other->globals_store_[other_vals[i]];
            set_global(other_keys[i], val);
        }
    }

    // --- Exports Logic (Đã tối ưu Index Access) ---
    
    // 1. Lấy export qua index (Fast Path cho VM)
    [[gnu::always_inline]]
    inline return_t get_export_by_index(uint32_t index) const noexcept {
        return exports_store_[index];
    }

    // 2. Tìm index của export (Dùng để cache lần đầu)
    // Trả về -1 nếu không tìm thấy (lưu ý trả về int32 hoặc check size)
    inline int32_t resolve_export_index(string_t name) noexcept {
        if (auto* idx_ptr = export_names_.find(name)) {
            return static_cast<int32_t>(*idx_ptr);
        }
        return -1;
    }

    // 3. Intern export (Tạo slot mới)
    uint32_t intern_export(string_t name) {
        if (auto* idx_ptr = export_names_.find(name)) {
            return *idx_ptr;
        }
        uint32_t next_idx = static_cast<uint32_t>(exports_store_.size());
        export_names_.try_emplace(name, next_idx);
        exports_store_.push_back(Value(null_t{}));
        return next_idx;
    }

    // 4. Các hàm tiện ích cũ
    inline return_t get_export(string_t name) noexcept {
        int32_t idx = resolve_export_index(name);
        if (idx != -1) return exports_store_[idx];
        return Value(null_t{});
    }
    
    inline void set_export(string_t name, param_t value) noexcept {
        uint32_t idx = intern_export(name);
        exports_store_[idx] = value; 
    }
    
    inline bool has_export(string_t name) {
        return export_names_.contains(name);
    }
    
    inline void import_all_export(const module_t other) noexcept {
        const auto& other_keys = other->export_names_.keys();
        const auto& other_vals = other->export_names_.values(); // Index bên module kia
        
        for (size_t i = 0; i < other_keys.size(); ++i) {
            // Lấy giá trị thực từ module kia
            Value val = other->exports_store_[other_vals[i]];
            // Set vào module này (sẽ tạo index mới nếu cần)
            set_export(other_keys[i], val);
        }
    }

    // --- Getters / Setters ---
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
    // Trả về map tên -> index để debug
    const auto& get_export_names_raw() const { return export_names_; }
};}