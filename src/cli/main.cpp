#include <iostream>
#include <filesystem>
#include <print>
#include <vector>
#include <cstring>
#include <meow/machine.h>
#include <meow/config.h>

namespace fs = std::filesystem;
using namespace meow;

int main(int argc, char* argv[]) {
    if (argc >= 2 && (std::strcmp(argv[1], "--version") == 0 || std::strcmp(argv[1], "-v") == 0)) {
        std::println("MeowVM v{} (Built with ❤️ by LazyPaws)", MEOW_VERSION_STR);
        return 0;
    }

    if (argc < 2) {
        std::println(stderr, "Usage: meow-vm <path_to_script> [args...]");
        std::println(stderr, "       meow-vm --version");
        return 1;
    }

    std::string input_path_str = argv[1];
    fs::path input_path(input_path_str);

    std::error_code ec;
    if (!fs::exists(input_path, ec) || !fs::is_regular_file(input_path, ec)) {
        std::println(stderr, "Error: File '{}' not found or is not a valid file.", input_path_str);
        return 1;
    }

    try {
        fs::path abs_path = fs::absolute(input_path);
        std::string root_dir = abs_path.parent_path().string();
        std::string entry_file = abs_path.filename().string();
        Machine vm(root_dir, entry_file, argc, argv);
        
        vm.interpret();
    } catch (const std::exception& e) {
        std::println(stderr, "An unexpected error occurred: {}", e.what());
        return 1;
    }

    return 0;
}