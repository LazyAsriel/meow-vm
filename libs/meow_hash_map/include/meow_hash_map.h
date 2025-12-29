#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>
#include <bit>
#include <memory>
#include <new>
#include <utility>
#include <limits>
#include <concepts>
#include <iterator>

namespace meow {

namespace detail {
    static constexpr uint64_t LO_BITS = 0x0101010101010101ULL;
    static constexpr uint64_t HI_BITS = 0x8080808080808080ULL;
    static constexpr size_t GROUP_SIZE = 8;
    
    alignas(64) static constexpr int8_t EMPTY_GROUP[16] = {
        (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80,
        (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80,
        (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80,
        (int8_t)0x80, (int8_t)0x80, (int8_t)0x80, (int8_t)0x80
    };

    enum : int8_t {
        CTRL_EMPTY    = static_cast<int8_t>(0x80),
        CTRL_DELETED  = static_cast<int8_t>(0xFE),
        CTRL_SENTINEL = static_cast<int8_t>(0xFF)
    };

    struct Group {
        uint64_t val;
        
        [[gnu::always_inline]] inline static Group load(const int8_t* pos) noexcept {
            uint64_t v;
            std::memcpy(&v, pos, sizeof(uint64_t));
            return {v};
        }
        
        [[gnu::always_inline]] inline uint64_t match_empty() const noexcept {
            return val & HI_BITS;
        }
        
        [[gnu::always_inline]] inline uint64_t match(int8_t h2) const noexcept {
            uint64_t x = val ^ (static_cast<uint8_t>(h2) * LO_BITS);
            return (x - LO_BITS) & ~x & HI_BITS;
        }
    };

    [[gnu::always_inline]] inline int count_trailing_zeros(uint64_t mask) noexcept {
        if constexpr (std::endian::native == std::endian::little) return std::countr_zero(mask);
        else return std::countl_zero(mask);
    }

    template <typename T>
    struct FastHash {
        [[gnu::always_inline]] inline static constexpr uint64_t mix(uint64_t A, uint64_t B) noexcept {
            __uint128_t r = A; r *= B;
            return static_cast<uint64_t>(r) ^ static_cast<uint64_t>(r >> 64);
        }
        [[gnu::always_inline]] inline uint64_t operator()(const T& key) const noexcept {
            if constexpr (std::is_integral_v<T> || std::is_pointer_v<T>) {
                return mix(static_cast<uint64_t>(key) ^ 0x9E3779B97F4A7C15ULL, 0xBF58476D1CE4E5B9ULL);
            } else return std::hash<T>{}(key);
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
    using allocator_type = Allocator;

private:
    using AllocTraits = std::allocator_traits<Allocator>;
    using ByteAllocator = typename AllocTraits::template rebind_alloc<uint8_t>;
    using ByteAllocTraits = std::allocator_traits<ByteAllocator>;

    struct Layout {
        size_t ctrl_offset;
        size_t slots_offset;
        size_t total_bytes;
    };

    [[gnu::always_inline]] inline static Layout calc_layout(size_type mask) {
        size_type capacity = mask;
        size_type n = capacity + 1 + detail::GROUP_SIZE;        
        constexpr size_t CACHE_LINE = 64;
        constexpr size_t SLOT_ALIGN = (alignof(value_type) > CACHE_LINE) ? alignof(value_type) : CACHE_LINE;

        size_t slots_off = (n + SLOT_ALIGN - 1) & ~(SLOT_ALIGN - 1);
        size_t slots_sz = (capacity + 1) * sizeof(value_type);
        
        return {0, slots_off, slots_off + slots_sz};
    }

    int8_t* ctrl_ = const_cast<int8_t*>(detail::EMPTY_GROUP);
    value_type* slots_ = nullptr;
    uint8_t* raw_allocation_ = nullptr;
    
    size_type size_ = 0;
    size_type capacity_ = 0;

    [[no_unique_address]] Hash hasher_;
    [[no_unique_address]] KeyEqual equal_;
    [[no_unique_address]] ByteAllocator allocator_;

    [[gnu::always_inline]] inline static int8_t h2(uint64_t hash) noexcept {
        return static_cast<int8_t>(hash >> 57) & 0x7F;
    }

public:
    hash_map() noexcept(std::is_nothrow_default_constructible_v<ByteAllocator>) = default;

    explicit hash_map(size_type n, const Allocator& alloc = Allocator()) 
        : allocator_(alloc) {
        if (n > 0) reserve(n);
    }

    ~hash_map() { destroy_layout(); }

    template<typename K>
    [[gnu::hot]] [[gnu::always_inline]] inline
    auto find(this auto&& self, const K& key) noexcept -> decltype(&self.slots_[0].second) {
        if (self.capacity_ == 0) [[unlikely]] return nullptr;

        const uint64_t hash = self.hasher_(key);
        const int8_t target_h2 = h2(hash);
        const size_type mask = self.capacity_;
        size_type idx = hash & mask;

        while (true) {
            auto group = detail::Group::load(self.ctrl_ + idx);
            uint64_t match_mask = group.match(target_h2);

            while (match_mask) {
                int bit_idx = detail::count_trailing_zeros(match_mask);
                size_type actual_idx = (idx + (bit_idx >> 3)) & mask;
                
                if (self.equal_(self.slots_[actual_idx].first, key)) {
                    return &self.slots_[actual_idx].second;
                }
                match_mask &= (match_mask - 1);
            }
            
            if (group.match_empty() != 0) [[likely]] return nullptr;
            
            idx = (idx + detail::GROUP_SIZE) & mask;
        }
    }

    [[nodiscard]] [[gnu::always_inline]] inline bool contains(const Key& key) const noexcept { 
        return find(key) != nullptr; 
    }

    template<typename K, typename... Args>
    [[gnu::hot]] T& try_emplace(K&& key, Args&&... args) {
        if (size_ >= capacity_ * 7 / 8) [[unlikely]] {
            rehash(capacity_ == 0 ? 7 : (capacity_ * 2) + 1);
        }

        const uint64_t hash = hasher_(key);
        const int8_t target_h2 = h2(hash);
        const size_type mask = capacity_;
        
        size_type idx = hash & mask;
        size_type target_idx = size_type(-1); 

        while (true) {
            auto group = detail::Group::load(ctrl_ + idx);
            uint64_t match_mask = group.match(target_h2);

            while (match_mask) {
                size_type actual_idx = (idx + (detail::count_trailing_zeros(match_mask) >> 3)) & mask;
                if (equal_(slots_[actual_idx].first, key)) {
                    return slots_[actual_idx].second;
                }
                match_mask &= (match_mask - 1);
            }
            
            uint64_t empty_mask = group.match_empty();
            if (empty_mask != 0) [[likely]] {
                target_idx = (idx + (detail::count_trailing_zeros(empty_mask) >> 3)) & mask;
                break; 
            }
            idx = (idx + detail::GROUP_SIZE) & mask;
        }

        std::construct_at(&slots_[target_idx], 
            std::piecewise_construct,
            std::forward_as_tuple(std::forward<K>(key)),
            std::forward_as_tuple(std::forward<Args>(args)...));
            
        ctrl_[target_idx] = target_h2;
        size_++;
        return slots_[target_idx].second;
    }

    T& operator[](const Key& key) { return try_emplace(key); }
    T& operator[](Key&& key) { return try_emplace(std::move(key)); }

    void reserve(size_type n) {
        if (n <= size_) return;
        n = std::bit_ceil(n); 
        if (n < 8) n = 8;
        rehash(n - 1);
    }

    void clear() noexcept {
        if (size_ == 0) return;
        
        if constexpr (!std::is_trivially_destructible_v<value_type>) {
            for (size_type i = 0; i <= capacity_; ++i) {
                if (static_cast<uint8_t>(ctrl_[i]) < 0x80) {
                    std::destroy_at(&slots_[i]);
                }
            }
        }

        if (capacity_ > 0) {
            std::memset(ctrl_, detail::CTRL_EMPTY, capacity_ + 1 + detail::GROUP_SIZE);
        }
        size_ = 0;
    }

private:
    [[gnu::cold]] void destroy_layout() {
        if (!raw_allocation_) return;
        clear();
        Layout l = calc_layout(capacity_);
        ByteAllocTraits::deallocate(allocator_, raw_allocation_, l.total_bytes);
    }

    [[gnu::cold]] void rehash(size_type new_mask) {
        Layout l = calc_layout(new_mask);
        uint8_t* raw = ByteAllocTraits::allocate(allocator_, l.total_bytes);
        
        int8_t* new_ctrl = reinterpret_cast<int8_t*>(raw);
        value_type* new_slots = reinterpret_cast<value_type*>(raw + l.slots_offset);

        std::memset(new_ctrl, detail::CTRL_EMPTY, new_mask + 1 + detail::GROUP_SIZE);

        int8_t* old_ctrl = ctrl_;
        value_type* old_slots = slots_;
        uint8_t* old_raw = raw_allocation_;
        size_type old_cap = capacity_;

        ctrl_ = new_ctrl;
        slots_ = new_slots;
        raw_allocation_ = raw;
        capacity_ = new_mask;

        if (old_raw) {
            for (size_type i = 0; i <= old_cap; ++i) {
                if (static_cast<uint8_t>(old_ctrl[i]) < 0x80) {
                    auto& key = old_slots[i].first;
                    uint64_t hash = hasher_(key);
                    int8_t target_h2 = h2(hash);
                    size_type idx = hash & new_mask;

                    while (true) {
                        auto group = detail::Group::load(new_ctrl + idx);
                        uint64_t empty_mask = group.match_empty();
                        if (empty_mask) {
                            size_type target_idx = (idx + (detail::count_trailing_zeros(empty_mask) >> 3)) & new_mask;
                            
                            std::construct_at(&new_slots[target_idx], std::move(old_slots[i]));
                            new_ctrl[target_idx] = target_h2;
                            break;
                        }
                        idx = (idx + detail::GROUP_SIZE) & new_mask;
                    }

                    if constexpr (!std::is_trivially_destructible_v<value_type>) {
                        std::destroy_at(&old_slots[i]);
                    }
                }
            }
            
            Layout old_l = calc_layout(old_cap);
            ByteAllocTraits::deallocate(allocator_, old_raw, old_l.total_bytes);
        }
        
        std::memcpy(ctrl_ + (new_mask + 1), ctrl_, detail::GROUP_SIZE);
    }
};

} // namespace meow