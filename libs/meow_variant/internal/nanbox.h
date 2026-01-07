#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include "internal/utils.h"

namespace meow::utils {

struct NanboxLayout {
    static constexpr uint64_t EXP_MASK     = 0x7FF0000000000000ULL;
    static constexpr uint64_t TAG_MASK     = 0x0007000000000000ULL;
    static constexpr uint64_t PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;
    static constexpr uint64_t QNAN_POS     = 0x7FF8000000000000ULL; 
    static constexpr uint64_t QNAN_NEG     = 0xFFF8000000000000ULL; 
    static constexpr uint64_t VALUELESS    = 0xFFFFFFFFFFFFFFFFULL;
    static constexpr unsigned TAG_SHIFT    = 48;

    [[nodiscard]] static consteval uint64_t make_tag(uint64_t index) noexcept {
        return QNAN_POS | (index << TAG_SHIFT);
    }
};

template <typename T> concept PointerLike = std::is_pointer_v<std::decay_t<T>>;
template <typename T> concept IntegralLike = std::is_integral_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>;
template <typename T> concept DoubleLike = std::is_floating_point_v<std::decay_t<T>>;
template <typename T> concept BoolLike = std::is_same_v<std::decay_t<T>, bool>;

template <typename List> struct all_nanboxable_impl;
template <typename... Ts>
struct all_nanboxable_impl<detail::type_list<Ts...>> {
    static constexpr bool value = ((sizeof(std::decay_t<Ts>) <= 8 && 
        (DoubleLike<Ts> || IntegralLike<Ts> || PointerLike<Ts> || 
         std::is_same_v<std::decay_t<Ts>, meow::monostate> || BoolLike<Ts>)) && ...);
};

using Layout = NanboxLayout;

[[gnu::always_inline]] inline uint64_t to_bits(double d) noexcept { return std::bit_cast<uint64_t>(d); }
[[gnu::always_inline]] inline double from_bits(uint64_t u) noexcept { return std::bit_cast<double>(u); }

[[gnu::always_inline]] inline bool is_double(uint64_t b) noexcept { 
    return (b & Layout::EXP_MASK) != Layout::EXP_MASK; 
}

template <typename... Args>
class NaNBoxedVariant {
    using flat_list = meow::utils::flattened_unique_t<Args...>;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    static constexpr std::size_t count = detail::type_list_length<flat_list>::value;
    static constexpr std::size_t dbl_idx = detail::type_list_index_of<double, flat_list>::value;
    static constexpr bool use_extended_tag = (count > 8);
    static constexpr bool has_double = (dbl_idx != npos);
    static constexpr std::size_t num_tagged = count - (has_double ? 1 : 0);

public:
    using inner_types = flat_list;
    using layout_traits = NanboxLayout;

    NaNBoxedVariant() noexcept {
        if constexpr (std::is_same_v<typename detail::nth_type<0, flat_list>::type, meow::monostate>) {
            bits_ = Layout::QNAN_POS;
        } else {
            bits_ = Layout::VALUELESS;
        }
    }

    NaNBoxedVariant(const NaNBoxedVariant&) = default;
    NaNBoxedVariant(NaNBoxedVariant&& other) noexcept : bits_(other.bits_) { other.bits_ = Layout::VALUELESS; }

    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    NaNBoxedVariant(T&& v) noexcept {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
    }

    NaNBoxedVariant& operator=(const NaNBoxedVariant&) = default;
    NaNBoxedVariant& operator=(NaNBoxedVariant&& o) noexcept {
        bits_ = o.bits_; o.bits_ = Layout::VALUELESS; return *this;
    }

    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    NaNBoxedVariant& operator=(T&& v) noexcept {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
        return *this;
    }

    [[nodiscard]] [[gnu::always_inline]]

    std::size_t index() const noexcept {        
        if (is_double(bits_)) return dbl_idx;
        std::size_t tag = (bits_ >> Layout::TAG_SHIFT) & 0x7;
        if constexpr (use_extended_tag) {
            if (static_cast<int64_t>(bits_) < 0) tag += 8; 
        }
        return tag;
    }

    [[nodiscard]] [[gnu::always_inline]]
    uint64_t raw_tag() const noexcept {
        return bits_ & Layout::TAG_MASK;
    }

    [[gnu::always_inline]]
    void set_raw(uint64_t raw_bits) noexcept {
        bits_ = raw_bits;
    }

    template <typename T>
    [[nodiscard]] [[gnu::always_inline]]
    bool holds_both(const NaNBoxedVariant& other) const noexcept {
        using U = std::decay_t<T>;
        
        if constexpr (DoubleLike<U>) {
            return ((bits_ & Layout::EXP_MASK) != Layout::EXP_MASK) & 
                   ((other.bits_ & Layout::EXP_MASK) != Layout::EXP_MASK);
        }
        else {
            constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
            constexpr uint64_t header_mask = 0xFFFF000000000000ULL; 
            constexpr uint64_t target_tag = []() consteval {
                if constexpr (use_extended_tag && idx >= 8) {
                    return Layout::QNAN_NEG | (static_cast<uint64_t>(idx - 8) << Layout::TAG_SHIFT);
                } else {
                    return Layout::QNAN_POS | (static_cast<uint64_t>(idx) << Layout::TAG_SHIFT);
                }
            }();
            
            return ((bits_ & header_mask) == target_tag) & 
                   ((other.bits_ & header_mask) == target_tag);
        }
    }

    [[nodiscard]] [[gnu::always_inline]] 
    bool has_same_type_as(const NaNBoxedVariant& other) const noexcept {
        const uint64_t a = bits_;
        const uint64_t b = other.bits_;
        const bool a_is_nan = (a & Layout::EXP_MASK) == Layout::EXP_MASK;
        const bool b_is_nan = (b & Layout::EXP_MASK) == Layout::EXP_MASK;
        const bool same_category = (a_is_nan == b_is_nan);
        const bool same_tag = (a & Layout::TAG_MASK) == (b & Layout::TAG_MASK);
        return same_category & ((!a_is_nan) | same_tag);
    }

    [[nodiscard]] [[gnu::always_inline]] 
    uint64_t raw() const noexcept { return bits_; }

    [[nodiscard]] [[gnu::always_inline]]
    static NaNBoxedVariant from_raw(uint64_t raw_bits) noexcept {
        NaNBoxedVariant v;
        v.bits_ = raw_bits;
        return v;
    }

    [[nodiscard]] bool valueless() const noexcept { return bits_ == Layout::VALUELESS; }

    template <typename T>
    [[nodiscard]] [[gnu::always_inline]]
    bool holds() const noexcept {
        using U = std::decay_t<T>;
        
        if constexpr (DoubleLike<U>) {
            return is_double(bits_);
        } else {
            constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
            
            constexpr uint64_t header_mask = 0xFFFF000000000000ULL; 
            constexpr uint64_t target_tag = []() consteval {
                if constexpr (use_extended_tag && idx >= 8) {
                    return Layout::QNAN_NEG | (static_cast<uint64_t>(idx - 8) << Layout::TAG_SHIFT);
                } else {
                    return Layout::QNAN_POS | (static_cast<uint64_t>(idx) << Layout::TAG_SHIFT);
                }
            }();
            
            return (bits_ & header_mask) == target_tag;
        }
    }

    template <typename T>
    [[nodiscard]] std::decay_t<T> safe_get() const {
        // if (!holds<T>()) [[unlikely]] throw std::bad_variant_access();
        if (!holds<T>()) [[unlikely]] std::abort();
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

    template <typename T>
    [[gnu::always_inline]]
    void unsafe_set(T&& v) noexcept {
        using U = std::decay_t<T>;
        
        if constexpr (DoubleLike<U>) {
             bits_ = to_bits(static_cast<double>(v));
        } 
        else {
            constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
            constexpr uint64_t tag = []() consteval {
                if constexpr (use_extended_tag && idx >= 8) {
                    return Layout::QNAN_NEG | (static_cast<uint64_t>(idx - 8) << Layout::TAG_SHIFT);
                } else {
                    return Layout::QNAN_POS | (static_cast<uint64_t>(idx) << Layout::TAG_SHIFT);
                }
            }();

            uint64_t payload = 0;
            if constexpr (PointerLike<U>) {
                payload = reinterpret_cast<uintptr_t>(v);
            } else if constexpr (IntegralLike<U>) {
                payload = static_cast<uint64_t>(static_cast<int64_t>(v));
            } else if constexpr (BoolLike<U>) {
                payload = v ? 1 : 0;
            }

            bits_ = tag | (payload & Layout::PAYLOAD_MASK);
        }
    }

    template <typename T>
    [[nodiscard]] [[gnu::always_inline]]
    T get_as() const noexcept {
        static_assert(sizeof(T) <= 8, "Type T must fit in 64-bit storage");
        static_assert(std::is_trivially_copyable_v<T>, "Type T must be trivially copyable");
        
        T ret{};
        std::memcpy(&ret, &bits_, sizeof(T));
        return ret;
    }

    template <typename T>
    [[gnu::always_inline]]
    uint64_t set_as(T val) noexcept {
        static_assert(sizeof(T) <= 8, "Type T must fit in 64-bit storage");
        static_assert(std::is_trivially_copyable_v<T>, "Type T must be trivially copyable");

        uint64_t old_bits = bits_;
        
        bits_ = 0; 
        std::memcpy(&bits_, &val, sizeof(T));
        
        return old_bits;
    }

    void swap(NaNBoxedVariant& other) noexcept { std::swap(bits_, other.bits_); }
    bool operator==(const NaNBoxedVariant& other) const { return bits_ == other.bits_; }
    bool operator!=(const NaNBoxedVariant& other) const { return bits_ != other.bits_; }

    template <typename Self, typename Visitor>
    [[gnu::always_inline]]
    decltype(auto) visit(this Self&& self, Visitor&& vis) {
        if constexpr (has_double) {
             if (is_double(self.bits_)) {
                 return std::invoke(std::forward<Visitor>(vis), from_bits(self.bits_));
             }
        }

        if constexpr (num_tagged == 1) {
            constexpr std::size_t idx = []() consteval {
                for(std::size_t i=0; i<count; ++i) if(i != dbl_idx) return i;
                return std::size_t(0); 
            }();
            using T = typename detail::nth_type<idx, flat_list>::type;
            
            return std::invoke(std::forward<Visitor>(vis), self.template unsafe_get<T>());
        }
        else {
            uint32_t tag = (self.bits_ >> Layout::TAG_SHIFT) & 0x7;
            
            if constexpr (use_extended_tag) {
                 if (self.bits_ & 0x8000000000000000ULL) tag += 8;
            }
            
            return self.visit_impl(std::forward<Visitor>(vis), tag);
        }
    }

private:
    uint64_t bits_;

    template <typename T>
    [[gnu::always_inline]]
    static uint64_t encode(std::size_t idx, T v) noexcept {
        using U = std::decay_t<T>;
        
        if constexpr (DoubleLike<U>) {
            uint64_t b = to_bits(static_cast<double>(v));
            if (!is_double(b)) return b ^ 1; 
            if (b == Layout::VALUELESS) return b ^ 1;
            return b;
        } 
        else {
            uint64_t payload = 0;
            
            if constexpr (PointerLike<U>) {
                payload = reinterpret_cast<uintptr_t>(v);
            }
            else if constexpr (IntegralLike<U>) {
                payload = static_cast<uint64_t>(static_cast<int64_t>(v));
            }
            else if constexpr (BoolLike<U>) {
                payload = v ? 1 : 0;
            }
            if constexpr (use_extended_tag) {
                if (idx < 8) {
                    return Layout::QNAN_POS | (static_cast<uint64_t>(idx) << Layout::TAG_SHIFT) | (payload & Layout::PAYLOAD_MASK);
                } else {
                    return Layout::QNAN_NEG | (static_cast<uint64_t>(idx - 8) << Layout::TAG_SHIFT) | (payload & Layout::PAYLOAD_MASK);
                }
            } else {
                return Layout::QNAN_POS | (static_cast<uint64_t>(idx) << Layout::TAG_SHIFT) | (payload & Layout::PAYLOAD_MASK);
            }
        }
    }

    template <typename T>
    [[gnu::always_inline]]
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
        else if constexpr (PointerLike<U>) return reinterpret_cast<U>(static_cast<uintptr_t>(bits & Layout::PAYLOAD_MASK));
        else if constexpr (BoolLike<U>) return static_cast<U>((bits & Layout::PAYLOAD_MASK) != 0);
        else return U{};
    }

    template <std::size_t I, typename Visitor>
    [[gnu::always_inline, gnu::hot]]
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

    template <typename Visitor>
    [[gnu::always_inline, gnu::hot]]
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
};

template <typename... Ts> void swap(NaNBoxedVariant<Ts...>& a, NaNBoxedVariant<Ts...>& b) noexcept { a.swap(b); }
}