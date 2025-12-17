#pragma once
#include "internal/utils.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory> 
#include <new>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace meow::utils {

template <typename T> concept TrivialCopy = std::is_trivially_copy_constructible_v<T>;
template <typename T> concept TrivialDestruct = std::is_trivially_destructible_v<T>;

// --- Helper Operations Table ---
template <typename... Ts>
struct ops {
    using destroy_fn = void (*)(void*) noexcept;
    using copy_fn    = void (*)(void*, const void*);
    using move_fn    = void (*)(void*, void*);

    template <typename T> static void destroy(void* p) noexcept {
        if constexpr (!TrivialDestruct<T>) reinterpret_cast<T*>(p)->~T();
    }
    template <typename T> static void copy(void* dst, const void* src) {
        std::construct_at(reinterpret_cast<T*>(dst), *reinterpret_cast<const T*>(src));
    }
    template <typename T> static void move(void* dst, void* src) {
        std::construct_at(reinterpret_cast<T*>(dst), std::move(*reinterpret_cast<T*>(src)));
    }

    static constexpr auto destroy_table = std::array{ &destroy<Ts>... };
    static constexpr auto copy_table    = std::array{ &copy<Ts>... };
    static constexpr auto move_table    = std::array{ &move<Ts>... };
};

// --- Destructor Mixin ---
template <typename Derived, bool IsTrivial>
struct DestructorMixin {};

template <typename Derived>
struct DestructorMixin<Derived, true> {
    ~DestructorMixin() = default;
};

template <typename Derived>
struct DestructorMixin<Derived, false> {
    ~DestructorMixin() {
        static_cast<Derived*>(this)->destroy();
    }
};

template <typename... Args>
class FallbackVariant : public DestructorMixin<FallbackVariant<Args...>, (std::is_trivially_destructible_v<Args> && ...)> {
    friend struct DestructorMixin<FallbackVariant<Args...>, false>;

    using flat_list = meow::utils::flattened_unique_t<Args...>;
    static constexpr std::size_t count = detail::type_list_length<flat_list>::value;
    
    using index_t = std::conditional_t<(count < 255), uint8_t, std::size_t>;
    static constexpr index_t npos = static_cast<index_t>(-1);

    // --- Helper Concepts ---
    static constexpr bool AllTrivialCopy = (std::is_trivially_copy_constructible_v<Args> && ...);
    static constexpr bool AllTrivialMove = (std::is_trivially_move_constructible_v<Args> && ...);
    static constexpr bool AllTrivialCopyAssign = (std::is_trivially_copy_assignable_v<Args> && ...);
    static constexpr bool AllTrivialMoveAssign = (std::is_trivially_move_assignable_v<Args> && ...);

public:
    using inner_types = flat_list;
    // --- 1. Interface Consistency: Expose void traits ---
    using layout_traits = void;

    // Default Constructor
    FallbackVariant() noexcept : index_(npos) {}
    FallbackVariant(const FallbackVariant&) requires AllTrivialCopy = default;
    FallbackVariant(const FallbackVariant& o) requires (!AllTrivialCopy) : index_(npos) {
        if (o.index_ != npos) {
            full_ops::copy_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
        }
    }

    // --- MOVE CONSTRUCTOR ---
    FallbackVariant(FallbackVariant&&) requires AllTrivialMove = default;

    FallbackVariant(FallbackVariant&& o) noexcept requires (!AllTrivialMove) : index_(npos) {
        if (o.index_ != npos) {
            full_ops::move_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
            o.index_ = npos;
        }
    }

    // --- COPY ASSIGNMENT ---
    FallbackVariant& operator=(const FallbackVariant&) requires AllTrivialCopyAssign = default;

    FallbackVariant& operator=(const FallbackVariant& o) requires (!AllTrivialCopyAssign) {
        if (this == &o) return *this;
        this->destroy();
        if (o.index_ != npos) {
            full_ops::copy_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
        }
        return *this;
    }

    // --- MOVE ASSIGNMENT ---
    FallbackVariant& operator=(FallbackVariant&&) requires AllTrivialMoveAssign = default;

    FallbackVariant& operator=(FallbackVariant&& o) noexcept requires (!AllTrivialMoveAssign) {
        if (this == &o) return *this;
        this->destroy();
        if (o.index_ != npos) {
            full_ops::move_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
            o.index_ = npos;
        }
        return *this;
    }

    // Value Constructor
    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    FallbackVariant(T&& v) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T&&>) : index_(npos) {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        std::construct_at(reinterpret_cast<U*>(storage_), std::forward<T>(v));
        index_ = static_cast<index_t>(idx);
    }

    // Assignment Operator
    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    FallbackVariant& operator=(T&& v) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T&&>) {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        this->destroy();
        std::construct_at(reinterpret_cast<U*>(storage_), std::forward<T>(v));
        index_ = static_cast<index_t>(idx);
        return *this;
    }

    template <typename Self, typename Visitor>
    decltype(auto) visit(this Self&& self, Visitor&& vis) {
        return self.visit_switch(std::forward<Visitor>(vis));
    }

    uint64_t raw() const noexcept { 
        return 0;
    }

    [[nodiscard]] __attribute__((always_inline))
    static FallbackVariant from_raw([[maybe_unused]] uint64_t bits) noexcept {
        return FallbackVariant();
    }

    std::size_t index() const noexcept { return static_cast<std::size_t>(index_); }
    bool valueless() const noexcept { return index_ == npos; }

    [[nodiscard]] __attribute__((always_inline))
    uint64_t raw_tag() const noexcept {
        return static_cast<uint64_t>(index_);
    }

    __attribute__((always_inline))
    void set_raw(uint64_t) noexcept {}

    template <typename T>
    [[nodiscard]] __attribute__((always_inline))
    bool holds_both(const FallbackVariant& other) const noexcept {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        if constexpr (idx == detail::invalid_index) return false;
        return (index_ == idx) & (other.index_ == idx);
    }

    // --- 2. Type Check Optimized: Integer Comparison ---
    [[nodiscard]] __attribute__((always_inline))
    bool has_same_type_as(const FallbackVariant& other) const noexcept {
        return index_ == other.index_;
    }

    template <typename T>
    bool holds() const noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<std::decay_t<T>, flat_list>::value;
        return index_ == static_cast<index_t>(idx);
    }

    // --- Getters ---
    template <typename T>
    decltype(auto) unsafe_get() const noexcept {
        return *reinterpret_cast<const T*>(storage_);
    }
    template <typename T>
    decltype(auto) unsafe_get() noexcept {
        return *reinterpret_cast<T*>(storage_);
    }
    
    template <typename T> decltype(auto) get() const noexcept { return unsafe_get<T>(); }
    template <typename T> decltype(auto) get() noexcept { return unsafe_get<T>(); }

    template <typename T>
    decltype(auto) safe_get() const {
        if (!holds<T>()) throw std::bad_variant_access();
        return unsafe_get<T>();
    }

    template <typename T>
    T* get_if() noexcept {
        if (holds<T>()) return reinterpret_cast<T*>(storage_);
        return nullptr;
    }
    template <typename T>
    const T* get_if() const noexcept {
        if (holds<T>()) return reinterpret_cast<const T*>(storage_);
        return nullptr;
    }

    void swap(FallbackVariant& other) noexcept {
        FallbackVariant temp = std::move(*this);
        *this = std::move(other);
        other = std::move(temp);
    }

    bool operator==(const FallbackVariant& o) const {
        if (index_ != o.index_) return false;
        if (index_ == npos) return true;
        return std::memcmp(storage_, o.storage_, sizeof(storage_)) == 0; 
    }

private:
    template <typename List> struct ops_resolver;
    template <typename... Ts> struct ops_resolver<detail::type_list<Ts...>> { using type = ops<Ts...>; };
    using full_ops = typename ops_resolver<flat_list>::type;

    template <typename List> struct max_vals;
    template <typename... Ts> struct max_vals<detail::type_list<Ts...>> {
        static constexpr std::size_t size = std::max({sizeof(Ts)...});
        static constexpr std::size_t align = std::max({alignof(Ts)...});
    };
    
    static constexpr std::size_t st_size = max_vals<flat_list>::size;
    static constexpr std::size_t st_align = max_vals<flat_list>::align;
    
    alignas(st_align) unsigned char storage_[st_size > 0 ? st_size : 1];
    index_t index_;

    void destroy() noexcept {
        if (index_ != npos) {
            full_ops::destroy_table[index_](storage_);
            index_ = npos;
        }
    }

    // --- Switch Generation ---
    template <typename Visitor>
    __attribute__((always_inline))
    decltype(auto) visit_switch(Visitor&& vis) const {
        switch (index_) {
            #define MEOW_FB_CASE(N) \
            case N: if constexpr (N < count) \
                return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<N, flat_list>::type>()); \
            else std::unreachable();

            MEOW_FB_CASE(0)  MEOW_FB_CASE(1)  MEOW_FB_CASE(2)  MEOW_FB_CASE(3)
            MEOW_FB_CASE(4)  MEOW_FB_CASE(5)  MEOW_FB_CASE(6)  MEOW_FB_CASE(7)
            MEOW_FB_CASE(8)  MEOW_FB_CASE(9)  MEOW_FB_CASE(10) MEOW_FB_CASE(11)
            MEOW_FB_CASE(12) MEOW_FB_CASE(13) MEOW_FB_CASE(14) MEOW_FB_CASE(15)
            MEOW_FB_CASE(16) MEOW_FB_CASE(17) MEOW_FB_CASE(18) MEOW_FB_CASE(19)
            MEOW_FB_CASE(20) MEOW_FB_CASE(21) MEOW_FB_CASE(22) MEOW_FB_CASE(23)
            MEOW_FB_CASE(24) MEOW_FB_CASE(25) MEOW_FB_CASE(26) MEOW_FB_CASE(27)
            MEOW_FB_CASE(28) MEOW_FB_CASE(29) MEOW_FB_CASE(30) MEOW_FB_CASE(31)

            #undef MEOW_FB_CASE
            
            default: std::unreachable();
        }
    }
    
    template <typename Visitor>
    __attribute__((always_inline))
    decltype(auto) visit_switch(Visitor&& vis) {
        switch (index_) {
            #define MEOW_FB_CASE(N) \
            case N: if constexpr (N < count) \
                return std::invoke(std::forward<Visitor>(vis), unsafe_get<typename detail::nth_type<N, flat_list>::type>()); \
            else std::unreachable();

            MEOW_FB_CASE(0)  MEOW_FB_CASE(1)  MEOW_FB_CASE(2)  MEOW_FB_CASE(3)
            MEOW_FB_CASE(4)  MEOW_FB_CASE(5)  MEOW_FB_CASE(6)  MEOW_FB_CASE(7)
            MEOW_FB_CASE(8)  MEOW_FB_CASE(9)  MEOW_FB_CASE(10) MEOW_FB_CASE(11)
            MEOW_FB_CASE(12) MEOW_FB_CASE(13) MEOW_FB_CASE(14) MEOW_FB_CASE(15)
            MEOW_FB_CASE(16) MEOW_FB_CASE(17) MEOW_FB_CASE(18) MEOW_FB_CASE(19)
            MEOW_FB_CASE(20) MEOW_FB_CASE(21) MEOW_FB_CASE(22) MEOW_FB_CASE(23)
            MEOW_FB_CASE(24) MEOW_FB_CASE(25) MEOW_FB_CASE(26) MEOW_FB_CASE(27)
            MEOW_FB_CASE(28) MEOW_FB_CASE(29) MEOW_FB_CASE(30) MEOW_FB_CASE(31)

            #undef MEOW_FB_CASE
            
            default: std::unreachable();
        }
    }
};

template <typename... Ts> void swap(FallbackVariant<Ts...>& a, FallbackVariant<Ts...>& b) noexcept { a.swap(b); }
}