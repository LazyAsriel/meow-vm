#pragma once

#include "pch.h"
#include <meow/definitions.h>
#include <meow/bytecode/chunk.h>
#include <meow/core/function.h>
#include <meow/core/module.h>

namespace meow {

class MemoryManager;

class LoaderError : public std::runtime_error {
public:
    explicit LoaderError(const std::string& msg) : std::runtime_error(msg) {}
};

class Loader {
public:
    Loader(MemoryManager* heap, const std::vector<uint8_t>& data);

    proto_t load_module();
    static void link_module(module_t module);

private:
    struct Patch {
        size_t proto_idx;
        size_t const_idx;
        uint32_t target_idx;
    };

    MemoryManager* heap_;
    const std::vector<uint8_t>& data_;
    size_t cursor_ = 0;

    std::vector<proto_t> loaded_protos_;
    std::vector<Patch> patches_;

    void check_can_read(size_t bytes);
    uint8_t  read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();
    double   read_f64();
    string_t read_string();
    
    value_t read_constant(size_t current_proto_idx, size_t current_const_idx);
    proto_t read_prototype(size_t current_proto_idx);
    
    void check_magic();
    void link_prototypes();
};

}