#pragma once

#include <cstring>
#include <tuple>
#include <array>
#include <type_traits>
#include <utility>
#include <concepts>
#include <format>

#include "vm/vm_state.h"
#include <meow/bytecode/op_codes.h>
#include <meow/value.h>
#include <meow/cast.h>
#include "runtime/operator_dispatcher.h"
#include "runtime/execution_context.h"
#include "runtime/call_frame.h"
#include <meow/core/function.h>
#include <meow/memory/memory_manager.h>
#include "runtime/upvalue.h"

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i16 = int16_t;
using i64 = int64_t;
using f64 = double;

#define HOT_HANDLER [[gnu::always_inline, gnu::hot, gnu::aligned(32)]] static const uint8_t*

namespace meow::handlers {

const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state);

template <size_t Offset = 0, typename... Args>
[[gnu::cold, gnu::noinline]]
static const uint8_t* ERROR(
    const uint8_t* current_ip, 
    Value* regs, 
    const Value* constants, 
    VMState* state,
    int code, 
    Args&&... args
) {
    const uint8_t* fault_ip = current_ip - Offset - 1; 
    std::string msg;
    if constexpr (sizeof...(Args) > 0) {
        msg = std::format("Error {}: {}", code, std::vformat(std::string_view("..."), std::make_format_args(args...))); 
    } else {
        msg = std::format("Runtime Error Code: {}", code);
    }
    state->error(msg, fault_ip);
    return impl_PANIC(fault_ip, regs, constants, state);
}

namespace decode {

    template <typename... Ts>
    constexpr size_t total_size_v = (sizeof(Ts) + ...);

    template <typename T>
    [[gnu::always_inline, gnu::hot]]
    inline T read_one(const uint8_t*& ip) {
        T val;
        __builtin_memcpy(&val, ip, sizeof(T));
        ip += sizeof(T);
        return val;
    }

    template <typename... Ts> struct ArgPack;
    template <typename T1> struct [[gnu::packed]] ArgPack<T1> { T1 v0; };
    template <typename T1, typename T2> struct [[gnu::packed]] ArgPack<T1, T2> { T1 v0; T2 v1; };
    template <typename T1, typename T2, typename T3> struct [[gnu::packed]] ArgPack<T1, T2, T3> { T1 v0; T2 v1; T3 v2; };
    template <typename T1, typename T2, typename T3, typename T4> struct [[gnu::packed]] ArgPack<T1, T2, T3, T4> { T1 v0; T2 v1; T3 v2; T4 v3; };
    template <typename T1, typename T2, typename T3, typename T4, typename T5> struct [[gnu::packed]] ArgPack<T1, T2, T3, T4, T5> { T1 v0; T2 v1; T3 v2; T4 v3; T5 v4; };

    template <typename... Ts>
    [[gnu::always_inline, gnu::hot]]
    inline auto args(const uint8_t*& ip) {
        using PackedType = ArgPack<Ts...>;
        PackedType val = *reinterpret_cast<const PackedType*>(ip);
        ip += sizeof(PackedType);
        return val;
    }

    template <typename... Ts>
    [[gnu::always_inline, gnu::hot]]
    inline const auto& view(const uint8_t* ip) {
        using PackedType = ArgPack<Ts...>;
        return *reinterpret_cast<const PackedType*>(ip);
    }

    template <typename... Ts>
    [[gnu::always_inline, gnu::hot]]
    inline ArgPack<Ts...> load(const uint8_t* ip) {
        ArgPack<Ts...> val;
        __builtin_memcpy(&val, ip, sizeof(val)); 
        return val;
    }

    template <typename... Ts>
    constexpr size_t size_of_v = sizeof(ArgPack<Ts...>);
    
    template <typename... Ts> struct ArgProxy;

    template <typename T0>
    struct ArgProxy<T0> {
        const uint8_t* ip;
        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip, sizeof(T0)); return v; }
    };

    template <typename T0, typename T1>
    struct ArgProxy<T0, T1> {
        const uint8_t* ip;
        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip, sizeof(T0)); return v; }
        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + sizeof(T0), sizeof(T1)); return v; }
    };

    template <typename T0, typename T1, typename T2>
    struct ArgProxy<T0, T1, T2> {
        const uint8_t* ip;
        static constexpr size_t O1 = sizeof(T0);
        static constexpr size_t O2 = O1 + sizeof(T1);
        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip,      sizeof(T0)); return v; }
        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + O1, sizeof(T1)); return v; }
        [[gnu::always_inline]] T2 v2() const { T2 v; __builtin_memcpy(&v, ip + O2, sizeof(T2)); return v; }
    };

    template <typename T0, typename T1, typename T2, typename T3>
    struct ArgProxy<T0, T1, T2, T3> {
        const uint8_t* ip;
        static constexpr size_t O1 = sizeof(T0);
        static constexpr size_t O2 = O1 + sizeof(T1);
        static constexpr size_t O3 = O2 + sizeof(T2);
        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip,      sizeof(T0)); return v; }
        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + O1, sizeof(T1)); return v; }
        [[gnu::always_inline]] T2 v2() const { T2 v; __builtin_memcpy(&v, ip + O2, sizeof(T2)); return v; }
        [[gnu::always_inline]] T3 v3() const { T3 v; __builtin_memcpy(&v, ip + O3, sizeof(T3)); return v; }
    };
    
    template <typename T0, typename T1, typename T2, typename T3, typename T4>
    struct ArgProxy<T0, T1, T2, T3, T4> {
        const uint8_t* ip;
        static constexpr size_t O1 = sizeof(T0);
        static constexpr size_t O2 = O1 + sizeof(T1);
        static constexpr size_t O3 = O2 + sizeof(T2);
        static constexpr size_t O4 = O3 + sizeof(T3);
        [[gnu::always_inline]] T0 v0() const { T0 v; __builtin_memcpy(&v, ip,      sizeof(T0)); return v; }
        [[gnu::always_inline]] T1 v1() const { T1 v; __builtin_memcpy(&v, ip + O1, sizeof(T1)); return v; }
        [[gnu::always_inline]] T2 v2() const { T2 v; __builtin_memcpy(&v, ip + O2, sizeof(T2)); return v; }
        [[gnu::always_inline]] T3 v3() const { T3 v; __builtin_memcpy(&v, ip + O3, sizeof(T3)); return v; }
        [[gnu::always_inline]] T4 v4() const { T4 v; __builtin_memcpy(&v, ip + O4, sizeof(T4)); return v; }
    };

    template <typename... Ts>
    [[gnu::always_inline]]
    inline ArgProxy<Ts...> make(const uint8_t* ip) {
        return ArgProxy<Ts...>{ip};
    }
    
    template <typename T>
    [[gnu::always_inline]]
    inline const T* as_struct(const uint8_t*& ip) {
        const T* ptr = reinterpret_cast<const T*>(ip);
        ip += sizeof(T);
        return ptr;
    }
} // namespace decode
} // namespace meow::handlers