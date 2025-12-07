#pragma once

#include "common/pch.h"
#include "vm/vm_state.h"

namespace meow {

class Interpreter {
public:
    // Entry point duy nhất
    static void run(VMState state) noexcept;
};

}