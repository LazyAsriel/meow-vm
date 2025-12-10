#pragma once

#include "common/pch.h"
#include "vm/vm_state.h"

namespace meow {

class Interpreter {
public:
    static void run(VMState state) noexcept;
};

}