#pragma once
#include "meow_arena.h"
#include <limits>
#include <new>

namespace meow {

template <typename T>
class allocator {
public:
    using value_type = T;

private:
    arena* source_;

public:
    explicit constexpr allocator(arena& source) noexcept : source_(&source) {}
    
    template <typename U>
    constexpr allocator(const allocator<U>& other) noexcept : source_(other.source()) {}

    [[nodiscard]] constexpr arena* source() const noexcept { return source_; }

    [[nodiscard]] [[gnu::always_inline]] T* allocate(std::size_t n) {
        return static_cast<T*>(source_->allocate(n * sizeof(T), alignof(T)));
    }

    [[gnu::always_inline]] void deallocate(T*, std::size_t) noexcept {}
    
    friend bool operator==(const allocator& a, const allocator& b) noexcept {
        return a.source_ == b.source_;
    }
};

}