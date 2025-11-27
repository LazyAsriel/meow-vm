/**
 * @file string.h
 * @author LazyPaws
 * @brief Core definition of String in TrangMeo (C++23 Refactored)
 */

#pragma once

#include "common/pch.h"
#include "core/meow_object.h"

namespace meow {
class ObjString : public ObjBase<ObjectType::STRING> {
private:
    using storage_t = std::string;
    using visitor_t = GCVisitor;
    storage_t data_;
public:
    // --- Constructors & destructor ---
    ObjString() = default;
    explicit ObjString(const storage_t& data) : data_(data) {}
    explicit ObjString(storage_t&& data) noexcept : data_(std::move(data)) {}
    explicit ObjString(const char* data) : data_(data) {}

    // --- Rule of 5 ---
    ObjString(const ObjString&) = delete;
    ObjString(ObjString&&) = default;
    ObjString& operator=(const ObjString&) = delete;
    ObjString& operator=(ObjString&&) = delete;
    ~ObjString() override = default;

    // --- Assignments ---
    inline ObjString operator+(const ObjString& other) const noexcept {
        return ObjString(data_ + other.data_);
    }
    // (Giữ nguyên operator!...)
    inline ObjString operator!() const noexcept {
        std::string temp(data_.size(), '\0');
        for (size_t i = 0; i < data_.size(); ++i) {
            temp[i] = 127 - data_[i];
        }
        return ObjString(temp);
    }

    // --- Iterator types ---
    using const_iterator = storage_t::const_iterator;
    using const_reverse_iterator = storage_t::const_reverse_iterator;
    // Thêm iterator non-const nếu muốn sửa nội dung (dù string thường immutable)
    using iterator = storage_t::iterator; 

    // --- Character access (C++23) ---
    // template <typename Self>
    // inline decltype(auto) get(this Self&& self, size_t index) noexcept {
    //     return std::forward<Self>(self).data_[index];
    // }

    inline char get(size_t index) const noexcept {
        return data_[index];
    }
    
    template <typename Self>
    inline decltype(auto) at(this Self&& self, size_t index) {
        return std::forward<Self>(self).data_.at(index);
    }

    // --- String access ---
    inline const char* c_str() const noexcept { return data_.c_str(); }

    // --- Capacity ---
    inline size_t size() const noexcept { return data_.size(); }
    inline bool empty() const noexcept { return data_.empty(); }

    // --- Iterators (C++23 Deducing this) ---
    
    template <typename Self>
    inline auto begin(this Self&& self) noexcept { return std::forward<Self>(self).data_.begin(); }
    
    template <typename Self>
    inline auto end(this Self&& self) noexcept { return std::forward<Self>(self).data_.end(); }
    
    template <typename Self>
    inline auto rbegin(this Self&& self) noexcept { return std::forward<Self>(self).data_.rbegin(); }
    
    template <typename Self>
    inline auto rend(this Self&& self) noexcept { return std::forward<Self>(self).data_.rend(); }

    inline void trace(visitor_t&) const noexcept override {}
};
}