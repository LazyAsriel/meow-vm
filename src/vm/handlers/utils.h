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

    // =========================================================
    // 1. WIRE FORMAT (PACKED)
    // Dùng để ánh xạ dữ liệu thô từ Bytecode (Memory)
    // =========================================================
    template <typename... Ts> struct [[gnu::packed]] WireArgs;

    template <typename T1> struct [[gnu::packed]] WireArgs<T1> { T1 v0; };
    template <typename T1, typename T2> struct [[gnu::packed]] WireArgs<T1, T2> { T1 v0; T2 v1; };
    template <typename T1, typename T2, typename T3> struct [[gnu::packed]] WireArgs<T1, T2, T3> { T1 v0; T2 v1; T3 v2; };
    template <typename T1, typename T2, typename T3, typename T4> struct [[gnu::packed]] WireArgs<T1, T2, T3, T4> { T1 v0; T2 v1; T3 v2; T4 v3; };
    template <typename T1, typename T2, typename T3, typename T4, typename T5> struct [[gnu::packed]] WireArgs<T1, T2, T3, T4, T5> { T1 v0; T2 v1; T3 v2; T4 v3; T5 v4; };

    // =========================================================
    // 2. REGISTER FORMAT (UNPACKED)
    // Dùng để trả về giá trị (Register friendly -> Fast access)
    // =========================================================
    template <typename... Ts> struct ArgPack;

    template <typename T1> 
    struct ArgPack<T1> { 
        T1 v0; 
        // Constructor copy từ Wire sang Register
        [[gnu::always_inline]] ArgPack(const WireArgs<T1>& w) : v0(w.v0) {}
    };

    template <typename T1, typename T2> 
    struct ArgPack<T1, T2> { 
        T1 v0; T2 v1;
        [[gnu::always_inline]] ArgPack(const WireArgs<T1, T2>& w) : v0(w.v0), v1(w.v1) {}
    };

    template <typename T1, typename T2, typename T3> 
    struct ArgPack<T1, T2, T3> { 
        T1 v0; T2 v1; T3 v2;
        [[gnu::always_inline]] ArgPack(const WireArgs<T1, T2, T3>& w) : v0(w.v0), v1(w.v1), v2(w.v2) {}
    };

    template <typename T1, typename T2, typename T3, typename T4> 
    struct ArgPack<T1, T2, T3, T4> { 
        T1 v0; T2 v1; T3 v2; T4 v3;
        [[gnu::always_inline]] ArgPack(const WireArgs<T1, T2, T3, T4>& w) : v0(w.v0), v1(w.v1), v2(w.v2), v3(w.v3) {}
    };

    template <typename T1, typename T2, typename T3, typename T4, typename T5> 
    struct ArgPack<T1, T2, T3, T4, T5> { 
        T1 v0; T2 v1; T3 v2; T4 v3; T5 v4;
        [[gnu::always_inline]] ArgPack(const WireArgs<T1, T2, T3, T4, T5>& w) : v0(w.v0), v1(w.v1), v2(w.v2), v3(w.v3), v4(w.v4) {}
    };

    // =========================================================
    // 3. CORE DECODER
    // =========================================================

    template <typename... Ts>
    constexpr size_t total_size_v = sizeof(WireArgs<Ts...>); // Dùng WireArgs để tính size chuẩn

    template <typename... Ts>
    constexpr size_t size_of_v = sizeof(WireArgs<Ts...>);

    template <typename... Ts>
    [[gnu::always_inline, gnu::hot]]
    inline auto args(const uint8_t*& ip) {
        using WireType = WireArgs<Ts...>;
        using RetType  = ArgPack<Ts...>;
        
        // 1. Load thô từ bộ nhớ (Packed) -> Compiler biến thành MOV
        WireType wire;
        __builtin_memcpy(&wire, ip, sizeof(WireType));
        
        // 2. Tăng IP theo kích thước Packed (Logic cũ, không phá API)
        ip += sizeof(WireType);
        
        // 3. Trả về dạng Unpacked (Constructor sẽ copy sang thanh ghi)
        // Nhờ inlining, bước copy này "tan biến", chỉ còn lại dữ liệu trên thanh ghi.
        return RetType(wire);
    }

    // Helper cho cấu trúc struct IC, v.v.
    template <typename T>
    [[gnu::always_inline]]
    inline const T* as_struct(const uint8_t*& ip) {
        const T* ptr = reinterpret_cast<const T*>(ip);
        ip += sizeof(T);
        return ptr;
    }

    // Proxy (Giữ lại nếu cần, nhưng args giờ đã max speed)
    template <typename... Ts>
    [[gnu::always_inline]]
    inline auto make(const uint8_t* ip) {
        // Lưu ý: proxy giữ nguyên logic cũ, view vào packed memory
        return reinterpret_cast<const WireArgs<Ts...>*>(ip);
    }
    
} // namespace decode
} // namespace meow::handlers