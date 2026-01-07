#pragma once

#include <concepts>
#include <utility>
#include <cassert>
#include <exception>
#include <type_traits>
#include <functional>
#include <bit>

#include "internal/nanbox.h"
#include "internal/fallback.h"
#include "internal/utils.h"

namespace meow {

namespace arch {
    constexpr bool is_little_endian = std::endian::native == std::endian::little;

    constexpr bool is_64bit = sizeof(void*) == 8;

    consteval bool is_supported_architecture() {
        #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
            return true;
        #else
            return false;
        #endif
    }

    constexpr bool can_use_nan_boxing = is_little_endian && is_64bit && is_supported_architecture();
}

using utils::overload;

struct no_flatten_t { explicit constexpr no_flatten_t(int) {} };
inline constexpr no_flatten_t no_flatten{0};

namespace layout {
    struct auto_detect {}; 
    struct nanbox {};      
    struct fallback {};    
}

namespace detail {
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

    template <bool Flatten, typename List> struct list_builder;
    template <typename... Ts>
    struct list_builder<true, utils::detail::type_list<Ts...>> {
        using type = utils::flattened_unique_t<Ts...>;
    };
    template <typename... Ts>
    struct list_builder<false, utils::detail::type_list<Ts...>> {
        using type = utils::unique_t<Ts...>;
    };

    template <typename LayoutPolicy, typename TypeList>
    struct backend_resolver;

    template <typename Policy, typename... Ts>
    struct backend_resolver<Policy, utils::detail::type_list<Ts...>> {
        static constexpr bool is_supported_env = arch::can_use_nan_boxing;
        static constexpr bool is_small_enough  = (sizeof...(Ts) <= 16);
        static constexpr bool are_types_valid  = utils::all_nanboxable_impl<utils::detail::type_list<Ts...>>::value;

        static constexpr bool use_nanbox = []() {
            if constexpr (std::is_same_v<Policy, layout::nanbox>) {
                static_assert(arch::is_little_endian, "Nanbox layout requires Little Endian architecture.");
                static_assert(arch::is_64bit, "Nanbox layout requires 64-bit pointers.");
                static_assert(arch::is_supported_architecture(), "Nanbox layout requires x86_64 or AArch64.");
                static_assert(are_types_valid, "Types are not compatible with Nanboxing.");
                static_assert(is_small_enough, "Too many types for Nanboxing (max 16).");
                return true;
            } else if constexpr (std::is_same_v<Policy, layout::fallback>) {
                return false;
            } else {
                return is_supported_env && is_small_enough && are_types_valid;
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
    // --- Expose Layout Info ---
    using layout_traits = typename implementation_t::layout_traits;

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
    template <typename T>
    static constexpr std::size_t index_of() noexcept {
        return utils::detail::type_list_index_of<std::decay_t<T>, final_list_t>::value;
    }

    [[nodiscard]] [[gnu::always_inline]]
    auto raw() const noexcept requires (requires { storage_.raw(); }) {
        return storage_.raw();
    }
    
    static basic_variant from_raw(uint64_t bits) noexcept 
    requires (requires { backend_type::from_raw(uint64_t{}); }) {
        basic_variant v;
        v.storage_ = backend_type::from_raw(bits);
        return v;
    }
    [[nodiscard]] constexpr std::size_t index() const noexcept { return storage_.index(); }
    [[nodiscard]] constexpr bool valueless() const noexcept { return storage_.valueless(); }
    [[nodiscard]] constexpr bool has_value() const noexcept { return !valueless(); }
    
    [[nodiscard]] [[gnu::always_inline]]
    uint64_t raw_tag() const noexcept {
        return storage_.raw_tag();
    }

    [[gnu::always_inline]]
    void set_raw(uint64_t bits) noexcept {
        storage_.set_raw(bits);
    }

    // --- 2. Type Checking API ---
    [[nodiscard]] [[gnu::always_inline]]
    bool has_same_type_as(const basic_variant& other) const noexcept {
        return storage_.has_same_type_as(other.storage_);
    }

    template <typename T>
    [[nodiscard]] [[gnu::always_inline]]
    bool holds_both(const basic_variant& other) const noexcept {
        return storage_.template holds_both<T>(other.storage_);
    }

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

    template <typename T>
    constexpr void set(T&& v) noexcept {
        *this = std::forward<T>(v);
    }

    template <typename T>
    [[gnu::always_inline]]
    void unsafe_set(T&& v) noexcept {
        storage_.template unsafe_set<T>(std::forward<T>(v));
    }

    template <typename T>
    constexpr bool safe_set(T&& v) noexcept {
        if (holds<T>()) [[likely]] {
            unsafe_set<T>(std::forward<T>(v));
            return true;
        }
        return false;
    }

    template <typename T>
    uint64_t set_as(T&& value) noexcept {
        return storage_.template set_as<std::decay_t<T>>(std::forward<T>(value));
    }

    template <typename T>
    T get_as() const noexcept {
        return storage_.template get_as<T>();
    }

    void recover(uint64_t backup_bits) noexcept {
        set_raw(backup_bits);
    }

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
        if (valueless()) return variant<meow::monostate>{};
        
        return this->visit([&](auto&& val) {
            using ResultType = std::invoke_result_t<F, decltype(val)>;
            return variant<ResultType>(std::invoke(std::forward<F>(f), val));
        });
    }

    template <typename F>
    constexpr auto and_then(F&& f) const {
        if (valueless()) {
             return std::invoke(std::forward<F>(f), meow::monostate{}); 
        }
        return this->visit([&](auto&& val) {
            return std::invoke(std::forward<F>(f), val);
        });
    }

    // --- Utilities ---
    void swap(basic_variant& other) noexcept { storage_.swap(other.storage_); }
    bool operator==(const basic_variant& o) const { return storage_ == o.storage_; }
    bool operator!=(const basic_variant& o) const { return storage_ != o.storage_; }

    std::partial_ordering operator<=>(const basic_variant& other) const {
        if (valueless() && other.valueless()) return std::partial_ordering::equivalent;
        if (valueless()) return std::partial_ordering::less;
        if (other.valueless()) return std::partial_ordering::greater;
        
        if (index() != other.index()) return index() <=> other.index();
        
        return this->visit([&](const auto& val) -> std::partial_ordering {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::three_way_comparable<T>) {
                return val <=> other.unsafe_get<T>();
            } else {
                return std::partial_ordering::equivalent; 
            }
        });
    }

    [[nodiscard]] backend_type& backend() noexcept { return storage_; }
    [[nodiscard]] const backend_type& backend() const noexcept { return storage_; }
};

// --- Free Function Utility ---
template <typename L, typename... Args>
[[nodiscard]] constexpr bool same_type(const basic_variant<L, Args...>& lhs, const basic_variant<L, Args...>& rhs) noexcept {
    return lhs.has_same_type_as(rhs);
}

template <typename T, typename L, typename... Args>
[[nodiscard]] constexpr bool holds_both(const basic_variant<L, Args...>& lhs, const basic_variant<L, Args...>& rhs) noexcept {
    return lhs.template holds_both<T>(rhs);
}

template <typename L, typename... Args>
void swap(basic_variant<L, Args...>& a, basic_variant<L, Args...>& b) noexcept {
    a.swap(b);
}

} // namespace meow