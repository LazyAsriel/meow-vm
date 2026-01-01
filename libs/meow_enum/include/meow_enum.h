#pragma once

#include <string_view>
#include <array>
#include <optional>
#include <algorithm>
#include <concepts>
#include <limits>
#include <utility>

namespace meow {

namespace detail {

template <auto V>
consteval auto get_enum_name_raw() {
    std::string_view name;
#if defined(__clang__) || defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
#else
    #error "Unsupported compiler for meow::enum_ptr"
#endif
    return name;
}

template <auto V>
consteval auto parse_enum_name() {
    constexpr std::string_view raw = get_enum_name_raw<V>();
    constexpr auto end = raw.find_last_not_of(" ]");
    
    constexpr auto start_eq = raw.find_last_of('=', end);
    if constexpr (start_eq == std::string_view::npos) return std::string_view{};

    constexpr auto start = raw.find_first_not_of(' ', start_eq + 1);
    
    constexpr auto final_view = raw.substr(start, end - start + 1);

    constexpr auto last_colon = final_view.find_last_of(':');
    if constexpr (last_colon != std::string_view::npos) return final_view.substr(last_colon + 1);
    
    return final_view;
}

template <typename E, auto V>
consteval bool is_valid_enum() {
    constexpr auto name = parse_enum_name<static_cast<E>(V)>();
    return !name.empty() && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z'));
}

} // namespace detail

template <typename E>
requires std::is_enum_v<E>
struct enum_traits {
    static constexpr int min_val = -128;
    static constexpr int max_val = 128;
};

template <auto V>
consteval std::string_view enum_name() {
    constexpr auto name = detail::parse_enum_name<V>();
    static_assert(!name.empty(), "Invalid enum value or unnamed enum");
    return name;
}

template <typename E>
[[nodiscard]] constexpr std::string_view enum_name(E value) {
    static_assert(std::is_enum_v<E>);
    constexpr int min = enum_traits<E>::min_val;
    constexpr int max = enum_traits<E>::max_val;
    constexpr int range = max - min + 1;

    static constexpr auto names = []<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<std::string_view, range>{
            detail::parse_enum_name<static_cast<E>(static_cast<int>(I) + min)>()...
        };
    }(std::make_index_sequence<range>{});

    static constexpr auto valid = []<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<bool, range>{
            detail::is_valid_enum<E, static_cast<E>(static_cast<int>(I) + min)>()...
        };
    }(std::make_index_sequence<range>{});

    int idx = static_cast<int>(value) - min;
    if (idx >= 0 && idx < range && valid[idx]) {
        return names[idx];
    }
    return {};
}

// 3. List all values
template <typename E>
[[nodiscard]] constexpr auto enum_values() {
    static_assert(std::is_enum_v<E>);
    constexpr int min = enum_traits<E>::min_val;
    constexpr int max = enum_traits<E>::max_val;
    constexpr int range = max - min + 1;

    return []<std::size_t... I>(std::index_sequence<I...>) {
        constexpr auto valid_mask = std::array<bool, range>{
            detail::is_valid_enum<E, static_cast<E>(static_cast<int>(I) + min)>()...
        };

        constexpr std::size_t count = std::count(valid_mask.begin(), valid_mask.end(), true);

        std::array<E, count> values{};
        std::size_t idx = 0;
        
        for (std::size_t k = 0; k < range; ++k) {
            if (valid_mask[k]) {
                values[idx++] = static_cast<E>(static_cast<int>(k) + min);
            }
        }
        return values;
    }(std::make_index_sequence<range>{});
}

template <typename E>
[[nodiscard]] constexpr std::optional<E> enum_cast(std::string_view str) {
    for (auto v : enum_values<E>()) {
        if (enum_name(v) == str) {
            return v;
        }
    }
    return std::nullopt;
}

} // namespace meow