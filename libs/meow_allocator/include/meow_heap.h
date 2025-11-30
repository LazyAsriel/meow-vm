#pragma once

#include "meow_allocator.h"
#include <vector>
#include <bit>
#include <algorithm>
#include <concepts>

namespace meow {

class heap {
private:
    static constexpr size_t MIN_ALIGN = 16;
    static constexpr size_t MAX_SMALL_OBJ_SIZE = 256;
    static constexpr size_t NUM_BINS = MAX_SMALL_OBJ_SIZE / MIN_ALIGN;

    meow::arena& arena_;

    using BinType = std::vector<void*, meow::allocator<void*>>;
    std::vector<BinType, meow::allocator<BinType>> free_bins_;
public:
    explicit heap(meow::arena& a) 
        : arena_(a), 
          free_bins_(NUM_BINS, BinType(meow::allocator<void*>(a)), meow::allocator<BinType>(a)) 
    {}

    heap(const heap&) = delete;
    heap& operator=(const heap&) = delete;

    [[nodiscard]] void* allocate_raw(size_t size) {
        if (size > MAX_SMALL_OBJ_SIZE) {
            return arena_.allocate(size);
        }
        size_t aligned_size = (size + MIN_ALIGN - 1) / MIN_ALIGN * MIN_ALIGN;
        size_t bin_idx = (aligned_size / MIN_ALIGN) - 1;

        if (!free_bins_[bin_idx].empty()) {
            void* ptr = free_bins_[bin_idx].back();
            free_bins_[bin_idx].pop_back();
            return ptr;
        }

        return arena_.allocate(aligned_size);
    }

    void deallocate_raw(void* ptr, size_t size) {
        if (!ptr) return;
        if (size > MAX_SMALL_OBJ_SIZE) return;
        size_t aligned_size = (size + MIN_ALIGN - 1) / MIN_ALIGN * MIN_ALIGN;
        size_t bin_idx = (aligned_size / MIN_ALIGN) - 1;
        free_bins_[bin_idx].push_back(ptr);
    }

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate_raw(sizeof(T));
        return std::construct_at(static_cast<T*>(ptr), std::forward<Args>(args)...);
    }

    template <typename T>
    void destroy(T* obj) {
        if (!obj) return;
        size_t s = sizeof(T);
        std::destroy_at(obj);
        deallocate_raw(obj, s);
    }
    
    void destroy_sized(void* obj, size_t size, void (*destructor_fn)(void*)) {
        if (!obj) return;
        if (destructor_fn) destructor_fn(obj);
        deallocate_raw(obj, size);
    }
};

}