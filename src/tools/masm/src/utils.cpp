#include <meow/masm/utils.h>
#include <meow/masm/lexer.h>
#include <meow/masm/assembler.h>
#include <print>
#include <fstream>
#include <vector>

namespace meow::masm {

void report_error(const Status& s, std::string_view stage) {
    if (s.code() == ErrorCode::FILE_OPEN_FAILED || 
        s.code() == ErrorCode::WRITE_ERROR || 
        s.code() == ErrorCode::READ_ERROR) {
        std::println(stderr, "[{}] Error: {}", stage, get_error_msg(s.code()));
    } else {
        std::println(stderr, "[{}] Error at {}:{}: {}", 
            stage, s.line(), s.col(), get_error_msg(s.code()));
    }
}

Status compile_file(const std::string& input_path, const std::string& output_path) {
    std::ifstream f(input_path, std::ios::binary | std::ios::ate);
    if (!f) [[unlikely]] return Status::error(ErrorCode::FILE_OPEN_FAILED, 0, 0);
    
    auto size = f.tellg();
    std::string source;
    source.resize_and_overwrite(size, [&](char* buf, size_t n) {
        f.seekg(0);
        f.read(buf, n);
        return f.gcount();
    });
    f.close();

    Lexer lexer(source);
    Assembler asm_tool(lexer);
    
    if (Status s = asm_tool.assemble_to_file(output_path); s.is_err()) [[unlikely]] {
        report_error(s, "Assembler");
        return s;
    }

    return Status::ok();
}

} // namespace meow::masm