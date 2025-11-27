#include "common.h"
#include "lexer.h"
#include "assembler.h"
#include <iostream>
#include <fstream>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: meow-asm <input.meow> [output.meowb]\n";
        return 1;
    }

    init_op_map();

    std::string input_path = argv[1];
    std::string output_path = (argc >= 3) ? argv[2] : "out.meowb";
    
    if (argc < 3 && input_path.size() > 5 && input_path.ends_with(".meow")) {
        output_path = input_path.substr(0, input_path.size()-5) + ".meowb";
    }

    std::ifstream f(input_path, std::ios::ate); // Mở ở cuối để lấy size
    if (!f) {
        std::cerr << "Cannot open input file: " << input_path << "\n";
        return 1;
    }
    
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::string source(size, '\0');
    if (!f.read(&source[0], size)) {
         std::cerr << "Read error\n";
         return 1;
    }
    f.close();

    // Lexing
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    // Assembling
    try {
        Assembler asm_tool(tokens);
        asm_tool.assemble(output_path);
    } catch (const std::exception& e) {
        std::cerr << "[Assembly Error] " << e.what() << "\n";
        return 1;
    }

    return 0;
}