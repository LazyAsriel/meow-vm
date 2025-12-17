#pragma once

#include <vector>
#include <algorithm>
#include <type_traits>
#include <concepts>
#include <utility>
#include <ranges>
#include <stdexcept>
#include <cstring> 

#if defined(__GNUC__) || defined(__clang__)
    #define MEOW_ALWAYS_INLINE __attribute__((always_inline)) inline
    #define MEOW_FLATTEN __attribute__((flatten))
#elif defined(_MSC_VER)
    #define MEOW_ALWAYS_INLINE __forceinline
    #define MEOW_FLATTEN
#else
    #define MEOW_ALWAYS_INLINE inline
    #define MEOW_FLATTEN
#endif

namespace meow {

template <typename T>
concept FastScalarKey = std::is_scalar_v<T> && (sizeof(T) <= sizeof(void*));

template<typename Key, typename T, 
         typename Compare = std::less<Key>, 
         typename KeyContainer = std::vector<Key>,
         typename ValueContainer = std::vector<T>>
class flat_map {
public:
    using size_type = std::size_t;
    static constexpr size_type LINEAR_SEARCH_THRESHOLD = 64;
    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    KeyContainer keys_;
    ValueContainer values_;
    [[no_unique_address]] Compare comp_;

    MEOW_ALWAYS_INLINE void grow_if_needed(size_type n = 1) {
        if (keys_.size() + n > keys_.capacity()) [[unlikely]] {
            size_type new_cap = keys_.capacity() ? keys_.capacity() * 2 : 8;
            if (new_cap < keys_.size() + n) new_cap = keys_.size() + n;
            keys_.reserve(new_cap);
            values_.reserve(new_cap);
        }
    }

    template <typename K>
    [[nodiscard]] MEOW_ALWAYS_INLINE MEOW_FLATTEN 
    size_type find_impl(const K& key) const noexcept {
        const size_type sz = keys_.size();
        const Key* __restrict k_ptr = keys_.data(); 

        if constexpr (FastScalarKey<Key> && std::is_same_v<Key, K>) {
            if (sz <= LINEAR_SEARCH_THRESHOLD) {
                for (size_type i = 0; i < sz; ++i) {
                    if (!comp_(k_ptr[i], key) && !comp_(key, k_ptr[i])) {
                        return i;
                    }
                }
                return npos;
            }
        }

        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        if (it != keys_.end() && !comp_(key, *it)) {
            return static_cast<size_type>(it - keys_.begin());
        }
        return npos;
    }

public:
    flat_map() = default;

    [[nodiscard]] MEOW_ALWAYS_INLINE const KeyContainer& keys() const noexcept { return keys_; }
    [[nodiscard]] MEOW_ALWAYS_INLINE const ValueContainer& values() const noexcept { return values_; }

    [[nodiscard]] MEOW_ALWAYS_INLINE T* data_values() noexcept { return values_.data(); }
    [[nodiscard]] MEOW_ALWAYS_INLINE const T* data_values() const noexcept { return values_.data(); }
    
    [[nodiscard]] MEOW_ALWAYS_INLINE bool empty() const noexcept { return keys_.empty(); }
    [[nodiscard]] MEOW_ALWAYS_INLINE size_type size() const noexcept { return keys_.size(); }
    [[nodiscard]] MEOW_ALWAYS_INLINE size_type capacity() const noexcept { return keys_.capacity(); }

    void clear() noexcept {
        keys_.clear();
        values_.clear();
    }
    
    void reserve(size_type n) {
        keys_.reserve(n);
        values_.reserve(n);
    }

    // --- Lookup ---

    template <typename K = Key>
    [[nodiscard]] MEOW_ALWAYS_INLINE bool contains(const K& key) const noexcept {
        return find_impl(key) != npos;
    }

    [[nodiscard]] MEOW_ALWAYS_INLINE T* find(const Key& key) noexcept {
        size_type idx = find_impl(key);
        if (idx != npos) return &values_[idx];
        return nullptr;
    }
    
    [[nodiscard]] MEOW_ALWAYS_INLINE const T* find(const Key& key) const noexcept {
        size_type idx = find_impl(key);
        if (idx != npos) return &values_[idx];
        return nullptr;
    }

    [[nodiscard]] MEOW_ALWAYS_INLINE T& get_unchecked(size_type idx) noexcept {
        return values_[idx];
    }

    // --- Operators ---

    T& operator[](const Key& key) {
        if constexpr (FastScalarKey<Key>) {
            size_type idx = find_impl(key);
            if (idx != npos) return values_[idx];
        }
        return *try_emplace(key).first;
    }

    T& operator[](Key&& key) {
        if constexpr (FastScalarKey<Key>) {
             size_type idx = find_impl(key);
             if (idx != npos) return values_[idx];
        }
        return *try_emplace(std::move(key)).first;
    }

    // --- Insertion ---

    template <typename... Args>
    MEOW_FLATTEN std::pair<T*, bool> try_emplace(Key&& key, Args&&... args) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        size_type idx = static_cast<size_type>(it - keys_.begin());

        if (it != keys_.end() && !comp_(key, *it)) {
            return {&values_[idx], false};
        }

        if (keys_.size() == keys_.capacity()) [[unlikely]] {
            grow_if_needed();
            it = keys_.begin() + idx;
        }

        keys_.insert(it, std::move(key));

        if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
            values_.emplace(values_.begin() + idx, std::forward<Args>(args)...);
        } else {
            try {
                values_.emplace(values_.begin() + idx, std::forward<Args>(args)...);
            } catch (...) {
                keys_.erase(keys_.begin() + idx);
                throw;
            }
        }

        return {&values_[idx], true};
    }

    template <typename... Args>
    std::pair<T*, bool> try_emplace(const Key& key, Args&&... args) {
        return try_emplace(Key(key), std::forward<Args>(args)...);
    }

    size_type erase(const Key& key) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        if (it != keys_.end() && !comp_(key, *it)) {
            size_type idx = static_cast<size_type>(it - keys_.begin());
            keys_.erase(it);
            values_.erase(values_.begin() + idx);
            return 1;
        }
        return 0;
    }
};

} // namespace meow