#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <algorithm>
#include <cstdint>
#include <bit>

namespace meow {

class arena {
    struct block_header {
        block_header* next;
    };

    char* ptr_ = nullptr;
    char* end_ = nullptr;
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
        other.ptr_ = other.end_ = nullptr;
        other.head_ = nullptr;
    }

    [[nodiscard]] __attribute__((always_inline)) void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t)) {
        [[assume((align & (align - 1)) == 0)]];

        std::uintptr_t current_addr = reinterpret_cast<std::uintptr_t>(ptr_);
        std::uintptr_t align_mask = align - 1;
        std::uintptr_t aligned_addr = (current_addr + align_mask) & ~align_mask;
        
        char* result = reinterpret_cast<char*>(aligned_addr);
        char* new_ptr = result + bytes;

        if (new_ptr <= end_) [[likely]] {
            ptr_ = new_ptr;
            return result;
        }

        return allocate_slow(bytes, align);
    }

    [[gnu::noinline]] void* allocate_slow(std::size_t bytes, std::size_t align) {
        std::size_t size = std::max(default_block_size_, bytes + sizeof(block_header) + align);
        
        void* mem = std::aligned_alloc(alignof(std::max_align_t), size);
        if (!mem) [[unlikely]] throw std::bad_alloc();

        block_header* new_block = static_cast<block_header*>(mem);
        new_block->next = head_;
        head_ = new_block;

        std::uintptr_t start_data = reinterpret_cast<std::uintptr_t>(new_block + 1);
        std::uintptr_t align_mask = align - 1;
        std::uintptr_t aligned_data = (start_data + align_mask) & ~align_mask;

        ptr_ = reinterpret_cast<char*>(aligned_data) + bytes;
        end_ = reinterpret_cast<char*>(mem) + size;

        return reinterpret_cast<void*>(aligned_data);
    }

    __attribute__((always_inline)) void deallocate(void*, std::size_t) noexcept {}

    void reset() {
        if (!head_) return;
        block_header* current_block = head_;
        std::uintptr_t start_data = reinterpret_cast<std::uintptr_t>(current_block + 1);
        std::uintptr_t align_mask = alignof(std::max_align_t) - 1;
        std::uintptr_t aligned_data = (start_data + align_mask) & ~align_mask;
        
        ptr_ = reinterpret_cast<char*>(aligned_data);
    }

    void clear() {
        while (head_) {
            block_header* next = head_->next;
            std::free(head_);
            head_ = next;
        }
        ptr_ = end_ = nullptr;
    }
};

}