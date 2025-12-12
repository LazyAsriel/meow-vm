#pragma once

#include "pch.h"
#include "vm/vm_state.h"

namespace meow {

class Interpreter {
public:
    static void run(VMState state) noexcept;
};

}