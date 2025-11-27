#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>
#include <concepts>

namespace meow {
template <typename...> class variant;
}

namespace meow::utils {

// Concept kiểm tra xem T có phải là meow::variant không
template <typename T>
concept MeowVariant = requires {
    typename T::inner_types;
};

namespace detail {

// ---------------- tiny typelist ----------------
template <typename... Ts> struct type_list {};

// Append
template <typename List, typename T> struct type_list_append;
template <typename... Ts, typename T>
struct type_list_append<type_list<Ts...>, T> { using type = type_list<Ts..., T>; };

// Contains
template <typename T, typename List> struct type_list_contains;
template <typename T, typename... Ts>
struct type_list_contains<T, type_list<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

// Append unique
template <typename List, typename T>
using type_list_append_unique_t = std::conditional_t<
    type_list_contains<T, List>::value, 
    List, 
    typename type_list_append<List, T>::type
>;

// Nth Type
template <std::size_t I, typename List> struct nth_type;
template <std::size_t I, typename Head, typename... Tail>
struct nth_type<I, type_list<Head, Tail...>> : nth_type<I - 1, type_list<Tail...>> {};
template <typename Head, typename... Tail>
struct nth_type<0, type_list<Head, Tail...>> { using type = Head; };

// Index Of (Fix warning unused value bằng cách dùng thuật toán clean hơn)
template <typename T, typename List> struct type_list_index_of;
template <typename T, typename... Ts>
struct type_list_index_of<T, type_list<Ts...>> {
    static constexpr std::size_t value = []() consteval {
        std::size_t idx = 0;
        bool found = false;
        // Dùng lambda để tránh warning expression result unused
        auto check = [&](bool is_same) {
            if (found) return;
            if (is_same) found = true;
            else idx++;
        };
        (check(std::is_same_v<T, Ts>), ...);
        return found ? idx : static_cast<std::size_t>(-1);
    }();
};
static constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

// Length
template <typename List> struct type_list_length;
template <typename... Ts>
struct type_list_length<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

// ---------------- Flatten logic ----------------
template <typename T>
struct variant_inner_list { using type = type_list<T>; };

template <typename... Inner>
struct variant_inner_list<meow::variant<Inner...>> { using type = type_list<Inner...>; };

template <typename...> struct flatten_list_implement;
template <> struct flatten_list_implement<> { using type = type_list<>; };

template <typename Head, typename... Tail>
struct flatten_list_implement<Head, Tail...> {
private:
    using tail_flat = typename flatten_list_implement<Tail...>::type;
    
    template <typename H> struct extract { using type = type_list<H>; };
    template <typename... Is> struct extract<type_list<Is...>> { using type = type_list<Is...>; };
    template <typename... Is> struct extract<meow::variant<Is...>> { using type = type_list<Is...>; }; 

    using head_list = typename extract<std::remove_cvref_t<Head>>::type;

    template <typename Src, typename Acc> struct merge_src;
    template <typename Acc> struct merge_src<type_list<>, Acc> { using type = Acc; };
    template <typename S0, typename... Ss, typename Acc>
    struct merge_src<type_list<S0, Ss...>, Acc> {
        using next_acc = type_list_append_unique_t<Acc, S0>;
        using type = typename merge_src<type_list<Ss...>, next_acc>::type;
    };

    using merged_head = typename merge_src<head_list, type_list<>>::type;
    using merged_all = typename merge_src<tail_flat, merged_head>::type;

public:
    using type = merged_all;
};

} // namespace detail

// Public API: Expose flattened_unique_t directly in utils namespace to fix your error
template <typename... Ts>
using flattened_unique_t = typename detail::flatten_list_implement<Ts...>::type;

template <class... Fs> struct overload : Fs... { using Fs::operator()...; };
template <class... Fs> overload(Fs...) -> overload<Fs...>;

} // namespace meow::utils