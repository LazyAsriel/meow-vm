/**
 * @file function.h
 * @author LazyPaws
 * @brief Core definition of Upvalue, Proto, Function in TrangMeo
 * @copyright Copyright (c) 2025 LazyPaws
 * @license All rights reserved. Unauthorized copying of this file, in any form
 * or medium, is strictly prohibited
 */

#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/meow_object.h"
#include "common/definitions.h"
#include "core/value.h"
#include "memory/gc_visitor.h"
#include "bytecode/chunk.h"

namespace meow {
struct UpvalueDesc {
    bool is_local_;
    size_t index_;
    UpvalueDesc(bool is_local = false, size_t index = 0) noexcept : is_local_(is_local), index_(index) {
    }
};

class ObjUpvalue : public ObjBase<ObjectType::UPVALUE> {
   private:
    using visitor_t = GCVisitor;

    enum class State { OPEN, CLOSED };
    State state_ = State::OPEN;
    size_t index_ = 0;
    Value closed_ = null_t{};

   public:
    explicit ObjUpvalue(size_t index = 0) noexcept : index_(index) {
    }
    inline void close(param_t value) noexcept {
        closed_ = value;
        state_ = State::CLOSED;
    }
    inline bool is_closed() const noexcept {
        return state_ == State::CLOSED;
    }
    inline return_t get_value() const noexcept {
        return closed_;
    }
    inline size_t get_index() const noexcept {
        return index_;
    }

    void trace(visitor_t& visitor) const noexcept override;
};

class ObjFunctionProto : public ObjBase<ObjectType::PROTO> {
   private:
    using chunk_t = Chunk;
    using string_t = string_t;
    using visitor_t = GCVisitor;

    size_t num_registers_;
    size_t num_upvalues_;
    string_t name_;
    chunk_t chunk_;
    std::vector<UpvalueDesc> upvalue_descs_;

   public:
    explicit ObjFunctionProto(size_t registers, size_t upvalues, string_t name, chunk_t&& chunk) noexcept : num_registers_(registers), num_upvalues_(upvalues), name_(name), chunk_(std::move(chunk)) {
    }
    explicit ObjFunctionProto(size_t registers, size_t upvalues, string_t name, chunk_t&& chunk, std::vector<UpvalueDesc>&& descs) noexcept
        : num_registers_(registers), num_upvalues_(upvalues), name_(name), chunk_(std::move(chunk)), upvalue_descs_(std::move(descs)) {
    }

    /// @brief Unchecked upvalue desc access. For performance-critical code
    inline const UpvalueDesc& get_desc(size_t index) const noexcept {
        return upvalue_descs_[index];
    }
    /// @brief Checked upvalue desc access. For performence-critical code
    inline const UpvalueDesc& at_desc(size_t index) const {
        return upvalue_descs_.at(index);
    }
    inline size_t get_num_registers() const noexcept {
        return num_registers_;
    }
    inline size_t get_num_upvalues() const noexcept {
        return num_upvalues_;
    }
    inline string_t get_name() const noexcept {
        return name_;
    }
    inline const chunk_t& get_chunk() const noexcept {
        return chunk_;
    }
    inline size_t desc_size() const noexcept {
        return upvalue_descs_.size();
    }

    void trace(visitor_t& visitor) const noexcept override;
};

class ObjClosure : public ObjBase<ObjectType::FUNCTION> {
   private:
    using proto_t = proto_t;
    using upvalue_t = upvalue_t;
    using visitor_t = GCVisitor;

    proto_t proto_;
    std::vector<upvalue_t> upvalues_;

   public:
    explicit ObjClosure(proto_t proto = nullptr) noexcept : proto_(proto), upvalues_(proto ? proto->get_num_upvalues() : 0) {
    }

    inline proto_t get_proto() const noexcept {
        return proto_;
    }
    /// @brief Unchecked upvalue access. For performance-critical code
    inline upvalue_t get_upvalue(size_t index) const noexcept {
        return upvalues_[index];
    }
    /// @brief Unchecked upvalue modification. For performance-critical code
    inline void set_upvalue(size_t index, upvalue_t upvalue) noexcept {
        upvalues_[index] = upvalue;
    }
    /// @brief Checked upvalue access. Throws if index is OOB
    inline upvalue_t at_upvalue(size_t index) const {
        return upvalues_.at(index);
    }

    void trace(visitor_t& visitor) const noexcept override;
};
}