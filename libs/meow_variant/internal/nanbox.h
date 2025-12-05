#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant> 
#include <stdexcept>
#include "internal/utils.h"

namespace meow::utils {

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__))
    #define MEOW_LITTLE_64 1
#else
    #define MEOW_LITTLE_64 0
#endif

template <typename T> concept PointerLike = std::is_pointer_v<std::decay_t<T>>;
template <typename T> concept IntegralLike = std::is_integral_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>;
template <typename T> concept DoubleLike = std::is_floating_point_v<std::decay_t<T>>;
template <typename T> concept BoolLike = std::is_same_v<std::decay_t<T>, bool>;

template <typename List> struct all_nanboxable_impl;
template <typename... Ts>
struct all_nanboxable_impl<detail::type_list<Ts...>> {
    static constexpr bool value = ((sizeof(std::decay_t<Ts>) <= 8 && 
        (DoubleLike<Ts> || IntegralLike<Ts> || PointerLike<Ts> || 
         std::is_same_v<std::decay_t<Ts>, std::monostate> || BoolLike<Ts>)) && ...);
};

static constexpr uint64_t MEOW_EXP_MASK     = 0x7FF0000000000000ULL;
static constexpr uint64_t MEOW_QNAN_POS     = 0x7FF8000000000000ULL; 
static constexpr uint64_t MEOW_QNAN_NEG     = 0xFFF8000000000000ULL; 
static constexpr uint64_t MEOW_TAG_MASK     = 0x0007000000000000ULL;
static constexpr uint64_t MEOW_PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;
static constexpr unsigned MEOW_TAG_SHIFT    = 48;
static constexpr uint64_t MEOW_VALUELESS    = 0xFFFFFFFFFFFFFFFFULL;
inline uint64_t to_bits(double d) { return std::bit_cast<uint64_t>(d); }
inline double from_bits(uint64_t u) { return std::bit_cast<double>(u); }
inline bool is_double(uint64_t b) { 
    return (b & MEOW_EXP_MASK) != MEOW_EXP_MASK; 
}

template <typename... Args>
class NaNBoxedVariant {
    using flat_list = meow::utils::flattened_unique_t<Args...>;
    static constexpr std::size_t count = detail::type_list_length<flat_list>::value;
    static constexpr std::size_t dbl_idx = detail::type_list_index_of<double, flat_list>::value;
    static constexpr bool use_extended_tag = (count > 8);

public:
    using inner_types = flat_list;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    NaNBoxedVariant() noexcept {
        if constexpr (std::is_same_v<typename detail::nth_type<0, flat_list>::type, std::monostate>) {
            bits_ = MEOW_QNAN_POS;
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

    // [[nodiscard]] std::size_t index() const noexcept {
    //     if (bits_ == MEOW_VALUELESS) return npos;
    //     if (is_double(bits_)) return dbl_idx;
        
    //     if constexpr (use_extended_tag) {
    //         std::size_t high_bit = (bits_ >> 63); 
    //         std::size_t low_bits = (bits_ >> MEOW_TAG_SHIFT) & 0x7;
    //         return (high_bit * 8) + low_bits;
    //     } else {
    //         return static_cast<std::size_t>((bits_ >> MEOW_TAG_SHIFT) & 0x7);
    //     }
    // }

    [[nodiscard]] std::size_t index() const noexcept {        
        if (is_double(bits_)) return dbl_idx;
        return static_cast<std::size_t>((bits_ >> MEOW_TAG_SHIFT) & 0x7);
    }

    [[nodiscard]] bool valueless() const noexcept { return bits_ == MEOW_VALUELESS; }

    template <typename T>
    [[nodiscard]] bool holds() const noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<std::decay_t<T>, flat_list>::value;
        return index() == idx;
    }

    template <typename T>
    [[nodiscard]] std::decay_t<T> safe_get() const {
        if (!holds<T>()) throw std::bad_variant_access();
        return decode<std::decay_t<T>>(bits_);
    }

    template <typename T>
    [[nodiscard]] std::decay_t<T> unsafe_get() const noexcept {
        return decode<std::decay_t<T>>(bits_);
    }

    template <typename T>
    [[nodiscard]] std::decay_t<T> get() const noexcept {
        return unsafe_get<T>();
    }

    template <typename T>
    [[nodiscard]] std::decay_t<T>* get_if() noexcept {
        if (!holds<T>()) return nullptr;
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

    // template <typename Self, typename Visitor>
    // decltype(auto) visit(this Self&& self, Visitor&& vis) {
    //     std::size_t idx = self.index();
    //     if (idx == npos) throw std::bad_variant_access();
    //     return self.visit_impl(std::forward<Visitor>(vis), idx);
    // }

    template <typename Self, typename Visitor>
    decltype(auto) visit(this Self&& self, Visitor&& vis) {
        if (self.bits_ == MEOW_VALUELESS) [[unlikely]] {
            throw std::bad_variant_access();
        }
        if constexpr (dbl_idx != detail::invalid_index) {
            if (is_double(self.bits_)) [[likely]] {
                return std::forward<Visitor>(vis)(self.template unsafe_get<double>());
            }
        }
        std::size_t tag = (self.bits_ >> MEOW_TAG_SHIFT) & 0x7;

        if constexpr (use_extended_tag) {
            if (self.bits_ & 0x8000000000000000ULL) {
                tag += 8;
            }
        }
        
        return self.visit_tag_dispatch(std::forward<Visitor>(vis), tag);
    }

private:
    uint64_t bits_;

    template <typename T>
    static uint64_t encode(std::size_t idx, T v) noexcept {
        using U = std::decay_t<T>;
        if constexpr (DoubleLike<U>) {
            uint64_t b = to_bits(static_cast<double>(v));
            if (!is_double(b)) return b ^ 1; 
            if (b == MEOW_VALUELESS) return b ^ 1;
            return b;
        } else {
            uint64_t payload = 0;
            if constexpr (PointerLike<U>) payload = reinterpret_cast<uintptr_t>(static_cast<const void*>(v));
            else if constexpr (IntegralLike<U>) payload = static_cast<uint64_t>(static_cast<int64_t>(v));
            else if constexpr (BoolLike<U>) payload = v ? 1 : 0;
            
            if constexpr (use_extended_tag) {
                if (idx < 8) {
                    return MEOW_QNAN_POS | (static_cast<uint64_t>(idx) << MEOW_TAG_SHIFT) | (payload & MEOW_PAYLOAD_MASK);
                } else {
                    return MEOW_QNAN_NEG | (static_cast<uint64_t>(idx - 8) << MEOW_TAG_SHIFT) | (payload & MEOW_PAYLOAD_MASK);
                }
            } else {
                return MEOW_QNAN_POS | (static_cast<uint64_t>(idx) << MEOW_TAG_SHIFT) | (payload & MEOW_PAYLOAD_MASK);
            }
        }
    }

    template <typename T>
    static T decode(uint64_t bits) noexcept {
        using U = std::decay_t<T>;
        if constexpr (DoubleLike<U>) return from_bits(bits);
        else if constexpr (IntegralLike<U>) {
            if constexpr (sizeof(U) <= 4) {
                return static_cast<U>(bits); 
            } else {
                return static_cast<U>(static_cast<int64_t>(bits << 16) >> 16);
            }
        }
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

    template <std::size_t I, typename Visitor>
    __attribute__((always_inline))
    decltype(auto) visit_recursive(Visitor&& vis, std::size_t idx) const {
        if (idx == I) {
            using T = typename detail::nth_type<I, flat_list>::type;
            return std::forward<Visitor>(vis)(decode<T>(bits_));
        }

        if constexpr (I + 1 < count) {
            return visit_recursive<I + 1>(std::forward<Visitor>(vis), idx);
        } else {
            std::unreachable();
        }
    }

    template <std::size_t Begin, std::size_t End, typename Visitor>
    __attribute__((always_inline))
    decltype(auto) visit_binary(Visitor&& vis, std::size_t idx) const {
        if constexpr (Begin + 1 == End) {
            using T = typename detail::nth_type<Begin, flat_list>::type;
            return std::forward<Visitor>(vis)(decode<T>(bits_)); 
        } else {
            constexpr std::size_t Middle = Begin + (End - Begin) / 2;
            if (idx < Middle) {
                return visit_binary<Begin, Middle>(std::forward<Visitor>(vis), idx);
            } else {
                return visit_binary<Middle, End>(std::forward<Visitor>(vis), idx);
            }
        }
    }

    // template <typename Visitor>
    // decltype(auto) visit_impl(Visitor&& vis, std::size_t idx) const {
    //     // return visit_table(std::forward<Visitor>(vis), idx, std::make_index_sequence<count>{});
    //     // return visit_recursive<0>(std::forward<Visitor>(vis), idx);
    //     return visit_binary<0, count>(std::forward<Visitor>(vis), idx);
    // }

    template <typename Visitor>
    __attribute__((always_inline)) // Ép Compiler phải Inline hàm này bằng mọi giá!
    decltype(auto) visit_impl(Visitor&& vis, std::size_t idx) const {
        
        switch (idx) {
            case 0: return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<0, flat_list>::type>());
            
            case 1: 
                if constexpr (count > 1) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<1, flat_list>::type>());
                else std::unreachable();
            case 2: 
                if constexpr (count > 2) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<2, flat_list>::type>());
                else std::unreachable();
            case 3: 
                if constexpr (count > 3) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<3, flat_list>::type>());
                else std::unreachable();
            case 4: 
                if constexpr (count > 4) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<4, flat_list>::type>());
                else std::unreachable();
            case 5: 
                if constexpr (count > 5) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<5, flat_list>::type>());
                else std::unreachable();
            case 6: 
                if constexpr (count > 6) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<6, flat_list>::type>());
                else std::unreachable();
            case 7: 
                if constexpr (count > 7) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<7, flat_list>::type>());
                else std::unreachable();
            case 8: 
                if constexpr (count > 8) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<8, flat_list>::type>());
                else std::unreachable();
            case 9: 
                if constexpr (count > 9) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<9, flat_list>::type>());
                else std::unreachable();
            case 10: 
                if constexpr (count > 10) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<10, flat_list>::type>());
                else std::unreachable();
            case 11: 
                if constexpr (count > 11) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<11, flat_list>::type>());
                else std::unreachable();
            case 12: 
                if constexpr (count > 12) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<12, flat_list>::type>());
                else std::unreachable();
            case 13: 
                if constexpr (count > 13) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<13, flat_list>::type>());
                else std::unreachable();
            case 14: 
                if constexpr (count > 14) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<14, flat_list>::type>());
                else std::unreachable();
            case 15: 
                if constexpr (count > 15) return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<15, flat_list>::type>());
                else std::unreachable();
            
            default: std::unreachable();
        }

        std::unreachable();
        return visit_recursive<0>(std::forward<Visitor>(vis), idx);
    }

    template <typename Visitor>
    __attribute__((always_inline)) 
    decltype(auto) visit_tag_dispatch(Visitor&& vis, std::size_t tag) const {
        if constexpr (count <= 16) {
            switch (tag) {
                #define MEOW_VISIT_CASE(N) \
                case N: { \
                    if constexpr (dbl_idx == N) std::unreachable(); \
                    else { \
                        if constexpr (N < count) \
                            return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<N, flat_list>::type>()); \
                        else std::unreachable(); \
                    } \
                }

                MEOW_VISIT_CASE(0)
                MEOW_VISIT_CASE(1)
                MEOW_VISIT_CASE(2)
                MEOW_VISIT_CASE(3)
                MEOW_VISIT_CASE(4)
                MEOW_VISIT_CASE(5)
                MEOW_VISIT_CASE(6)
                MEOW_VISIT_CASE(7)
                MEOW_VISIT_CASE(8)
                MEOW_VISIT_CASE(9)
                MEOW_VISIT_CASE(10)
                MEOW_VISIT_CASE(11)
                MEOW_VISIT_CASE(12)
                MEOW_VISIT_CASE(13)
                MEOW_VISIT_CASE(14)
                MEOW_VISIT_CASE(15)

                #undef MEOW_VISIT_CASE
                
                default: std::unreachable();
            }
        }
        
        return visit_recursive<0>(std::forward<Visitor>(vis), tag);
    }
};

template <typename... Ts> void swap(NaNBoxedVariant<Ts...>& a, NaNBoxedVariant<Ts...>& b) noexcept { a.swap(b); }
}
