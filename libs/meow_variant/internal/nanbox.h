#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant> 
#include <stdexcept> // bad_variant_access
#include "internal/utils.h"

namespace meow::utils {

// Check platform
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__))
    #define MEOW_LITTLE_64 1
#else
    #define MEOW_LITTLE_64 0
#endif

// Classifiers
template <typename T> concept PointerLike = std::is_pointer_v<std::decay_t<T>>;
template <typename T> concept IntegralLike = std::is_integral_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>;
template <typename T> concept DoubleLike = std::is_floating_point_v<std::decay_t<T>>;
template <typename T> concept BoolLike = std::is_same_v<std::decay_t<T>, bool>;

// Nanbox check
template <typename List> struct all_nanboxable_impl;
template <typename... Ts>
struct all_nanboxable_impl<detail::type_list<Ts...>> {
    static constexpr bool value = ((sizeof(std::decay_t<Ts>) <= 8 && 
        (DoubleLike<Ts> || IntegralLike<Ts> || PointerLike<Ts> || 
         std::is_same_v<std::decay_t<Ts>, std::monostate> || BoolLike<Ts>)) && ...);
};

static constexpr uint64_t MEOW_EXP_MASK     = 0x7FF0000000000000ULL;
static constexpr uint64_t MEOW_QNAN_PREFIX  = 0x7FF8000000000000ULL;
static constexpr uint64_t MEOW_TAG_MASK     = 0x0007000000000000ULL;
static constexpr uint64_t MEOW_PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;
static constexpr unsigned MEOW_TAG_SHIFT    = 48;
static constexpr uint64_t MEOW_VALUELESS    = 0xFFFFFFFFFFFFFFFFULL;

inline uint64_t to_bits(double d) { return std::bit_cast<uint64_t>(d); }
inline double from_bits(uint64_t u) { return std::bit_cast<double>(u); }
inline bool is_double(uint64_t b) { return (b & MEOW_EXP_MASK) != MEOW_EXP_MASK; }

// ============================================================================
// NaNBoxedVariant
// ============================================================================
template <typename... Args>
class NaNBoxedVariant {
    using flat_list = meow::utils::flattened_unique_t<Args...>;
    static constexpr std::size_t count = detail::type_list_length<flat_list>::value;
    static constexpr std::size_t dbl_idx = detail::type_list_index_of<double, flat_list>::value;

public:
    using inner_types = flat_list;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    NaNBoxedVariant() noexcept {
        if constexpr (std::is_same_v<typename detail::nth_type<0, flat_list>::type, std::monostate>) {
            bits_ = MEOW_QNAN_PREFIX;
        } else {
            bits_ = MEOW_VALUELESS;
        }
    }

    NaNBoxedVariant(const NaNBoxedVariant&) = default;
    NaNBoxedVariant(NaNBoxedVariant&& o) noexcept : bits_(o.bits_) { o.bits_ = MEOW_VALUELESS; }

    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    NaNBoxedVariant(T&& v) noexcept {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
    }

    NaNBoxedVariant& operator=(const NaNBoxedVariant&) = default;
    NaNBoxedVariant& operator=(NaNBoxedVariant&& o) noexcept {
        bits_ = o.bits_; o.bits_ = MEOW_VALUELESS; return *this;
    }

    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    NaNBoxedVariant& operator=(T&& v) noexcept {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
        return *this;
    }

    [[nodiscard]] std::size_t index() const noexcept {
        if (bits_ == MEOW_VALUELESS) return npos;
        if (is_double(bits_)) return dbl_idx;
        return static_cast<std::size_t>((bits_ >> MEOW_TAG_SHIFT) & 0x7);
    }

    [[nodiscard]] bool valueless() const noexcept { return bits_ == MEOW_VALUELESS; }

    template <typename T>
    [[nodiscard]] bool holds() const noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<std::decay_t<T>, flat_list>::value;
        return index() == idx;
    }

    // --- Fix: Thêm hàm safe_get mà code cũ gọi ---
    template <typename T>
    [[nodiscard]] std::decay_t<T> safe_get() const {
        if (!holds<T>()) throw std::bad_variant_access();
        return decode<std::decay_t<T>>(bits_);
    }

    // --- Fix: Thêm hàm get (unsafe) ---
    template <typename T>
    [[nodiscard]] std::decay_t<T> get() const noexcept {
        return decode<std::decay_t<T>>(bits_);
    }

    // --- Fix: get_if trả về pointer ---
    template <typename T>
    [[nodiscard]] std::decay_t<T>* get_if() noexcept {
        if (!holds<T>()) return nullptr;
        // Hack: Vì primitive types ko có địa chỉ trong nanbox, 
        // trả về địa chỉ của biến static thread_local để thỏa mãn API.
        thread_local static std::decay_t<T> temp; 
        temp = decode<std::decay_t<T>>(bits_);
        return &temp;
    }
    
    template <typename T>
    [[nodiscard]] const std::decay_t<T>* get_if() const noexcept {
        return const_cast<NaNBoxedVariant*>(this)->get_if<T>();
    }

    void swap(NaNBoxedVariant& o) noexcept { std::swap(bits_, o.bits_); }
    bool operator==(const NaNBoxedVariant& o) const { return bits_ == o.bits_; }
    bool operator!=(const NaNBoxedVariant& o) const { return bits_ != o.bits_; }

    template <typename Self, typename Visitor>
    decltype(auto) visit(this Self&& self, Visitor&& vis) {
        std::size_t idx = self.index();
        if (idx == npos) throw std::bad_variant_access();
        return self.visit_impl(std::forward<Visitor>(vis), idx);
    }

private:
    uint64_t bits_;

    template <typename T>
    static uint64_t encode(std::size_t idx, T v) noexcept {
        using U = std::decay_t<T>;
        if constexpr (DoubleLike<U>) {
            uint64_t b = to_bits(static_cast<double>(v));
            if (!is_double(b)) return b; 
            if (b == MEOW_VALUELESS) return b ^ 1;
            return b;
        } else {
            uint64_t payload = 0;
            if constexpr (PointerLike<U>) payload = reinterpret_cast<uintptr_t>(static_cast<const void*>(v));
            else if constexpr (IntegralLike<U>) payload = static_cast<uint64_t>(static_cast<int64_t>(v));
            else if constexpr (BoolLike<U>) payload = v ? 1 : 0;
            return MEOW_QNAN_PREFIX | (static_cast<uint64_t>(idx) << MEOW_TAG_SHIFT) | (payload & MEOW_PAYLOAD_MASK);
        }
    }

    template <typename T>
    static T decode(uint64_t bits) noexcept {
        using U = std::decay_t<T>;
        if constexpr (DoubleLike<U>) return from_bits(bits);
        else if constexpr (IntegralLike<U>) return static_cast<U>(static_cast<int64_t>(bits << 16) >> 16);
        else if constexpr (PointerLike<U>) return reinterpret_cast<U>(static_cast<uintptr_t>(bits & MEOW_PAYLOAD_MASK));
        else if constexpr (BoolLike<U>) return static_cast<U>((bits & MEOW_PAYLOAD_MASK) != 0);
        else return U{};
    }

    template <typename Visitor, std::size_t... Is>
    decltype(auto) visit_table(Visitor&& vis, std::size_t idx, std::index_sequence<Is...>) const {
        using R = std::invoke_result_t<Visitor, typename detail::nth_type<0, flat_list>::type>;
        static constexpr std::array<R (*)(uint64_t, Visitor&&), count> table = {{
            [](uint64_t b, Visitor&& v) -> R {
                return std::forward<Visitor>(v)(decode<typename detail::nth_type<Is, flat_list>::type>(b));
            }...
        }};
        return table[idx](bits_, std::forward<Visitor>(vis));
    }

    template <typename Visitor>
    decltype(auto) visit_impl(Visitor&& vis, std::size_t idx) const {
        return visit_table(std::forward<Visitor>(vis), idx, std::make_index_sequence<count>{});
    }
};

template <typename... Ts> void swap(NaNBoxedVariant<Ts...>& a, NaNBoxedVariant<Ts...>& b) noexcept { a.swap(b); }
}