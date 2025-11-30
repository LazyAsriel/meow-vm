#pragma once

#include "meow_variant.h"
#include <stdexcept>
#include <concepts>
#include <compare> 

namespace meow {

struct nullopt_t {
    explicit constexpr nullopt_t(int) {}
};
inline constexpr nullopt_t nullopt{0};

template <typename T>
class optional {
    meow::variant<std::monostate, T> storage_;

public:
    using value_type = T;

    constexpr optional() noexcept : storage_(std::monostate{}) {}
    constexpr optional(nullopt_t) noexcept : storage_(std::monostate{}) {}

    constexpr optional(const T& v) : storage_(v) {}
    constexpr optional(T&& v) : storage_(std::move(v)) {}

    constexpr optional(const optional&) = default;
    constexpr optional(optional&&) = default;

    constexpr optional& operator=(nullopt_t) noexcept {
        storage_ = std::monostate{};
        return *this;
    }
    
    constexpr optional& operator=(const optional&) = default;
    constexpr optional& operator=(optional&&) = default;

    template <typename U = T>
    requires std::is_constructible_v<T, U> && (!std::is_same_v<std::decay_t<U>, optional>)
    constexpr optional& operator=(U&& v) {
        storage_ = std::forward<U>(v);
        return *this;
    }

    // 2. Observers
    [[nodiscard]] constexpr bool has_value() const noexcept {
        return storage_.index() == 1; // Index 1 là T
    }

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] constexpr T& operator*() & noexcept {
        return storage_.template get<T>();
    }

    [[nodiscard]] constexpr const T& operator*() const & noexcept {
        return storage_.template get<T>();
    }

    [[nodiscard]] constexpr T* operator->() noexcept {
        return storage_.template get_if<T>();
    }

    [[nodiscard]] constexpr const T* operator->() const noexcept {
        return storage_.template get_if<T>();
    }

    constexpr T& value() & {
        if (!has_value()) throw std::bad_variant_access();
        return storage_.template get<T>();
    }

    constexpr const T& value() const & {
        if (!has_value()) throw std::bad_variant_access();
        return storage_.template get<T>();
    }

    template <typename U>
    constexpr T value_or(U&& default_value) const & {
        if (has_value()) return **this;
        return static_cast<T>(std::forward<U>(default_value));
    }

    template <typename F>
    constexpr auto and_then(F&& f) const & {
        if (has_value()) {
            return std::invoke(std::forward<F>(f), **this);
        } else {
            return std::remove_cvref_t<std::invoke_result_t<F, const T&>>{};
        }
    }

    template <typename F>
    constexpr auto transform(F&& f) const & {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if (has_value()) {
            return optional<U>(std::invoke(std::forward<F>(f), **this));
        } else {
            return optional<U>{};
        }
    }

    template <typename F>
    constexpr auto or_else(F&& f) const & {
        if (has_value()) {
            return *this;
        } else {
            return std::invoke(std::forward<F>(f));
        }
    }
    
    void reset() noexcept {
        storage_ = std::monostate{};
    }

    void swap(optional& other) noexcept {
        storage_.swap(other.storage_);
    }
    
    friend bool operator==(const optional& lhs, const optional& rhs) {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;
        return *lhs == *rhs;
    }
    
    friend bool operator==(const optional& lhs, nullopt_t) { return !lhs.has_value(); }
};

} // namespace meow