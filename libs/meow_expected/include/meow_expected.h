#pragma once

#include "meow_variant.h"
#include <stdexcept>
#include <concepts>
#include <type_traits>

namespace meow {

template <typename E>
struct unexpected {
    E value;
    explicit unexpected(E&& v) : value(std::move(v)) {}
    explicit unexpected(const E& v) : value(v) {}
};

template <typename E> unexpected(E) -> unexpected<E>;

template <typename T, typename E>
class expected {
    meow::variant<T, E> storage_;
public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    // 1. Constructor mặc định (nếu T mặc định được)
    expected() requires std::is_default_constructible_v<T> 
        : storage_(T{}) {}

    // 2. Construct từ Value (T)
    // Dùng concept để tránh nhầm lẫn với copy/move constructor của chính expected
    template <typename U = T>
    requires (!std::is_same_v<std::decay_t<U>, expected> &&
              !std::is_same_v<std::decay_t<U>, unexpected<E>> &&
              std::is_constructible_v<T, U>)
    expected(U&& v) : storage_(std::forward<U>(v)) {}

    // 3. Construct từ Error (unexpected<E>)
    // Ta "bóc" cái vỏ unexpected ra, nhét ruột E vào variant
    template <typename G>
    expected(const unexpected<G>& e) : storage_(e.value) {}
    
    template <typename G>
    expected(unexpected<G>&& e) : storage_(std::move(e.value)) {}

    // --- Observers ---

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return storage_.template holds<T>();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    constexpr const T& value() const & {
        if (!has_value()) throw std::bad_variant_access();
        return storage_.template get<T>();
    }

    constexpr T& value() & {
        if (!has_value()) throw std::bad_variant_access();
        return storage_.template get<T>();
    }

    constexpr const T& operator*() const & noexcept {
        return storage_.template get<T>();
    }

    constexpr T& operator*() & noexcept {
        return storage_.template get<T>();
    }

    constexpr const T* operator->() const noexcept {
        return storage_.template get_if<T>();
    }

    constexpr T* operator->() noexcept {
        return storage_.template get_if<T>();
    }

    constexpr const E& error() const & {
        if (has_value()) throw std::bad_variant_access(); 
        return storage_.template get<E>();
    }
    
    constexpr E& error() & {
        if (has_value()) throw std::bad_variant_access();
        return storage_.template get<E>();
    }

    template <typename F>
    constexpr auto and_then(F&& f) const & {
        if (has_value()) {
            return std::invoke(std::forward<F>(f), value());
        } else {
            using Ret = std::invoke_result_t<F, const T&>;
            return Ret(unexpected<E>(error()));
        }
    }

    template <typename F>
    constexpr auto transform(F&& f) const & {
        using U = std::invoke_result_t<F, const T&>;
        if (has_value()) {
            return expected<U, E>(std::invoke(std::forward<F>(f), value()));
        } else {
            return expected<U, E>(unexpected<E>(error()));
        }
    }

    template <typename F>
    constexpr auto or_else(F&& f) const & {
        if (!has_value()) {
            return std::invoke(std::forward<F>(f), error());
        } else {
            return *this;
        }
    }
    
    template <typename U>
    constexpr T value_or(U&& default_value) const & {
        if (has_value()) return value();
        return static_cast<T>(std::forward<U>(default_value));
    }
};

} // namespace meow