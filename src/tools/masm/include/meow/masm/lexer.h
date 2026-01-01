#pragma once
#include "common.h"
#include <string_view>

namespace meow::masm {

class Lexer {
public:
    explicit Lexer(std::string_view src) noexcept;
    
    // [MỚI] Hàm này chỉ lấy đúng 1 token tiếp theo rồi dừng
    Token next_token();

private:
    std::string_view src_;
    // [MỚI] Trạng thái được lưu giữ giữa các lần gọi
    const char* cursor_;
    const char* end_;
    const char* line_start_;
    uint32_t line_ = 1;
};

} // namespace meow::masm