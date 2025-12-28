#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <bit>
#include <cstring>
#include <new>
#include <limits>
#include <memory>
#include <cmath>

namespace meow {

namespace detail {
    template <typename T>
    struct FastHash {
        [[nodiscard]] static constexpr uint64_t mix(uint64_t v) noexcept {
            v ^= v >> 33; v *= 0xff51afd7ed558ccdULL;
            v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ULL;
            v ^= v >> 33; return v;
        }
        [[nodiscard]] uint64_t operator()(const T& key) const noexcept {
            if constexpr (std::is_integral_v<T> || std::is_pointer_v<T>) return mix(static_cast<uint64_t>(key));
            else return mix(std::hash<T>{}(key));
        }
    };
}

template<
    typename Key,
    typename T,
    typename Hash = detail::FastHash<Key>,
    typename KeyEqual = std::equal_to<Key>,
    typename Allocator = std::allocator<std::pair<Key, T>>
>
class hash_map {
public:
    using value_type = std::pair<Key, T>;
    using size_type = std::size_t;

private:
    struct Metadata {
        uint64_t hash;
        int16_t psl;
    };

    using AllocTraits = std::allocator_traits<Allocator>;
    using ByteAlloc = typename AllocTraits::template rebind_alloc<std::byte>;

    Metadata* meta_ = nullptr;
    value_type* slots_ = nullptr;
    
    size_type mask_ = 0;
    size_type size_ = 0;
    size_type capacity_ = 0;
    int16_t max_psl_ = 0;
    
    [[no_unique_address]] Hash hasher_;
    [[no_unique_address]] KeyEqual equal_;
    [[no_unique_address]] ByteAlloc allocator_;

public:
    hash_map() noexcept = default;

    explicit hash_map(size_type n, const Allocator& alloc = Allocator()) noexcept 
        : allocator_(alloc) {
        if (n > 0) reserve(n);
    }

    ~hash_map() noexcept {
        if (capacity_ > 0) deallocate_layout(meta_, slots_, capacity_);
    }

    hash_map(hash_map&& other) noexcept 
        : meta_(other.meta_), slots_(other.slots_),
          mask_(other.mask_), size_(other.size_), capacity_(other.capacity_),
          max_psl_(other.max_psl_), allocator_(std::move(other.allocator_)) {
        other.meta_ = nullptr; other.slots_ = nullptr;
        other.size_ = 0; other.capacity_ = 0;
    }

    hash_map& operator=(hash_map&& other) noexcept {
        if (this == &other) return *this;
        if (capacity_ > 0) deallocate_layout(meta_, slots_, capacity_);
        
        meta_ = other.meta_; slots_ = other.slots_;
        mask_ = other.mask_; size_ = other.size_;
        capacity_ = other.capacity_; max_psl_ = other.max_psl_;
        allocator_ = std::move(other.allocator_);
        
        other.meta_ = nullptr; other.slots_ = nullptr;
        other.size_ = 0; other.capacity_ = 0;
        return *this;
    }

    // --- LOOKUP ---

    [[nodiscard]] __attribute__((always_inline)) 
    T* find(const Key& key) noexcept {
        if (size_ == 0) [[unlikely]] return nullptr;

        const uint64_t h = hasher_(key);
        size_type idx = h & mask_;
        int16_t dist = 0;
        
        const Metadata* __restrict local_meta = meta_;

        while (true) {
            const auto& m = local_meta[idx];
            if (dist > m.psl) return nullptr;
            if (m.hash == h) {
                if (equal_(slots_[idx].first, key)) {
                    return &slots_[idx].second;
                }
            }
            idx = (idx + 1) & mask_;
            dist++;
        }
    }

    [[nodiscard]] bool contains(const Key& key) const noexcept {
        return const_cast<hash_map*>(this)->find(key) != nullptr;
    }

    T& operator[](Key&& key) noexcept {
        return *try_emplace_impl(std::move(key));
    }
    
    T& operator[](const Key& key) noexcept {
        return *try_emplace_impl(key);
    }

    template <typename... Args>
    std::pair<T*, bool> try_emplace(Key&& key, Args&&... args) noexcept {
        T* ptr = try_emplace_impl(std::move(key), std::forward<Args>(args)...);
        return {ptr, true};
    }
    
    template <typename... Args>
    std::pair<T*, bool> try_emplace(const Key& key, Args&&... args) noexcept {
        T* ptr = try_emplace_impl(key, std::forward<Args>(args)...);
        return {ptr, true};
    }

    void reserve(size_type n) noexcept {
        if (n == 0) return;
        size_type new_cap = std::bit_ceil(static_cast<size_type>(n * 1.1f)); 
        if (new_cap < 16) new_cap = 16;
        if (new_cap > capacity_) rehash(new_cap);
    }

    void clear() noexcept {
        if (size_ == 0) return;
        for (size_type i = 0; i < capacity_; ++i) {
            if (meta_[i].psl >= 0) {
                meta_[i].psl = -1;
                slots_[i].~value_type();
            }
        }
        size_ = 0; max_psl_ = 0;
    }

private:
    void allocate_layout(size_type cap, Metadata*& out_meta, value_type*& out_slots) noexcept {
        size_type meta_bytes = cap * sizeof(Metadata);
        size_type slot_bytes = cap * sizeof(value_type);
        size_type align_offset = (alignof(value_type) - (meta_bytes % alignof(value_type))) % alignof(value_type);
        
        size_type total_bytes = meta_bytes + align_offset + slot_bytes;
        
        std::byte* raw = allocator_.allocate(total_bytes); // Nếu fail -> terminate
        
        out_meta = reinterpret_cast<Metadata*>(raw);
        out_slots = reinterpret_cast<value_type*>(raw + meta_bytes + align_offset);

        for(size_type i=0; i<cap; ++i) out_meta[i].psl = -1;
    }

    void deallocate_layout(Metadata* m, value_type* s, size_type cap) noexcept {
        for (size_type i = 0; i < cap; ++i) {
            if (m[i].psl >= 0) s[i].~value_type();
        }
        
        size_type meta_bytes = cap * sizeof(Metadata);
        size_type slot_bytes = cap * sizeof(value_type);
        size_type align_offset = (alignof(value_type) - (meta_bytes % alignof(value_type))) % alignof(value_type);
        
        allocator_.deallocate(reinterpret_cast<std::byte*>(m), meta_bytes + align_offset + slot_bytes);
    }

    void rehash(size_type new_cap) noexcept {
        Metadata* new_meta = nullptr;
        value_type* new_slots = nullptr;
        
        allocate_layout(new_cap, new_meta, new_slots);

        Metadata* old_meta = meta_;
        value_type* old_slots = slots_;
        size_type old_cap = capacity_;

        meta_ = new_meta;
        slots_ = new_slots;
        capacity_ = new_cap;
        mask_ = new_cap - 1;
        size_ = 0;
        max_psl_ = 0;

        if (old_meta) {
            for (size_type i = 0; i < old_cap; ++i) {
                if (old_meta[i].psl >= 0) {
                    insert_move_on_rehash(std::move(old_slots[i]), old_meta[i].hash);
                }
            }
            for(size_type i=0; i<old_cap; ++i) old_meta[i].psl = -1;
            
            size_type meta_bytes = old_cap * sizeof(Metadata);
            size_type align_offset = (alignof(value_type) - (meta_bytes % alignof(value_type))) % alignof(value_type);
            allocator_.deallocate(reinterpret_cast<std::byte*>(old_meta), 
                                  meta_bytes + align_offset + old_cap * sizeof(value_type));
        }
    }

    void insert_move_on_rehash(value_type&& val, uint64_t h) noexcept {
        size_type idx = h & mask_;
        int16_t curr_psl = 0;
        
        value_type curr_val = std::move(val);
        uint64_t curr_h = h;

        while (true) {
            if (meta_[idx].psl == -1) {
                new (&slots_[idx]) value_type(std::move(curr_val));
                meta_[idx].hash = curr_h;
                meta_[idx].psl = curr_psl;
                size_++;
                if (curr_psl > max_psl_) max_psl_ = curr_psl;
                return;
            }

            if (curr_psl > meta_[idx].psl) {
                std::swap(curr_val, slots_[idx]);
                std::swap(curr_h, meta_[idx].hash);
                std::swap(curr_psl, meta_[idx].psl);
                if (curr_psl > max_psl_) max_psl_ = curr_psl;
            }
            idx = (idx + 1) & mask_;
            curr_psl++;
        }
    }

    template <typename K, typename... Args>
    T* try_emplace_impl(K&& key, Args&&... args) noexcept {
        if (size_ + 1 > capacity_ * 0.9) [[unlikely]] {
            rehash(capacity_ == 0 ? 16 : capacity_ * 2);
        }

        uint64_t h = hasher_(key);
        size_type idx = h & mask_;
        int16_t dist = 0;

        while (true) {
            if (dist > meta_[idx].psl) break; 
            if (meta_[idx].hash == h && equal_(slots_[idx].first, key)) {
                return &slots_[idx].second;
            }
            idx = (idx + 1) & mask_;
            dist++;
        }

        idx = h & mask_;
        int16_t curr_psl = 0;
        uint64_t curr_h = h;

        alignas(value_type) std::byte temp_storage[sizeof(value_type)];
        value_type* curr_val_ptr = reinterpret_cast<value_type*>(temp_storage);
        
        new (curr_val_ptr) value_type(std::piecewise_construct,
                                     std::forward_as_tuple(std::forward<K>(key)),
                                     std::forward_as_tuple(std::forward<Args>(args)...));

        T* result_ptr = nullptr;
        bool is_first = true;

        while (true) {
            if (meta_[idx].psl == -1) {
                new (&slots_[idx]) value_type(std::move(*curr_val_ptr));
                meta_[idx].hash = curr_h;
                meta_[idx].psl = curr_psl;
                
                size_++;
                if (curr_psl > max_psl_) max_psl_ = curr_psl;
                
                if (is_first) result_ptr = &slots_[idx].second;
                curr_val_ptr->~value_type();
                return is_first ? result_ptr : &slots_[idx].second;
            }

            if (curr_psl > meta_[idx].psl) {
                std::swap(curr_h, meta_[idx].hash);
                std::swap(curr_psl, meta_[idx].psl);
                std::swap(*curr_val_ptr, slots_[idx]);
                
                if (is_first) {
                    result_ptr = &slots_[idx].second;
                    is_first = false;
                }
                
                if (curr_psl > max_psl_) max_psl_ = curr_psl;
            }
            idx = (idx + 1) & mask_;
            curr_psl++;
        }
    }
};

} // namespace meow