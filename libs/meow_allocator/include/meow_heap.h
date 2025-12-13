#pragma once

#include "meow_arena.h"
#include <memory>
#include <bit>
#include <array>
#include <cassert>
#include <new>
#include <cstdlib>

namespace meow {

class heap {
private:
    static constexpr size_t MIN_ALIGN_SHIFT = 3;
    static constexpr size_t MIN_ALIGN = 1 << MIN_ALIGN_SHIFT;
    static constexpr size_t MAX_SMALL_OBJ_SIZE = 256;
    static constexpr size_t NUM_BINS = MAX_SMALL_OBJ_SIZE >> MIN_ALIGN_SHIFT;

    struct FreeNode {
        FreeNode* next;
    };

    meow::arena& arena_;
    
    alignas(64) FreeNode* free_bins_[NUM_BINS] = {nullptr};

    static constexpr size_t get_bin_index(size_t size) {
        return (size + MIN_ALIGN - 1) >> MIN_ALIGN_SHIFT;
    }

public:
    explicit heap(meow::arena& a) noexcept : arena_(a) {}

    heap(const heap&) = delete;
    heap& operator=(const heap&) = delete;

    [[nodiscard]] __attribute__((always_inline)) void* allocate_raw(size_t size) {
        [[assume(size > 0)]];

        if (size > MAX_SMALL_OBJ_SIZE) [[unlikely]] {
            return std::malloc(size);
        }

        size_t bin_idx = get_bin_index(size);
        if (bin_idx > 0) bin_idx--;

        // Tái sử dụng block cũ từ Free List
        if (FreeNode* node = free_bins_[bin_idx]) {
            free_bins_[bin_idx] = node->next;
            return node;
        }

        size_t alloc_size = (bin_idx + 1) << MIN_ALIGN_SHIFT;
        return arena_.allocate(alloc_size, MIN_ALIGN);
    }

    template <typename T>
    [[nodiscard]] __attribute__((always_inline)) void* fast_alloc_raw() {
        constexpr size_t size = sizeof(T);
        
        // [OPTIMIZE] Compile-time check cho struct lớn
        if constexpr (size > MAX_SMALL_OBJ_SIZE) {
            return std::malloc(size);
        } else {
            constexpr size_t aligned_size = (size < MIN_ALIGN) ? MIN_ALIGN : size;
            constexpr size_t idx = get_bin_index(aligned_size) - 1;
            
            if (FreeNode* node = free_bins_[idx]) {
                free_bins_[idx] = node->next;
                return node;
            }
            
            constexpr size_t alloc_sz = (idx + 1) << MIN_ALIGN_SHIFT;
            return arena_.allocate(alloc_sz, MIN_ALIGN);
        }
    }

    __attribute__((always_inline)) void deallocate_raw(void* ptr, size_t size) {
        if (!ptr) return;

        // [OPTIMIZE] Trả lại System Heap
        if (size > MAX_SMALL_OBJ_SIZE) [[unlikely]] {
            std::free(ptr);
            return;
        }

        // Trả lại Free List của Arena
        size_t bin_idx = get_bin_index(size);
        if (bin_idx > 0) bin_idx--;

        auto* node = reinterpret_cast<FreeNode*>(ptr);
        node->next = free_bins_[bin_idx];
        free_bins_[bin_idx] = node;
    }

    // --- Object Creation ---

    template <typename T, typename... Args>
    [[nodiscard]] __attribute__((always_inline)) T* create(Args&&... args) {
        void* ptr = fast_alloc_raw<T>();
        return ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    [[nodiscard]] __attribute__((always_inline)) T* create_varsize(size_t extra_bytes, Args&&... args) {
        size_t total_size = sizeof(T) + extra_bytes;
        void* ptr = allocate_raw(total_size);
        return ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
    }

    // --- Object Destruction ---

    template <typename T>
    __attribute__((always_inline)) void destroy(T* obj) {
        if (!obj) return;
        std::destroy_at(obj);
        deallocate_raw(obj, sizeof(T));
    }

    template <typename T>
    __attribute__((always_inline)) void destroy_dynamic(T* obj, size_t actual_size) {
        if (!obj) return;
        std::destroy_at(obj);
        deallocate_raw(obj, actual_size);
    }
};

}