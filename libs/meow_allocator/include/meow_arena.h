#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <algorithm>
#include <memory>

namespace meow {

class arena {
    struct block {
        std::byte* data;
        std::size_t size;
        std::size_t used;
        block* next;

        block(std::size_t s) : size(s), used(0), next(nullptr) {
            data = static_cast<std::byte*>(std::aligned_alloc(alignof(std::max_align_t), s));
            if (!data) throw std::bad_alloc();
        }

        ~block() {
            std::free(data);
        }
    };

    block* head_ = nullptr;
    block* current_ = nullptr;
    std::size_t default_block_size_;

public:
    explicit arena(std::size_t block_size = 4096) 
        : default_block_size_(block_size) {}

    ~arena() {
        clear();
    }

    arena(const arena&) = delete;
    arena& operator=(const arena&) = delete;
    
    arena(arena&& other) noexcept 
        : head_(other.head_), current_(other.current_), default_block_size_(other.default_block_size_) {
        other.head_ = nullptr;
        other.current_ = nullptr;
    }

    arena& operator=(arena&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_;
            current_ = other.current_;
            default_block_size_ = other.default_block_size_;
            other.head_ = nullptr;
            other.current_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t)) {
        std::size_t padding = 0;
        void* ptr = nullptr;
        std::size_t space = 0;

        if (current_) {
            ptr = current_->data + current_->used;
            space = current_->size - current_->used;
            if (std::align(align, bytes, ptr, space)) {
                std::size_t aligned_offset = static_cast<std::byte*>(ptr) - current_->data;
                current_->used = aligned_offset + bytes;
                return ptr;
            }
        }

        std::size_t next_size = std::max(default_block_size_, bytes + align);
        block* new_block = new block(next_size);
        
        if (current_) {
            current_->next = new_block;
        } else {
            head_ = new_block;
        }
        current_ = new_block;

        ptr = current_->data;
        space = current_->size;
        std::align(align, bytes, ptr, space);
        std::size_t aligned_offset = static_cast<std::byte*>(ptr) - current_->data;
        current_->used = aligned_offset + bytes;
        
        return ptr;
    }

    void deallocate(void*, std::size_t) noexcept {}

    void clear() {
        while (head_) {
            block* next = head_->next;
            delete head_;
            head_ = next;
        }
        current_ = nullptr;
    }

    void reset() {
        block* b = head_;
        while (b) {
            b->used = 0;
            b = b->next;
        }
        current_ = head_;
    }
    
    std::size_t total_capacity() const {
        std::size_t total = 0;
        for (block* b = head_; b; b = b->next) total += b->size;
        return total;
    }
};

}