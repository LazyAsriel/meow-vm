/**
 * @file module.h
 * @author LazyPaws
 * @brief Core definition of Module in TrangMeo
 * @copyright Copyright (c) 2025 LazyPaws
 * @license All rights reserved. Unauthorized copying of this file, in any form
 * or medium, is strictly prohibited
 */

#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/meow_object.h"
#include "common/definitions.h"
#include "core/value.h"
#include "memory/gc_visitor.h"

namespace meow {
class ObjModule : public ObjBase<ObjectType::MODULE> {
private:
    using string_t = string_t;
    using proto_t = proto_t;
    using module_map = std::unordered_map<string_t, value_t>;
    using visitor_t = GCVisitor;

    enum class State { EXECUTING, EXECUTED };

    module_map globals_;
    module_map exports_;
    string_t file_name_;
    string_t file_path_;
    proto_t main_proto_;

    State state;

public:
    explicit ObjModule(string_t file_name, string_t file_path, proto_t main_proto = nullptr) noexcept : file_name_(file_name), file_path_(file_path), main_proto_(main_proto) {}

    // --- Globals ---
    inline return_t get_global(string_t name) noexcept {
        return globals_[name];
    }
    inline void set_global(string_t name, param_t value) noexcept {
        globals_[name] = value;
    }
    inline bool has_global(string_t name) {
        return globals_.contains(name);
    }
    inline void import_all_global(const module_t other) noexcept {
        for (const auto& [key, value] : other->globals_) {
            globals_[key] = value;
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

    // --- File info ---
    inline string_t get_file_name() const noexcept {
        return file_name_;
    }
    inline string_t get_file_path() const noexcept {
        return file_path_;
    }

    // --- Main proto ---
    inline proto_t get_main_proto() const noexcept {
        return main_proto_;
    }
    inline void set_main_proto(proto_t proto) noexcept {
        main_proto_ = proto;
    }
    inline bool is_has_main() const noexcept {
        return main_proto_ != nullptr;
    }

    // --- Execution state ---
    inline void set_execution() noexcept {
        state = State::EXECUTING;
    }
    inline void set_executed() noexcept {
        state = State::EXECUTED;
    }
    inline bool is_executing() const noexcept {
        return state == State::EXECUTING;
    }
    inline bool is_executed() const noexcept {
        return state == State::EXECUTED;
    }

    void trace(visitor_t& visitor) const noexcept override;
};
}


// /**
//  * @file module.h
//  * @brief Module: The container of Code, Data, and Interface.
//  */

// #pragma once

// #include <vector>
// #include "common/pch.h"
// #include "core/meow_object.h"
// #include "core/value.h"

// namespace meow {

// class ObjModule final : public ObjBase<ObjectType::MODULE> {
// public:
//     enum class State : uint8_t { LOADING, RUNNING, READY };
// private:
//     // --- Data Storage (Flat Layout) ---
//     std::vector<value_t> globals_;
//     std::vector<value_t> exports_;
    
//     std::vector<string_t> global_names_; 
//     std::vector<string_t> export_names_;

//     // --- Metadata ---
//     string_t path_;
//     proto_t  entry_point_;
//     State    state_{ State::LOADING };

// public:
//     explicit ObjModule(string_t path, proto_t main = nullptr) noexcept
//         : path_(path), entry_point_(main) {}

//     inline void resize_globals(uint32_t size) {
//         globals_.resize(size, {});
//         global_names_.resize(size, nullptr);
//     }

//     [[nodiscard]] inline value_t global(uint32_t idx) const noexcept {
//         return globals_[idx];
//     }

//     inline void set_global(uint32_t idx, value_t val) noexcept {
//         globals_[idx] = val;
//     }

//     inline void resize_exports(uint32_t size) {
//         exports_.resize(size, {});
//         export_names_.resize(size, nullptr);
//     }

//     [[nodiscard]] inline value_t exported(uint32_t idx) const noexcept {
//         return exports_[idx];
//     }

//     inline void set_exported(uint32_t idx, value_t val) noexcept {
//         exports_[idx] = val;
//     }

//     void name_global(uint32_t idx, string_t name) {
//         if (idx < global_names_.size()) global_names_[idx] = name;
//     }
    
//     void name_export(uint32_t idx, string_t name) {
//         if (idx < export_names_.size()) export_names_[idx] = name;
//     }

//     [[nodiscard]] value_t find_global(string_t name) const noexcept {
//         return find_in(globals_, global_names_, name);
//     }

//     [[nodiscard]] value_t find_exported(string_t name) const noexcept {
//         return find_in(exports_, export_names_, name);
//     }
//     [[nodiscard]] inline string_t path()  const noexcept { return path_; }
//     [[nodiscard]] inline proto_t  entry() const noexcept { return entry_point_; }
    
//     inline void set_entry(proto_t p) noexcept { entry_point_ = p; }

//     inline void set_state(State s) noexcept { state_ = s; }
//     [[nodiscard]] inline bool is_ready() const noexcept { return state_ == State::READY; }

//     // --- GC ---
//     void trace(GCVisitor& v) const noexcept override;

// private:
//     static value_t find_in(const std::vector<value_t>& vals, 
//                            const std::vector<string_t>& names, 
//                            string_t key) {
//         for (size_t i = 0; i < names.size(); ++i) {
//             if (names[i] == key) return vals[i];
//         }
//         return {};
//     }
// };

// } // namespace meow