#pragma once

#include "common/pch.h"
#include <print>

namespace meow {
    
template <typename... Args>
inline void print(std::format_string<Args...> fmt, Args&&... args) {
    std::print(stdout, "[log] "); 
    std::print(stdout, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void printl(std::format_string<Args...> fmt, Args&&... args) {
    std::print(stdout, "[log] ");
    std::println(stdout, fmt, std::forward<Args>(args)...);
}

}