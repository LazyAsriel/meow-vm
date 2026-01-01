#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <cstring>
#include <format>
#include <utility>
#include <bit>
#include <concepts>
#include <functional>
#include <type_traits>
#include <variant>

// C++23/26 Compatibility macros
#if __cplusplus >= 202302L
    #define MEOW_CONSTEXPR constexpr
#else
    #define MEOW_CONSTEXPR inline
#endif

namespace meow {

// ============================================================================
// [CORE] Script Registry (String Interning)
// Thread-safe, Read-optimized
// ============================================================================
class ScriptRegistry {
public:
    static ScriptRegistry& get() noexcept {
        static ScriptRegistry instance;
        return instance;
    }

    // Hot path: Lock-free read if possible via shared_lock
    [[gnu::hot]] [[nodiscard]]
    std::string_view get_name(uint16_t id) const noexcept {
        if (id == 0xFFFF) return "<unknown>";
        std::shared_lock lock(mtx_);
        if (id < ids_.size()) [[likely]] {
            return ids_[id];
        }
        return "<invalid>";
    }

    // Cold path: Write lock
    [[gnu::cold]]
    uint16_t register_file(std::string_view filename) {
        if (filename.empty()) return 0xFFFF;
        
        // Optimistic read
        {
            std::shared_lock lock(mtx_);
            if (auto it = lookup_.find(filename); it != lookup_.end()) [[likely]] {
                return it->second;
            }
        }

        // Write
        std::unique_lock lock(mtx_);
        // Double-check locking pattern
        if (auto it = lookup_.find(filename); it != lookup_.end()) return it->second;

        if (ids_.size() >= 0xFFFE) [[unlikely]] return 0xFFFF; // Max uint16 check

        size_t len = filename.length();
        if (current_offset_ + len > BLOCK_SIZE) [[unlikely]] {
            allocate_block();
        }

        char* dest = current_block_ + current_offset_;
        std::memcpy(dest, filename.data(), len);
        
        std::string_view stored_view(dest, len);
        current_offset_ += len;

        uint16_t id = static_cast<uint16_t>(ids_.size());
        ids_.push_back(stored_view);
        lookup_.emplace(stored_view, id);
        
        return id;
    }

private:
    ScriptRegistry() {
        allocate_block();
        ids_.reserve(512);
        lookup_.reserve(512);
    }
    
    // Non-copyable/movable (Singleton)
    ScriptRegistry(const ScriptRegistry&) = delete;
    ScriptRegistry& operator=(const ScriptRegistry&) = delete;

    void allocate_block() {
        auto ptr = std::make_unique<char[]>(BLOCK_SIZE);
        current_block_ = ptr.get();
        current_offset_ = 0;
        blocks_.emplace_back(std::move(ptr));
    }

    static constexpr size_t BLOCK_SIZE = 8192; // 8KB pages

    std::vector<std::unique_ptr<char[]>> blocks_;
    char* current_block_ = nullptr;
    size_t current_offset_ = 0;

    std::vector<std::string_view> ids_;
    std::unordered_map<std::string_view, uint16_t> lookup_;
    
    mutable std::shared_mutex mtx_;
};

// ============================================================================
// [CORE] Status (Bit-packed Error Type)
// Layout: [FileID:16][Line:24][Col:16][Code:8] = 64 bits
// ============================================================================

template <typename E>
requires std::is_enum_v<E>
struct [[nodiscard]] Status {
    uint64_t raw;
    
    [[gnu::always_inline]] constexpr Status() noexcept : raw(0) {}
    [[gnu::always_inline]] explicit constexpr Status(uint64_t v) noexcept : raw(v) {}

    [[gnu::always_inline]] [[gnu::const]]
    static constexpr Status make(E code, uint16_t fid, uint32_t line, uint32_t col) noexcept {
        return Status(
            (static_cast<uint64_t>(code) & 0xFFULL) |
            ((static_cast<uint64_t>(col) & 0xFFFFULL) << 8) |
            ((static_cast<uint64_t>(line) & 0xFFFFFFULL) << 24) |
            ((static_cast<uint64_t>(fid) & 0xFFFFULL) << 48)
        );
    }
    
    // Quick helpers
    static constexpr Status ok() noexcept { return Status(0); }

    [[gnu::always_inline]] constexpr bool is_ok() const noexcept { return raw == 0; }
    [[gnu::always_inline]] constexpr bool is_err() const noexcept { return raw != 0; }
    [[gnu::always_inline]] explicit constexpr operator bool() const noexcept { return is_ok(); }

    [[gnu::always_inline]] constexpr E code() const noexcept { 
        return static_cast<E>(raw & 0xFF); 
    }

    // Unpack helpers (Cold paths usually)
    [[gnu::cold]] std::string_view filename() const noexcept {
        if (is_ok()) return "";
        uint16_t fid = static_cast<uint16_t>((raw >> 48) & 0xFFFF);
        return ScriptRegistry::get().get_name(fid);
    }

    constexpr uint32_t line() const noexcept { return static_cast<uint32_t>((raw >> 24) & 0xFFFFFF); }
    constexpr uint32_t col() const noexcept  { return static_cast<uint32_t>((raw >> 8) & 0xFFFF); }
    
    constexpr bool operator==(const Status& other) const = default;
};

// ============================================================================
// [CORE] Context (Parser state tracker)
// ============================================================================
struct Context {
    uint16_t file_id = 0xFFFF;
    uint32_t line    = 1;
    uint32_t col     = 0;

    void load(std::string_view filename) {
        file_id = ScriptRegistry::get().register_file(filename);
        line = 1;
        col = 0;
    }

    constexpr void advance_line() noexcept { line++; col = 0; }
    constexpr void advance_col(uint32_t n = 1) noexcept { col += n; }

    template <typename E>
    [[gnu::always_inline]] Status<E> error(E code) const noexcept {
        return Status<E>::make(code, file_id, line, col);
    }
};

// ============================================================================
// [API] Result (Monadic, Zero-exception wrapper)
// Replaces std::expected for our specific packed Status
// ============================================================================

template <typename T, typename E>
requires std::is_enum_v<E>
class [[nodiscard]] Result {
    union {
        T value_;
        Status<E> error_;
    };
    bool has_value_;

public:
    // Constructors
    constexpr Result(T&& val) : value_(std::move(val)), has_value_(true) {}
    constexpr Result(const T& val) : value_(val), has_value_(true) {}
    constexpr Result(Status<E> err) : error_(err), has_value_(false) {}
    
    // Destructor (handles T's lifetime)
    constexpr ~Result() {
        if (has_value_) std::destroy_at(&value_);
    }

    // Copy/Move semantics (Simplified for brevity, proper impl needs check)
    constexpr Result(Result&& other) noexcept(std::is_nothrow_move_constructible_v<T>) 
        : has_value_(other.has_value_) {
        if (has_value_) std::construct_at(&value_, std::move(other.value_));
        else error_ = other.error_;
    }

    // Observers
    [[gnu::always_inline]] constexpr bool ok() const noexcept { return has_value_; }
    [[gnu::always_inline]] constexpr bool failed() const noexcept { return !has_value_; }
    
    // Accessors
    constexpr T& value() & { 
        if (!has_value_) [[unlikely]] std::abort(); // Or generic panic
        return value_; 
    }
    constexpr const T& value() const & { 
        if (!has_value_) [[unlikely]] std::abort();
        return value_; 
    }
    constexpr Status<E> error() const noexcept { 
        return has_value_ ? Status<E>::ok() : error_; 
    }

    // --- Monadic Operations (C++23 style) ---

    // .transform(F): Maps T -> U if ok, keeps Error if failed
    template <typename F>
    constexpr auto transform(F&& f) const & {
        using U = std::invoke_result_t<F, const T&>;
        if (has_value_) {
            return Result<U, E>(std::invoke(std::forward<F>(f), value_));
        }
        return Result<U, E>(error_);
    }

    // .and_then(F): Maps T -> Result<U, E> (FlatMap)
    template <typename F>
    constexpr auto and_then(F&& f) const & {
        using U = typename std::invoke_result_t<F, const T&>; 
        // U must be Result<Something, E>
        if (has_value_) {
            return std::invoke(std::forward<F>(f), value_);
        }
        return U(error_);
    }

    // .or_else(F): Handles error, returns Result<T, E>
    template <typename F>
    constexpr Result<T, E> or_else(F&& f) const & {
        if (!has_value_) {
            return std::invoke(std::forward<F>(f), error_);
        }
        return *this;
    }
    
    // Unpack with default
    constexpr T value_or(T&& def) const & {
        return has_value_ ? value_ : std::forward<T>(def);
    }
};

template <typename E>
requires std::is_enum_v<E>
class [[nodiscard]] Result<void, E> {
    Status<E> error_;
public:
    constexpr Result() : error_(Status<E>::ok()) {}
    constexpr Result(Status<E> err) : error_(err) {}
    
    [[gnu::always_inline]] constexpr bool ok() const noexcept { return error_.is_ok(); }
    [[gnu::always_inline]] constexpr bool failed() const noexcept { return error_.is_err(); }
    constexpr Status<E> error() const noexcept { return error_; }
    
    template <typename F>
    constexpr auto and_then(F&& f) const {
        using U = std::invoke_result_t<F>;
        if (ok()) return std::invoke(std::forward<F>(f));
        return U(error_);
    }
};

} // namespace meow

template <typename E>
struct std::formatter<meow::Status<E>> : std::formatter<std::string> {
    auto format(const meow::Status<E>& s, format_context& ctx) const {
        if (s.is_ok()) return std::format_to(ctx.out(), "OK");
        
        return std::format_to(ctx.out(), "[{}:{}:{}] Error({})", 
            s.filename(), 
            s.line(), 
            s.col(), 
            static_cast<int>(s.code()));
    }
};