/**
 * @file array.h
 * @author LazyPaws
 * @brief Core definition of Array in TrangMeo (C++23 Refactored with Arena)
 */

#pragma once

#include <cstdint>
#include <vector>
#include <meow/core/meow_object.h>
#include <meow/value.h>
#include <meow/memory/gc_visitor.h>
#include "meow_allocator.h"

namespace meow {
class ObjArray : public ObjBase<ObjectType::ARRAY> {
public:
    using allocator_t = meow::allocator<value_t>;
    using container_t = std::vector<value_t, allocator_t>;
    
private:
    using visitor_t = GCVisitor;
    container_t elements_;

public:
    explicit ObjArray(allocator_t alloc) : elements_(alloc) {}

    ObjArray(const std::vector<value_t>& elements, allocator_t alloc) 
        : elements_(elements.begin(), elements.end(), alloc) {}

    ObjArray(container_t&& elements) noexcept 
        : elements_(std::move(elements)) {}

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

    // --- Accessors & Modifiers (Giữ nguyên logic cũ, chỉ thay đổi container type ngầm) ---
    
    template <typename Self>
    inline decltype(auto) get(this Self&& self, size_t index) noexcept {
        return std::forward<Self>(self).elements_[index]; 
    }

    template <typename Self>
    inline decltype(auto) at(this Self&& self, size_t index) {
        return std::forward<Self>(self).elements_.at(index);
    }

    template <typename Self>
    inline decltype(auto) operator[](this Self&& self, size_t index) noexcept {
        return std::forward<Self>(self).elements_[index];
    }

    template <typename Self>
    inline decltype(auto) front(this Self&& self) noexcept {
        return std::forward<Self>(self).elements_.front();
    }

    template <typename Self>
    inline decltype(auto) back(this Self&& self) noexcept {
        return std::forward<Self>(self).elements_.back();
    }

    template <typename T>
    inline void set(size_t index, T&& value) noexcept {
        elements_[index] = std::forward<T>(value);
    }

    inline size_t size() const noexcept { return elements_.size(); }
    inline bool empty() const noexcept { return elements_.empty(); }
    inline size_t capacity() const noexcept { return elements_.capacity(); }

    template <typename T>
    inline void push(T&& value) { elements_.emplace_back(std::forward<T>(value)); }
    inline void pop() noexcept { elements_.pop_back(); }
    
    template <typename... Args>
    inline void emplace(Args&&... args) { elements_.emplace_back(std::forward<Args>(args)...); }
    
    inline void resize(size_t size) { elements_.resize(size); }
    inline void reserve(size_t capacity) { elements_.reserve(capacity); }
    inline void shrink() { elements_.shrink_to_fit(); }
    inline void clear() { elements_.clear(); }

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