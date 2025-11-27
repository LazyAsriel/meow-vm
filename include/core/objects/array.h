/**
 * @file array.h
 * @author LazyPaws
 * @brief Core definition of Array in TrangMeo (C++23 Refactored)
 */

#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/meow_object.h"
#include "core/value.h"
#include "memory/gc_visitor.h"

namespace meow {
class ObjArray : public ObjBase<ObjectType::ARRAY> {
private:
    using container_t = std::vector<value_t>;
    using visitor_t = GCVisitor;

    container_t elements_;
public:
    // --- Constructors & destructor ---
    ObjArray() = default;
    explicit ObjArray(const container_t& elements) : elements_(elements) {}
    explicit ObjArray(container_t&& elements) noexcept : elements_(std::move(elements)) {}
    explicit ObjArray(std::initializer_list<value_t> elements) : elements_(elements) {}

    // --- Rule of 5 ---
    ObjArray(const ObjArray&) = delete;
    ObjArray(ObjArray&&) = default;
    ObjArray& operator=(const ObjArray&) = delete;
    ObjArray& operator=(ObjArray&&) = delete;
    ~ObjArray() override = default;

    // --- Iterator types ---
    using iterator = container_t::iterator;
    using const_iterator = container_t::const_iterator;
    using reverse_iterator = container_t::reverse_iterator;
    using const_reverse_iterator = container_t::const_reverse_iterator;

    // --- Element access (C++23 Deducing this) ---

    /// @brief Unchecked element access. For performance-critical code
    template <typename Self>
    inline decltype(auto) get(this Self&& self, size_t index) noexcept {
        return std::forward<Self>(self).elements_[index]; 
    }

    /// @brief Checked element access. Throws if index is OOB
    template <typename Self>
    inline decltype(auto) at(this Self&& self, size_t index) {
        return std::forward<Self>(self).elements_.at(index);
    }

    // Gộp cả const và non-const operator[]
    template <typename Self>
    inline decltype(auto) operator[](this Self&& self, size_t index) noexcept {
        return std::forward<Self>(self).elements_[index];
    }

    // Gộp front/back
    template <typename Self>
    inline decltype(auto) front(this Self&& self) noexcept {
        return std::forward<Self>(self).elements_.front();
    }

    template <typename Self>
    inline decltype(auto) back(this Self&& self) noexcept {
        return std::forward<Self>(self).elements_.back();
    }

    // --- Unchecked modification (Explicitly non-const only) ---
    template <typename T>
    inline void set(size_t index, T&& value) noexcept {
        elements_[index] = std::forward<T>(value);
    }

    // --- Capacity ---
    inline size_t size() const noexcept { return elements_.size(); }
    inline bool empty() const noexcept { return elements_.empty(); }
    inline size_t capacity() const noexcept { return elements_.capacity(); }

    // --- Modifiers ---
    template <typename T>
    inline void push(T&& value) { elements_.emplace_back(std::forward<T>(value)); }
    inline void pop() noexcept { elements_.pop_back(); }
    
    template <typename... Args>
    inline void emplace(Args&&... args) { elements_.emplace_back(std::forward<Args>(args)...); }
    
    inline void resize(size_t size) { elements_.resize(size); }
    inline void reserve(size_t capacity) { elements_.reserve(capacity); }
    inline void shrink() { elements_.shrink_to_fit(); }
    inline void clear() { elements_.clear(); }

    // --- Iterators (C++23 Deducing this) ---
    
    template <typename Self>
    inline auto begin(this Self&& self) noexcept { return std::forward<Self>(self).elements_.begin(); }
    
    template <typename Self>
    inline auto end(this Self&& self) noexcept { return std::forward<Self>(self).elements_.end(); }

    template <typename Self>
    inline auto rbegin(this Self&& self) noexcept { return std::forward<Self>(self).elements_.rbegin(); }

    template <typename Self>
    inline auto rend(this Self&& self) noexcept { return std::forward<Self>(self).elements_.rend(); }

    void trace(visitor_t& visitor) const noexcept override;
};
}