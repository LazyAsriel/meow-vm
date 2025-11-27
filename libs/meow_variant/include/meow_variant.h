#pragma once

#include <concepts>
#include <utility>
#include <cassert>
#include <exception>

#include "internal/nanbox.h"
#include "internal/fallback.h"
#include "internal/utils.h"

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#define MEOW_BYTE_ORDER __BYTE_ORDER__
#define MEOW_ORDER_LITTLE __ORDER_LITTLE_ENDIAN__
#endif

#if (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)) && (defined(MEOW_BYTE_ORDER) && MEOW_BYTE_ORDER == MEOW_ORDER_LITTLE)
#define MEOW_CAN_USE_NAN_BOXING 1
#else
#define MEOW_CAN_USE_NAN_BOXING 0
#endif

namespace meow {

using utils::overload;

namespace detail_backend {
    template <typename FlattenedList> struct select;
    template <typename... Ts>
    struct select<utils::detail::type_list<Ts...>> {
        static constexpr bool can_nanbox = MEOW_CAN_USE_NAN_BOXING;
        static constexpr bool small = (sizeof...(Ts) <= 8);
        static constexpr bool types_ok = utils::all_nanboxable_impl<utils::detail::type_list<Ts...>>::value;
        
        using type = std::conditional_t<(can_nanbox && small && types_ok), 
                                        utils::NaNBoxedVariant<Ts...>, 
                                        utils::FallbackVariant<Ts...>>;
    };
}

template <typename... Args>
class variant {
    using flattened_list_t = utils::flattened_unique_t<Args...>;
    using implementation_t = typename detail_backend::select<flattened_list_t>::type;

    implementation_t storage_;

public:
    variant() = default;
    variant(const variant&) = default;
    variant(variant&&) = default;
    variant& operator=(const variant&) = default;
    variant& operator=(variant&&) = default;

    template <typename T>
    requires (!std::is_same_v<std::decay_t<T>, variant> && 
              utils::detail::type_list_contains<std::decay_t<T>, flattened_list_t>::value)
    variant(T&& value) noexcept : storage_(std::forward<T>(value)) {}

    template <typename T>
    requires (!std::is_same_v<std::decay_t<T>, variant> && 
              utils::detail::type_list_contains<std::decay_t<T>, flattened_list_t>::value)
    variant& operator=(T&& value) noexcept {
        storage_ = std::forward<T>(value);
        return *this;
    }

    [[nodiscard]] std::size_t index() const noexcept { return storage_.index(); }
    [[nodiscard]] bool valueless() const noexcept { return storage_.valueless(); }
    [[nodiscard]] bool has_value() const noexcept { return !valueless(); }

    template <typename T> [[nodiscard]] bool holds() const noexcept { return storage_.template holds<T>(); }
    template <typename T> [[nodiscard]] bool is() const noexcept { return holds<T>(); }

    template <typename T> decltype(auto) get() { return storage_.template safe_get<T>(); }
    template <typename T> decltype(auto) get() const { return storage_.template safe_get<T>(); }

    template <typename T> [[nodiscard]] auto* get_if() noexcept { return storage_.template get_if<T>(); }
    template <typename T> [[nodiscard]] const auto* get_if() const noexcept { return storage_.template get_if<T>(); }

    template <typename Self, typename Visitor>
    decltype(auto) visit(this Self&& self, Visitor&& vis) {
        return self.storage_.visit(std::forward<Visitor>(vis));
    }
    
    template <typename Self, typename... Fs>
    decltype(auto) visit(this Self&& self, Fs&&... fs) {
        return self.storage_.visit(overload{std::forward<Fs>(fs)...});
    }

    template <typename F>
    auto transform(F&& f) const {
        if (valueless()) return variant<std::monostate>{}; 
        return this->visit([&](auto&& val) {
            return std::invoke(std::forward<F>(f), val);
        });
    }

    template <typename F>
    auto and_then(F&& f) const {
        if (valueless()) return std::invoke(std::forward<F>(f), std::monostate{}); 
        return this->visit([&](auto&& val) {
            return std::invoke(std::forward<F>(f), val);
        });
    }

    void swap(variant& other) noexcept { storage_.swap(other.storage_); }
    bool operator==(const variant& o) const { return storage_ == o.storage_; }
    bool operator!=(const variant& o) const { return storage_ != o.storage_; }
};

} // namespace meow