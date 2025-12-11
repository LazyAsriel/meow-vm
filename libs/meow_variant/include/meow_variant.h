#pragma once

#include <concepts>
#include <utility>
#include <cassert>
#include <exception>
#include <type_traits>
#include <functional>

#include "internal/nanbox.h"
#include "internal/fallback.h"
#include "internal/utils.h"

// --- Platform Detection ---
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #define MEOW_BYTE_ORDER __BYTE_ORDER__
    #define MEOW_ORDER_LITTLE __ORDER_LITTLE_ENDIAN__
#endif

#if (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)) && \
    (defined(MEOW_BYTE_ORDER) && MEOW_BYTE_ORDER == MEOW_ORDER_LITTLE)
    #define MEOW_CAN_USE_NAN_BOXING 1
#else
    #define MEOW_CAN_USE_NAN_BOXING 0
#endif

namespace meow {

using utils::overload;

struct no_flatten_t { explicit constexpr no_flatten_t(int) {} };
inline constexpr no_flatten_t no_flatten{0};

// --- Layout Policies ---
namespace layout {
    struct auto_detect {}; 
    struct nanbox {};      
    struct fallback {};    
}

namespace detail {
    // Config Parser
    template<typename... Args>
    struct config_parser {
        static constexpr bool flatten = true;
        using types = utils::detail::type_list<Args...>;
    };

    template<typename... Args>
    struct config_parser<no_flatten_t, Args...> {
        static constexpr bool flatten = false;
        using types = utils::detail::type_list<Args...>;
    };

    // List Builder
    template <bool Flatten, typename List> struct list_builder;
    template <typename... Ts>
    struct list_builder<true, utils::detail::type_list<Ts...>> {
        using type = utils::flattened_unique_t<Ts...>;
    };
    template <typename... Ts>
    struct list_builder<false, utils::detail::type_list<Ts...>> {
        using type = utils::unique_t<Ts...>;
    };

    // Backend Resolver
    template <typename LayoutPolicy, typename TypeList>
    struct backend_resolver;

    template <typename Policy, typename... Ts>
    struct backend_resolver<Policy, utils::detail::type_list<Ts...>> {
        static constexpr bool is_supported_arch = MEOW_CAN_USE_NAN_BOXING;
        static constexpr bool is_small_enough   = (sizeof...(Ts) <= 16);
        static constexpr bool are_types_valid   = utils::all_nanboxable_impl<utils::detail::type_list<Ts...>>::value;

        static constexpr bool use_nanbox = []() {
            if constexpr (std::is_same_v<Policy, layout::nanbox>) {
                static_assert(is_supported_arch, "Nanbox layout requires 64-bit Little Endian architecture.");
                static_assert(are_types_valid, "Types are not compatible with Nanboxing.");
                static_assert(is_small_enough, "Too many types for Nanboxing (max 16).");
                return true;
            } else if constexpr (std::is_same_v<Policy, layout::fallback>) {
                return false;
            } else {
                return is_supported_arch && is_small_enough && are_types_valid;
            }
        }();

        using type = std::conditional_t<use_nanbox,
                                        meow::utils::NaNBoxedVariant<Ts...>,
                                        meow::utils::FallbackVariant<Ts...>>;
    };
}

template <typename... Args>
using variant = basic_variant<layout::auto_detect, Args...>;

template <typename... Args>
using nanboxed_variant = basic_variant<layout::nanbox, Args...>;

template <typename... Args>
using fallback_variant = basic_variant<layout::fallback, Args...>;

// --- Basic Variant Class ---
template <typename LayoutPolicy, typename... Args>
class basic_variant {
    using config = detail::config_parser<Args...>;
    using final_list_t = typename detail::list_builder<config::flatten, typename config::types>::type;
    using implementation_t = typename detail::backend_resolver<LayoutPolicy, final_list_t>::type;

    implementation_t storage_;

public:
    using inner_types = final_list_t;
    using backend_type = implementation_t;
    using layout_type = LayoutPolicy;

    // --- Constructors ---
    constexpr basic_variant() = default;
    constexpr basic_variant(const basic_variant&) = default;
    constexpr basic_variant(basic_variant&&) = default;
    constexpr basic_variant& operator=(const basic_variant&) = default;
    constexpr basic_variant& operator=(basic_variant&&) = default;

    // Value Constructor
    template <typename T>
    requires (!std::is_same_v<std::decay_t<T>, basic_variant> && 
              utils::detail::type_list_contains<std::decay_t<T>, final_list_t>::value)
    constexpr basic_variant(T&& value) noexcept 
        : storage_(std::forward<T>(value)) {}

    template <typename T>
    requires (!std::is_same_v<std::decay_t<T>, basic_variant> && 
              utils::detail::type_list_contains<std::decay_t<T>, final_list_t>::value)
    constexpr basic_variant& operator=(T&& value) noexcept {
        storage_ = std::forward<T>(value);
        return *this;
    }

    // Converting Constructor
    template <typename OtherLayout, typename... OtherArgs>
    constexpr basic_variant(const basic_variant<OtherLayout, OtherArgs...>& other) {
        other.visit([&](const auto& val) {
            if constexpr (utils::detail::type_list_contains<std::decay_t<decltype(val)>, final_list_t>::value) {
                storage_ = val;
            } else {
                assert(false && "Incompatible variant conversion"); 
            }
        });
    }

    // --- Accessors ---
    [[nodiscard]] constexpr std::size_t index() const noexcept { return storage_.index(); }
    [[nodiscard]] constexpr bool valueless() const noexcept { return storage_.valueless(); }
    [[nodiscard]] constexpr bool has_value() const noexcept { return !valueless(); }
    
    template <typename T> [[nodiscard]] constexpr bool holds() const noexcept { return storage_.template holds<T>(); }
    template <typename T> [[nodiscard]] constexpr bool is() const noexcept { return holds<T>(); }

    template <typename T> constexpr decltype(auto) get() & noexcept { return storage_.template get<T>(); }
    template <typename T> constexpr decltype(auto) get() const & noexcept { return storage_.template get<T>(); }
    template <typename T> constexpr decltype(auto) get() && noexcept { return std::move(storage_).template get<T>(); }

    template <typename T> constexpr decltype(auto) safe_get() & { return storage_.template safe_get<T>(); }
    template <typename T> constexpr decltype(auto) safe_get() const & { return storage_.template safe_get<T>(); }
    template <typename T> constexpr decltype(auto) safe_get() && { return std::move(storage_).template safe_get<T>(); }

    template <typename T> constexpr decltype(auto) unsafe_get() & noexcept { return storage_.template unsafe_get<T>(); }
    template <typename T> constexpr decltype(auto) unsafe_get() const & noexcept { return storage_.template unsafe_get<T>(); }

    template <typename T> [[nodiscard]] constexpr auto* get_if() noexcept { return storage_.template get_if<T>(); }
    template <typename T> [[nodiscard]] constexpr const auto* get_if() const noexcept { return storage_.template get_if<T>(); }

    // --- Visitation ---
    template <typename Self, typename Visitor>
    constexpr decltype(auto) visit(this Self&& self, Visitor&& vis) {
        return std::forward<Self>(self).storage_.visit(std::forward<Visitor>(vis));
    }
    
    template <typename Self, typename... Fs>
    constexpr decltype(auto) visit(this Self&& self, Fs&&... fs) {
        return std::forward<Self>(self).storage_.visit(overload{std::forward<Fs>(fs)...});
    }

    template <typename R, typename Self, typename Visitor>
    constexpr R visit(this Self&& self, Visitor&& vis) {
        return std::forward<Self>(self).storage_.visit(
            [&vis]<typename T>(T&& arg) -> R {
                if constexpr (std::is_void_v<R>) {
                    std::invoke(std::forward<Visitor>(vis), std::forward<T>(arg));
                } else {
                    return static_cast<R>(std::invoke(std::forward<Visitor>(vis), std::forward<T>(arg)));
                }
            }
        );
    }

    // --- Monadic Operations ---
    template <typename F>
    constexpr auto transform(F&& f) const {
        if (valueless()) return variant<std::monostate>{};
        
        return this->visit([&](auto&& val) {
            using ResultType = std::invoke_result_t<F, decltype(val)>;
            return variant<ResultType>(std::invoke(std::forward<F>(f), val));
        });
    }

    template <typename F>
    constexpr auto and_then(F&& f) const {
        if (valueless()) {
             return std::invoke(std::forward<F>(f), std::monostate{}); 
        }
        return this->visit([&](auto&& val) {
            return std::invoke(std::forward<F>(f), val);
        });
    }

    // --- Utilities ---
    void swap(basic_variant& other) noexcept { storage_.swap(other.storage_); }
    bool operator==(const basic_variant& o) const { return storage_ == o.storage_; }
    bool operator!=(const basic_variant& o) const { return storage_ != o.storage_; }

    [[nodiscard]] backend_type& backend() noexcept { return storage_; }
    [[nodiscard]] const backend_type& backend() const noexcept { return storage_; }
};

template <typename L, typename... Args>
void swap(basic_variant<L, Args...>& a, basic_variant<L, Args...>& b) noexcept {
    a.swap(b);
}

} // namespace meow