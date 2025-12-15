#pragma once

#include "meow_arena.h"
#include <memory>
#include <bit>
#include <array>
#include <new>
#include <cstdlib>

namespace meow {

struct alignas(std::max_align_t) ObjectMeta {
    union {
        ObjectMeta* next_gc;
        ObjectMeta* next_free;
    };
    uint32_t size;
    uint32_t flags;
};

class heap {
private:
    static constexpr size_t MIN_ALIGN_SHIFT = 4;
    static constexpr size_t MIN_ALIGN = 1 << MIN_ALIGN_SHIFT;
    static constexpr size_t MAX_SMALL_OBJ_SIZE = 256;
    static constexpr size_t META_SIZE = sizeof(ObjectMeta);
    
    static constexpr size_t NUM_BINS = (MAX_SMALL_OBJ_SIZE + META_SIZE + MIN_ALIGN - 1) >> MIN_ALIGN_SHIFT;

    meow::arena& arena_;

    alignas(64) ObjectMeta* free_bins_[NUM_BINS] = {nullptr};

    static constexpr size_t get_bin_index(size_t total_size) {
        return (total_size + MIN_ALIGN - 1) >> MIN_ALIGN_SHIFT;
    }

    [[nodiscard]] __attribute__((always_inline)) void* allocate_impl(size_t total_size) {
        if (total_size > MAX_SMALL_OBJ_SIZE + META_SIZE) [[unlikely]] {
             return std::aligned_alloc(alignof(std::max_align_t), total_size);
        }

        const size_t bin_idx = get_bin_index(total_size) - 1;

        if (ObjectMeta* node = free_bins_[bin_idx]) {
            if (node->next_free) {
                __builtin_prefetch(node->next_free, 1, 3);
            }

            free_bins_[bin_idx] = node->next_free;
            
            node->next_gc = nullptr; 
            return node;
        }

        size_t alloc_size = (bin_idx + 1) << MIN_ALIGN_SHIFT;
        return arena_.allocate(alloc_size, MIN_ALIGN);
    }

public:
    explicit heap(meow::arena& a) noexcept : arena_(a) {}
    
    heap(const heap&) = delete;
    heap& operator=(const heap&) = delete;

    [[nodiscard]] static ObjectMeta* get_meta(void* obj_data) {
        return reinterpret_cast<ObjectMeta*>(static_cast<char*>(obj_data) - META_SIZE);
    }

    [[nodiscard]] static void* get_data(ObjectMeta* meta) {
        return reinterpret_cast<void*>(reinterpret_cast<char*>(meta) + META_SIZE);
    }

    template <typename T, typename... Args>
    [[nodiscard]] __attribute__((always_inline)) T* create(Args&&... args) {
        constexpr size_t data_size = sizeof(T);
        constexpr size_t total_size = META_SIZE + data_size;

        void* raw_block = allocate_impl(total_size);
        
        auto* meta = static_cast<ObjectMeta*>(raw_block);
        
        meta->next_gc = nullptr; 
        meta->size = static_cast<uint32_t>(data_size);
        meta->flags = 0;

        void* data_ptr = get_data(meta);
        return ::new (data_ptr) T(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    [[nodiscard]] __attribute__((always_inline)) T* create_varsize(size_t extra_bytes, Args&&... args) {
        size_t data_size = sizeof(T) + extra_bytes;
        size_t total_size = META_SIZE + data_size;
        
        void* raw_block = allocate_impl(total_size);

        auto* meta = static_cast<ObjectMeta*>(raw_block);
        meta->next_gc = nullptr;
        meta->size = static_cast<uint32_t>(data_size);
        meta->flags = 0;

        void* data_ptr = get_data(meta);
        return ::new (data_ptr) T(std::forward<Args>(args)...);
    }

    template <typename T>
    __attribute__((always_inline)) void destroy(T* obj) {
        if (!obj) return;
        
        std::destroy_at(obj);

        ObjectMeta* meta = get_meta(obj);
        size_t total_size = META_SIZE + meta->size;
        
        deallocate_raw(meta, total_size);
    }

    __attribute__((always_inline)) void deallocate_raw(ObjectMeta* meta, size_t total_size) {
        if (total_size > MAX_SMALL_OBJ_SIZE + META_SIZE) [[unlikely]] {
            std::free(meta); // Trả OS
            return;
        }

        const size_t bin_idx = get_bin_index(total_size) - 1;

        meta->next_free = free_bins_[bin_idx];
        free_bins_[bin_idx] = meta;
    }
};

}