#pragma once

#include "meow_arena.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <memory>
#include <limits>
#include <new>
#include <utility>
#include <type_traits>
#include <cassert>

namespace meow {
template <typename T>
class allocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
private:
    arena* source_;
public:
    explicit constexpr allocator(arena& source) noexcept : source_(&source) {}
    
    template <typename U>
    constexpr allocator(const allocator<U>& other) noexcept : source_(other.source()) {}

    [[nodiscard]] constexpr arena* source() const noexcept { return source_; }

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();
        
        return static_cast<T*>(source_->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept {
        source_->deallocate(p, n * sizeof(T));
    }
    
    friend bool operator==(const allocator& a, const allocator& b) noexcept {
        return a.source_ == b.source_;
    }
};

}