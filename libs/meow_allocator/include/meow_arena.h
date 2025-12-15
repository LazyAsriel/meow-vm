#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <algorithm>
#include <cstdint>
#include <bit>
#include <stdexcept>

namespace meow {

class arena {
    struct block_header {
        block_header* next;
    };

    std::uintptr_t ptr_ = 0;
    std::uintptr_t end_ = 0;
    block_header* head_ = nullptr;
    std::size_t default_block_size_;

public:
    explicit arena(std::size_t block_size = 64 * 1024)
        : default_block_size_(block_size) {}

    ~arena() { clear(); }

    arena(const arena&) = delete;
    arena& operator=(const arena&) = delete;
    
    arena(arena&& other) noexcept 
        : ptr_(other.ptr_), end_(other.end_), head_(other.head_), default_block_size_(other.default_block_size_) {
        other.ptr_ = 0;
        other.end_ = 0;
        other.head_ = nullptr;
    }

    [[nodiscard]] __attribute__((always_inline)) void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t)) {
        [[assume((align & (align - 1)) == 0)]];

        std::uintptr_t aligned_ptr = (ptr_ + (align - 1)) & ~(align - 1);
        std::uintptr_t new_ptr = aligned_ptr + bytes;

        if (new_ptr <= end_) [[likely]] {
            ptr_ = new_ptr;
            return reinterpret_cast<void*>(aligned_ptr);
        }

        return allocate_slow(bytes, align);
    }

    [[gnu::noinline]] void* allocate_slow(std::size_t bytes, std::size_t align) {
        std::size_t alloc_size = std::max(default_block_size_, bytes + sizeof(block_header) + align);
        
        void* mem = std::aligned_alloc(alignof(std::max_align_t), alloc_size);
        if (!mem) [[unlikely]] throw std::bad_alloc();

        auto* new_block = static_cast<block_header*>(mem);
        new_block->next = head_;
        head_ = new_block;

        std::uintptr_t start_data = reinterpret_cast<std::uintptr_t>(new_block + 1);
        std::uintptr_t align_mask = align - 1;
        std::uintptr_t aligned_data = (start_data + align_mask) & ~align_mask;

        ptr_ = aligned_data + bytes;
        end_ = reinterpret_cast<std::uintptr_t>(mem) + alloc_size;

        return reinterpret_cast<void*>(aligned_data);
    }

    __attribute__((always_inline)) void deallocate(void*, std::size_t) noexcept {}

    void clear() {
        while (head_) {
            block_header* next = head_->next;
            std::free(head_);
            head_ = next;
        }
        ptr_ = 0;
        end_ = 0;
        head_ = nullptr;
    }
    
    void reset() {
        if (!head_) return;
        
        block_header* first = head_; 
        
        std::uintptr_t start_data = reinterpret_cast<std::uintptr_t>(first + 1);
        std::uintptr_t align_mask = alignof(std::max_align_t) - 1;
        std::uintptr_t aligned_data = (start_data + align_mask) & ~align_mask;
        
        ptr_ = aligned_data;
    }
};

}