#pragma once
#include "internal/utils.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory> 
#include <new>
#include <stdexcept>
#include <utility>

namespace meow::utils {

// Check ops trivial
template <typename T> concept TrivialCopy = std::is_trivially_copy_constructible_v<T>;
template <typename T> concept TrivialDestruct = std::is_trivially_destructible_v<T>;

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

template <typename... Args>
class FallbackVariant {
    using flat_list = meow::utils::flattened_unique_t<Args...>;
    static constexpr std::size_t count = detail::type_list_length<flat_list>::value;
    
    using index_t = std::conditional_t<(count <= 0xFF), uint8_t, std::size_t>;
    static constexpr index_t npos = static_cast<index_t>(-1);

public:
    using inner_types = flat_list;

    FallbackVariant() noexcept : index_(npos) {}
    ~FallbackVariant() { destroy(); }

    FallbackVariant(const FallbackVariant& o) : index_(npos) {
        if (o.index_ != npos) {
            full_ops::copy_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
        }
    }

    FallbackVariant(FallbackVariant&& o) noexcept : index_(npos) {
        if (o.index_ != npos) {
            full_ops::move_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
            o.index_ = npos;
        }
    }

    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    FallbackVariant(T&& v) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T&&>) : index_(npos) {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        std::construct_at(reinterpret_cast<U*>(storage_), std::forward<T>(v));
        index_ = static_cast<index_t>(idx);
    }

    FallbackVariant& operator=(const FallbackVariant& o) {
        if (this == &o) return *this;
        destroy();
        if (o.index_ != npos) {
            full_ops::copy_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
        }
        return *this;
    }

    FallbackVariant& operator=(FallbackVariant&& o) noexcept {
        if (this == &o) return *this;
        destroy();
        if (o.index_ != npos) {
            full_ops::move_table[o.index_](storage_, o.storage_);
            index_ = o.index_;
            o.index_ = npos; 
        }
        return *this;
    }

    template <typename Self, typename Visitor>
    decltype(auto) visit(this Self&& self, Visitor&& vis) {
        if (self.index_ == npos) throw std::bad_variant_access();
        return self.visit_impl(std::forward<Visitor>(vis), std::make_index_sequence<count>{});
    }

    std::size_t index() const noexcept { return static_cast<std::size_t>(index_); }
    bool valueless() const noexcept { return index_ == npos; }

    template <typename T>
    bool holds() const noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<std::decay_t<T>, flat_list>::value;
        return index_ == static_cast<index_t>(idx);
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

    // --- Fix: Thêm hàm safe_get ---
    template <typename T>
    decltype(auto) safe_get() const {
        if (!holds<T>()) throw std::bad_variant_access();
        return *reinterpret_cast<const T*>(storage_);
    }

    template <typename T>
    decltype(auto) safe_get() {
        if (!holds<T>()) throw std::bad_variant_access();
        return *reinterpret_cast<T*>(storage_);
    }
    
    // Unsafe get
    template <typename T> decltype(auto) get() const { return *reinterpret_cast<const T*>(storage_); }
    template <typename T> decltype(auto) get() { return *reinterpret_cast<T*>(storage_); }

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

    template <typename Self, typename Visitor, std::size_t... Is>
    decltype(auto) visit_impl(this Self&& self, Visitor&& vis, std::index_sequence<Is...>) {
        using Ret = std::invoke_result_t<Visitor, decltype(*reinterpret_cast<typename detail::nth_type<0, flat_list>::type*>(self.storage_))>;
        using fn_t = Ret (*)(void*, Visitor&&);
        static constexpr std::array<fn_t, count> table = {{
            [](void* ptr, Visitor&& v) -> Ret {
                using T = typename detail::nth_type<Is, flat_list>::type;
                if constexpr (std::is_const_v<std::remove_reference_t<Self>>) {
                    return std::invoke(std::forward<Visitor>(v), *reinterpret_cast<const T*>(ptr));
                } else {
                    return std::invoke(std::forward<Visitor>(v), *reinterpret_cast<T*>(ptr));
                }
            }...
        }};
        return table[self.index_](const_cast<unsigned char*>(self.storage_), std::forward<Visitor>(vis));
    }
};

template <typename... Ts> void swap(FallbackVariant<Ts...>& a, FallbackVariant<Ts...>& b) noexcept { a.swap(b); }
}