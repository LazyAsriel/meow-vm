#include <iostream>
#include <filesystem>
#include <print>
#include <vector>
#include <string>

#include <meow/machine.h>
#include <meow/config.h>
#include <meow/masm/utils.h> 

namespace fs = std::filesystem;
using namespace meow;

void print_usage() {
    std::println(stderr, "Usage: meow-vm [options] <file>");
    std::println(stderr, "Options:");
    std::println(stderr, "  -b, --bytecode    Run pre-compiled bytecode (.meowc) [Default]");
    std::println(stderr, "  -c, --compile     Compile and run source assembly (.meowb/.asm)");
    std::println(stderr, "  -v, --version     Show version info");
    std::println(stderr, "  -h, --help        Show this help message");
}

enum class Mode {
    Bytecode,
    SourceAsm,
    Unknown
};

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::vector<std::string> args;
    args.reserve(argc);
    for(int i=1; i<argc; ++i) args.push_back(argv[i]);

    if (args[0] == "--version" || args[0] == "-v") {
        std::println("MeowVM v{} (Built with ❤️ by LazyPaws)", MEOW_VERSION_STR);
        return 0;
    }
    if (args[0] == "--help" || args[0] == "-h") {
        print_usage();
        return 0;
    }

    Mode mode = Mode::Bytecode;
    std::string input_file;

    if (args[0] == "-c" || args[0] == "--compile") {
        mode = Mode::SourceAsm;
        if (args.size() < 2) {
            std::println(stderr, "Error: Missing input file for compile mode.");
            return 1;
        }
        input_file = args[1];
    } else if (args[0] == "-b" || args[0] == "--bytecode") {
        mode = Mode::Bytecode;
        if (args.size() < 2) {
            std::println(stderr, "Error: Missing input file.");
            return 1;
        }
        input_file = args[1];
    } else {
        input_file = args[0];
        if (input_file.ends_with(".meowb") || input_file.ends_with(".asm")) {
            mode = Mode::SourceAsm;
        }
    }

    if (!fs::exists(input_file)) {
        std::println(stderr, "Error: File '{}' not found.", input_file);
        return 1;
    }

    if (mode == Mode::SourceAsm) {
        masm::init_op_map();

        fs::path src_path(input_file);
        fs::path bin_path = src_path;
        bin_path.replace_extension(".meowc");

        masm::Status s = masm::compile_file(input_file, bin_path.string());
        
        if (s.is_err()) {
            return 1; 
        }

        input_file = bin_path.string();
    }
    
    std::vector<char*> clean_argv;
    clean_argv.reserve(argc);
    clean_argv.push_back(argv[0]); // Program name

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-b" || arg == "--bytecode" || arg == "-c" || arg == "--compile") {
            continue; 
        }
        clean_argv.push_back(argv[i]);
    }

    fs::path abs_path = fs::absolute(input_file);
    std::string root_dir = abs_path.parent_path().string();
    std::string entry_file = abs_path.filename().string();
    
    Machine vm(root_dir, entry_file, static_cast<int>(clean_argv.size()), clean_argv.data()); 
    vm.interpret();

    return 0;
}