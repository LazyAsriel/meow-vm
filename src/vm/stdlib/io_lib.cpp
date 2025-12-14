#include "pch.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/cast.h>

namespace meow::natives::io {

namespace fs = std::filesystem;

#define CHECK_ARGS(n) if (argc < n) { vm->error("IO Error: Expected " #n " arguments."); return Value(null_t{}); }

// io.input(prompt)
static Value input(Machine* vm, int argc, Value* argv) {
    if (argc > 0) {
        std::print("{}", to_string(argv[0]));
        std::cout.flush();
    }
    
    std::string line;
    if (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return Value(vm->get_heap()->new_string(line));
    }
    return Value(null_t{});
}

// io.read(path)
static Value read_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::string path_str = argv[0].as_string()->c_str();
    
    std::ifstream file(path_str, std::ios::binary | std::ios::ate);
    if (!file) return Value(null_t{});

    auto size = file.tellg();
    file.seekg(0);

    std::string content(size, '\0');
    if (file.read(&content[0], size)) {
        return Value(vm->get_heap()->new_string(content));
    }
    return Value(null_t{});
}

// io.write(path, data, append)
static Value write_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(2);
    std::string path = argv[0].as_string()->c_str();
    std::string data = to_string(argv[1]);
    bool append = (argc > 2) ? to_bool(argv[2]) : false;

    auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    std::ofstream file(path, mode);
    
    if (file << data) return Value(true);
    return Value(false);
}

// io.fileExists(path)
static Value file_exists(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::exists(argv[0].as_string()->c_str(), ec));
}

// io.isDirectory(path)
static Value is_directory(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::is_directory(argv[0].as_string()->c_str(), ec));
}

// io.listDir(path)
static Value list_dir(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::string path = argv[0].as_string()->c_str();
    std::error_code ec;
    
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) return Value(null_t{});

    auto arr = vm->get_heap()->new_array();
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        arr->push(Value(vm->get_heap()->new_string(entry.path().filename().string())));
    }
    return Value(arr);
}

// io.createDir(path)
static Value create_dir(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::create_directories(argv[0].as_string()->c_str(), ec));
}

// io.deleteFile(path)
static Value delete_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::remove_all(argv[0].as_string()->c_str(), ec) > 0);
}

// io.getFileName(path)
static Value get_file_name(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    fs::path p(argv[0].as_string()->c_str());
    return Value(vm->get_heap()->new_string(p.filename().string()));
}

// io.getFileExtension(path)
static Value get_extension(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    fs::path p(argv[0].as_string()->c_str());
    std::string ext = p.extension().string();
    if (ext.starts_with(".")) ext.erase(0, 1);
    return Value(vm->get_heap()->new_string(ext));
}

// io.getAbsolutePath(path)
static Value get_abs_path(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    fs::path p = fs::absolute(argv[0].as_string()->c_str(), ec);
    return Value(vm->get_heap()->new_string(p.string()));
}

} // namespace meow::natives::io