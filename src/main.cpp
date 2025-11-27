#include <iostream>
#include <filesystem>
#include <print>
#include <vector>
#include "vm/machine.h"

namespace fs = std::filesystem;
using namespace meow;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println(stderr, "Usage: meow-vm <path_to_script> [args...]");
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

    } catch (const VMError& e) {
        std::println(stderr, "VM Runtime Error: {}", e.what());
        return 1;
    } catch (const std::exception& e) {
        std::println(stderr, "An unexpected error occurred: {}", e.what());
        return 1;
    }

    return 0;
}