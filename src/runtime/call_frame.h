#pragma once
#include <meow/definitions.h>

namespace meow {

struct CallFrame {
    function_t function_ = nullptr;
    Value* regs_base_ = nullptr; 
    Value* ret_dest_ = nullptr;
    const uint8_t* ip_ = nullptr;
    CallFrame() = default;

    CallFrame(function_t func, Value* regs, Value* ret, const uint8_t* ip)
        : function_(func), regs_base_(regs), ret_dest_(ret), ip_(ip) {
    }
};

}