#pragma once

#include <cstdint>
#include <cstring>
#include <bit> 
#include <meow/common.h>
#include <meow/core/meow_object.h>
#include <meow/value.h>
#include <meow/memory/gc_visitor.h>
#include <meow/core/string.h>
#include <meow_allocator.h> 

namespace meow {

struct Entry {
    string_t first = nullptr;
    Value second;
};

class ObjHashTable : public ObjBase<ObjectType::HASH_TABLE> {
public:
    using Allocator = meow::allocator<Entry>;
private:
    Entry* entries_ = nullptr;
    uint32_t count_ = 0;
    uint32_t capacity_ = 0;
    uint32_t mask_ = 0;
    
    [[no_unique_address]] Allocator allocator_;

    static constexpr double MAX_LOAD_FACTOR = 0.75;
    static constexpr uint32_t MIN_CAPACITY = 8;
public:
    explicit ObjHashTable(Allocator allocator, uint32_t capacity = 0) : allocator_(allocator) {
        if (capacity > 0) allocate(capacity);
    }

    ~ObjHashTable() noexcept override {
        if (entries_) {
            allocator_.deallocate(entries_, capacity_);
        }
    }

    [[gnu::always_inline]] 
    inline Entry* find_entry(Entry* entries, uint32_t mask, string_t key) const noexcept {
        uint32_t index = key->hash() & mask;
        for (;;) {
            Entry* entry = &entries[index];
            if (entry->first == key || entry->first == nullptr) [[likely]] {
                return entry;
            }
            index = (index + 1) & mask;
        }
    }

    [[gnu::always_inline]] 
    inline bool set(string_t key, Value value) noexcept {
        if (count_ + 1 > (capacity_ * MAX_LOAD_FACTOR)) [[unlikely]] {
            grow();
        }

        Entry* entry = find_entry(entries_, mask_, key);
        bool is_new = (entry->first == nullptr);
        
        if (is_new) [[likely]] {
            count_++;
            entry->first = key;
        }
        entry->second = value;
        return is_new;
    }

    [[gnu::always_inline]] 
    inline bool get(string_t key, Value* result) const noexcept {
        if (count_ == 0) [[unlikely]] return false;
        Entry* entry = find_entry(entries_, mask_, key);
        if (entry->first == nullptr) return false;
        *result = entry->second;
        return true;
    }

    [[gnu::always_inline]]
    inline Value get(string_t key) const noexcept {
        if (count_ == 0) return Value(null_t{});
        Entry* entry = find_entry(entries_, mask_, key);
        if (entry->first == nullptr) return Value(null_t{});
        return entry->second;
    }

    [[gnu::always_inline]] 
    inline bool has(string_t key) const noexcept {
        if (count_ == 0) return false;
        return find_entry(entries_, mask_, key)->first != nullptr;
    }

    bool remove(string_t key) noexcept {
        if (count_ == 0) return false;
        Entry* entry = find_entry(entries_, mask_, key);
        if (entry->first == nullptr) return false;

        entry->first = nullptr;
        entry->second = Value(null_t{});
        count_--;

        uint32_t index = (uint32_t)(entry - entries_);
        uint32_t next_index = index;

        for (;;) {
            next_index = (next_index + 1) & mask_;
            Entry* next = &entries_[next_index];
            if (next->first == nullptr) break;

            uint32_t ideal = next->first->hash() & mask_;
            bool shift = false;
            if (index < next_index) {
                if (ideal <= index || ideal > next_index) shift = true;
            } else {
                if (ideal <= index && ideal > next_index) shift = true;
            }

            if (shift) {
                entries_[index] = *next;
                entries_[next_index].first = nullptr;
                entries_[next_index].second = Value(null_t{});
                index = next_index;
            }
        }
        return true;
    }

    // --- Capacity ---
    inline uint32_t size() const noexcept { return count_; }
    inline bool empty() const noexcept { return count_ == 0; }
    inline uint32_t capacity() const noexcept { return capacity_; }

    class Iterator {
        Entry* ptr_; Entry* end_;
    public:
        Iterator(Entry* ptr, Entry* end) : ptr_(ptr), end_(end) {
            while (ptr_ < end_ && ptr_->first == nullptr) ptr_++;
        }
        Iterator& operator++() {
            do { ptr_++; } while (ptr_ < end_ && ptr_->first == nullptr);
            return *this;
        }
        bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }
        
        Entry& operator*() const { return *ptr_; }
        Entry* operator->() const { return ptr_; }
    };

    inline Iterator begin() { return Iterator(entries_, entries_ + capacity_); }
    inline Iterator end() { return Iterator(entries_ + capacity_, entries_ + capacity_); }

    void trace(GCVisitor& visitor) const noexcept override {
        for (uint32_t i = 0; i < capacity_; i++) {
            if (entries_[i].first) {
                visitor.visit_object(entries_[i].first);
                visitor.visit_value(entries_[i].second);
            }
        }
    }

private:
    void allocate(uint32_t capacity) {
        capacity_ = (capacity < MIN_CAPACITY) ? MIN_CAPACITY : std::bit_ceil(capacity);
        mask_ = capacity_ - 1;
        
        entries_ = allocator_.allocate(capacity_);
        std::memset(static_cast<void*>(entries_), 0, sizeof(Entry) * capacity_);
    }

    void grow() {
        uint32_t old_cap = capacity_;
        Entry* old_entries = entries_;

        allocate(old_cap == 0 ? MIN_CAPACITY : old_cap * 2);
        count_ = 0;

        if (old_entries) {
            for (uint32_t i = 0; i < old_cap; i++) {
                if (old_entries[i].first != nullptr) {
                    Entry* dest = find_entry(entries_, mask_, old_entries[i].first);
                    dest->first = old_entries[i].first;
                    dest->second = old_entries[i].second;
                    count_++;
                }
            }
            allocator_.deallocate(old_entries, old_cap);
        }
    }
};

} // namespace meow