#pragma once

#include "common/definitions.h"

namespace meow {
struct CallFrame {
    function_t function_;
    module_t module_;
    size_t start_reg_;
    size_t ret_reg_;
    const uint8_t* ip_;
    CallFrame(function_t function, module_t module, size_t start_reg, size_t ret_reg, const uint8_t* ip)
        : function_(function), module_(module), start_reg_(start_reg), ret_reg_(ret_reg), ip_(ip) {
    }
};
}