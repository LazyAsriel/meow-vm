#pragma once

#include "common/pch.h"
namespace meow {
struct ExceptionHandler {
    size_t catch_ip_;
    size_t frame_depth_; // Số lượng frame tại thời điểm try
    size_t stack_depth_; // Số lượng register tại thời điểm try
    size_t error_reg_;   // Register để lưu lỗi (0xFFFF nếu không cần)

    ExceptionHandler(size_t catch_ip = 0, size_t frame_depth = 0, size_t stack_depth = 0, size_t error_reg = static_cast<size_t>(-1)) 
        : catch_ip_(catch_ip), frame_depth_(frame_depth), stack_depth_(stack_depth), error_reg_(error_reg) {
    }
};
}