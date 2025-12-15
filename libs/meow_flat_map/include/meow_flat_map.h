#pragma once

#include <vector>
#include <algorithm>
#include <functional>
#include <concepts>
#include <utility>
#include <stdexcept>
#include <ranges>

namespace meow {

template<typename Key, typename T, 
         typename Compare = std::less<Key>, 
         typename KeyContainer = std::vector<Key>,
         typename ValueContainer = std::vector<T>>
class flat_map {
public:
    using key_type = Key;
    using mapped_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;

private:
    KeyContainer keys_;
    ValueContainer values_;
    [[no_unique_address]] Compare comp_;

public:
    flat_map() = default;
    
    void reserve(size_type n) {
        keys_.reserve(n);
        values_.reserve(n);
    }

    [[nodiscard]] bool empty() const noexcept { return keys_.empty(); }
    [[nodiscard]] size_type size() const noexcept { return keys_.size(); }
    [[nodiscard]] size_type capacity() const noexcept { return keys_.capacity(); }

    void clear() noexcept {
        keys_.clear();
        values_.clear();
    }

    [[nodiscard]] const KeyContainer& keys() const noexcept { return keys_; }
    [[nodiscard]] const ValueContainer& values() const noexcept { return values_; }

    T& unsafe_get(size_type index) { return values_[index]; }
    const T& unsafe_get(size_type index) const { return values_[index]; }

    T& at(const Key& key) {
        size_type idx = index_of(key);
        if (idx == npos) throw std::out_of_range("meow::flat_map::at: key not found");
        return values_[idx];
    }
    
    const T& at(const Key& key) const {
        size_type idx = index_of(key);
        if (idx == npos) throw std::out_of_range("meow::flat_map::at: key not found");
        return values_[idx];
    }

    T& operator[](const Key& key) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        size_type idx = std::distance(keys_.begin(), it);

        if (it != keys_.end() && !comp_(key, *it)) {
            return values_[idx];
        }

        keys_.insert(it, key);
        return *values_.insert(values_.begin() + idx, T{});
    }

    T& operator[](Key&& key) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        size_type idx = std::distance(keys_.begin(), it);

        if (it != keys_.end() && !comp_(key, *it)) {
            return values_[idx];
        }

        keys_.insert(it, std::move(key));
        return *values_.insert(values_.begin() + idx, T{});
    }

    template <typename... Args>
    std::pair<size_type, bool> try_emplace(const Key& key, Args&&... args) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        size_type idx = std::distance(keys_.begin(), it);

        if (it != keys_.end() && !comp_(key, *it)) {
            return {idx, false};
        }

        keys_.insert(it, key);
        values_.emplace(values_.begin() + idx, std::forward<Args>(args)...);
        return {idx, true};
    }
    
    template <typename M>
    std::pair<size_type, bool> insert_or_assign(const Key& key, M&& obj) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        size_type idx = std::distance(keys_.begin(), it);

        if (it != keys_.end() && !comp_(key, *it)) {
            values_[idx] = std::forward<M>(obj);
            return {idx, false};
        }

        keys_.insert(it, key);
        values_.insert(values_.begin() + idx, std::forward<M>(obj));
        return {idx, true};
    }

    size_type erase(const Key& key) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
        if (it != keys_.end() && !comp_(key, *it)) {
            size_type idx = std::distance(keys_.begin(), it);
            keys_.erase(it);
            values_.erase(values_.begin() + idx);
            return 1;
        }
        return 0;
    }

    static constexpr size_type npos = static_cast<size_type>(-1);

    [[nodiscard]] bool contains(const Key& key) const noexcept {
        return index_of(key) != npos;
    }

    [[nodiscard]] size_type index_of(const Key& key) const noexcept {
        if (keys_.size() < 32) {
            for (size_type i = 0; i < keys_.size(); ++i) {
                if (!comp_(keys_[i], key) && !comp_(key, keys_[i])) return i;
            }
            return npos;
        }

        auto it = std::ranges::lower_bound(keys_, key, comp_);
        
        if (it != keys_.end() && !comp_(key, *it)) {
            return std::distance(keys_.begin(), it);
        }
        return npos;
    }
    
    template <typename K>
    requires (!std::is_same_v<K, Key> && requires(const Compare& c, const Key& k, const K& ok) { c(k, ok); c(ok, k); })
    [[nodiscard]] size_type index_of(const K& x) const noexcept {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), x, comp_);
        if (it != keys_.end() && !comp_(*it, x) && !comp_(x, *it)) {
            return std::distance(keys_.begin(), it);
        }
        return npos;
    }
};

} // namespace meow