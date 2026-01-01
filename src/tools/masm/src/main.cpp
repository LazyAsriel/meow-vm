#include <meow/masm/utils.h>
#include <meow/masm/lexer.h>
#include <meow/masm/assembler.h>
#include <print>
#include <string>
#include <chrono>
#include <fstream>

using namespace meow::masm;
using Clock = std::chrono::high_resolution_clock;

std::string read_fast(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    std::string s;
    s.resize_and_overwrite(size, [&](char* buf, size_t n) {
        f.seekg(0);
        f.read(buf, n);
        return f.gcount();
    });
    return s;
}

int main(int argc, char* argv[]) {
    auto t_start = Clock::now();

    if (argc < 2) {
        std::println("Usage: masm <input.meow> [output.meowb]");
        return 1;
    }

    init_op_map();

    std::string input_path = argv[1];
    std::string output_path = (argc >= 3) ? argv[2] : "out.meowc";
    
    if (argc < 3 && input_path.size() > 6 && input_path.ends_with(".meowb")) {
        output_path = input_path.substr(0, input_path.size()-6) + ".meowc";
    }

    auto t0 = Clock::now();
    std::string source = read_fast(input_path);
    if (source.empty()) return 1;
    auto t1 = Clock::now();

    Lexer lexer(source);
    Assembler asm_tool(lexer);
    
    if (Status s = asm_tool.assemble(); s.is_err()) {
        report_error(s, "Assembler");
        return 1;
    }
    auto t2 = Clock::now();

    std::ofstream out(output_path, std::ios::binary);
    char buffer[65536];
    out.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
    asm_tool.write_binary(out);
    out.close();
    auto t3 = Clock::now();

    using ms = std::chrono::duration<double, std::milli>;
    std::println("Done: '{}' -> '{}'", input_path, output_path);
    std::println("Stats:");
    std::println("  Read     : {:.3f} ms", ms(t1 - t0).count());
    std::println("  Lex+Asm  : {:.3f} ms", ms(t2 - t1).count());
    std::println("  Write    : {:.3f} ms", ms(t3 - t2).count());
    std::println("  Total    : {:.3f} ms", ms(t3 - t_start).count());

    return 0;
}