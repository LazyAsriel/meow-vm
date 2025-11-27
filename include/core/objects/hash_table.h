/**
 * @file hash_table.h
 * @author LazyPaws
 * @brief Core definition of Hash Table in TrangMeo
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
class ObjHashTable : public ObjBase<ObjectType::HASH_TABLE> {
   private:
    using key_t = string_t;
    using map_t = std::unordered_map<key_t, value_t>;
    using visitor_t = GCVisitor;

    map_t fields_;

   public:
    // --- Constructors & destructor---
    ObjHashTable() = default;
    explicit ObjHashTable(const map_t& fields) : fields_(fields) {
    }
    explicit ObjHashTable(map_t&& fields) noexcept : fields_(std::move(fields)) {
    }

    // --- Rule of 5 ---
    ObjHashTable(const ObjHashTable&) = delete;
    ObjHashTable(ObjHashTable&&) = delete;
    ObjHashTable& operator=(const ObjHashTable&) = delete;
    ObjHashTable& operator=(ObjHashTable&&) = delete;
    ~ObjHashTable() override = default;

    // --- Iterator types ---
    using iterator = map_t::iterator;
    using const_iterator = map_t::const_iterator;

    // --- Lookup ---

    // Unchecked lookup. For performance-critical code
    inline return_t get(key_t key) noexcept {
        return fields_[key];
    }
    // Unchecked lookup/update. For performance-critical code
    template <typename T>
    inline void set(key_t key, T&& value) noexcept {
        fields_[key] = std::forward<T>(value);
    }
    // Checked lookup. Throws if key is not found
    inline return_t at(key_t key) const {
        return fields_.at(key);
    }
    inline bool has(key_t key) const {
        return fields_.contains(key);
    }

    // --- Capacity ---
    inline size_t size() const noexcept {
        return fields_.size();
    }
    inline bool empty() const noexcept {
        return fields_.empty();
    }

    // --- Iterators ---
    template <typename Self>
    auto begin(this Self&& self) noexcept { return std::forward<Self>(self).fields_.begin(); }

    template <typename Self>
    auto end(this Self&& self) noexcept { return std::forward<Self>(self).fields_.end(); }
    void trace(visitor_t& visitor) const noexcept override;
};
}