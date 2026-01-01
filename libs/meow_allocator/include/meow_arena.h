#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <algorithm>
#include <cstdint>
#include <bit>

namespace meow {

class arena {
public:
    struct BlockHeader {
        BlockHeader* next;
        std::uintptr_t data_start;
        std::uintptr_t data_end;
    };

private:
    std::uintptr_t ptr_ = 0;
    std::uintptr_t end_ = 0;
    
    BlockHeader* head_ = nullptr;    // Block đầu tiên (gốc)
    BlockHeader* current_ = nullptr; // Block đang ghi
    
    std::size_t default_block_size_;

public:
    explicit arena(std::size_t block_size = 64 * 1024) noexcept
        : default_block_size_(block_size) {}

    ~arena() { clear(); }

    arena(const arena&) = delete;
    arena& operator=(const arena&) = delete;

    arena(arena&& other) noexcept 
        : ptr_(other.ptr_), end_(other.end_), 
          head_(other.head_), current_(other.current_),
          default_block_size_(other.default_block_size_) {
        other.ptr_ = 0; other.end_ = 0;
        other.head_ = nullptr; other.current_ = nullptr;
    }

    // --- HOT PATH: Inline tối đa ---
    [[nodiscard]] [[gnu::always_inline]] void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t)) {
        // C++23 hint: Giả định alignment luôn là lũy thừa của 2
        [[assume((align & (align - 1)) == 0)]];

        std::uintptr_t aligned_ptr = (ptr_ + (align - 1)) & ~(align - 1);
        std::uintptr_t new_ptr = aligned_ptr + bytes;

        // Branch prediction: Khả năng cao là còn chỗ
        if (new_ptr <= end_) [[likely]] {
            ptr_ = new_ptr;
            return reinterpret_cast<void*>(aligned_ptr);
        }

        return allocate_slow(bytes, align);
    }

    // Reset cực nhanh: Chỉ tua lại con trỏ, không free bộ nhớ
    [[gnu::always_inline]] void reset() {
        current_ = head_;
        if (current_) [[likely]] {
            ptr_ = current_->data_start;
            end_ = current_->data_end;
        } else {
            ptr_ = 0;
            end_ = 0;
        }
    }

    // Xóa thật sự (dùng khi hủy level/scene)
    void clear() {
        while (head_) {
            BlockHeader* next = head_->next;
            std::free(head_);
            head_ = next;
        }
        ptr_ = 0; end_ = 0;
        head_ = nullptr; current_ = nullptr;
    }

private:
    // Đẩy logic chậm ra khỏi instruction cache của hot path
    [[gnu::noinline]] void* allocate_slow(std::size_t bytes, std::size_t align) {
        // 1. Chiến thuật Tái sử dụng (Reuse Strategy)
        // Nếu block hiện tại có block kế tiếp (đã alloc từ frame trước), thử dùng nó
        if (current_ && current_->next) {
            BlockHeader* next_block = current_->next;
            
            // Tính toán alignment cho block kế tiếp
            std::uintptr_t start = next_block->data_start;
            std::uintptr_t align_mask = align - 1;
            std::uintptr_t aligned_ptr = (start + align_mask) & ~align_mask;
            
            // Kiểm tra xem block cũ này có đủ lớn cho yêu cầu hiện tại không
            if (aligned_ptr + bytes <= next_block->data_end) {
                current_ = next_block; // Chuyển sang block kế
                ptr_ = aligned_ptr + bytes;
                end_ = next_block->data_end;
                return reinterpret_cast<void*>(aligned_ptr);
            }
            // Nếu block kế tiếp quá nhỏ (edge case), ta buộc phải alloc mới và chèn vào giữa
            // (Code dưới sẽ xử lý việc alloc mới)
        }

        // 2. Alloc mới từ OS (Trường hợp lần đầu chạy hoặc hết block tái sử dụng)
        std::size_t header_size = sizeof(BlockHeader);
        // Padding header để đảm bảo data bắt đầu ở max_align
        std::size_t header_align_pad = (alignof(std::max_align_t) - (header_size & (alignof(std::max_align_t) - 1))) & (alignof(std::max_align_t) - 1);
        std::size_t actual_data_offset = header_size + header_align_pad;

        std::size_t alloc_size = std::max(default_block_size_, bytes + actual_data_offset + align);
        
        void* mem = std::aligned_alloc(alignof(std::max_align_t), alloc_size);
        // if (!mem) [[unlikely]] throw std::bad_alloc();
        if (!mem) [[unlikely]] std::abort();

        auto* new_block = static_cast<BlockHeader*>(mem);
        
        // Thiết lập thông số block
        new_block->data_start = reinterpret_cast<std::uintptr_t>(mem) + actual_data_offset;
        new_block->data_end = reinterpret_cast<std::uintptr_t>(mem) + alloc_size;
        
        // Link vào danh sách
        if (current_) {
            // Chèn vào sau current (giữ chuỗi liên kết nếu có)
            new_block->next = current_->next;
            current_->next = new_block;
            current_ = new_block;
        } else {
            // Block đầu tiên
            new_block->next = nullptr;
            head_ = current_ = new_block;
        }

        // Tính toán trả về pointer
        std::uintptr_t align_mask = align - 1;
        std::uintptr_t aligned_res = (new_block->data_start + align_mask) & ~align_mask;
        
        ptr_ = aligned_res + bytes;
        end_ = new_block->data_end;

        return reinterpret_cast<void*>(aligned_res);
    }
};

}