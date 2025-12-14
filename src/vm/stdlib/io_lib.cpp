#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/cast.h>
#include <meow/core/module.h>

namespace meow::natives::io {

namespace fs = std::filesystem;

// Macro check args tối ưu (Branch prediction hint)
#define CHECK_ARGS(n) \
    if (argc < n) [[unlikely]] { \
        vm->error("IO Error: Expected at least " #n " arguments."); \
        return Value(null_t{}); \
    }

// Macro check Argument type is String (for paths)
// Nếu pass, tạo biến path_str_idx (ví dụ: path_str_0) là con trỏ const char*
#define CHECK_PATH_ARG(idx) \
    if (argc <= idx || !argv[idx].is_string()) [[unlikely]] { \
        /* Báo lỗi chính xác kiểu dữ liệu nhận được để dễ debug */ \
        vm->error(std::format("IO Error: Argument {} (Path) expects a String, but received {}.", idx, to_string(argv[idx]))); \
        return Value(null_t{}); \
    } \
    const char* path_str_##idx = argv[idx].as_string()->c_str();


// --- Basic IO ---

static Value input(Machine* vm, int argc, Value* argv) {
    // Prompt argument is optional and can be any value (will be converted to string)
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

static Value read_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    
    std::ifstream file(path_str_0, std::ios::binary | std::ios::ate);
    if (!file) return Value(null_t{});

    auto size = file.tellg();
    if (size == -1) return Value(null_t{});
    
    file.seekg(0);

    std::string content(static_cast<size_t>(size), '\0');
    if (file.read(content.data(), size)) {
        if (content.size() >= 3 && 
            static_cast<unsigned char>(content[0]) == 0xEF && 
            static_cast<unsigned char>(content[1]) == 0xBB && 
            static_cast<unsigned char>(content[2]) == 0xBF) {
            content.erase(0, 3);
        }

        return Value(vm->get_heap()->new_string(content));
    }
    return Value(null_t{});
}

static Value write_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(2);
    CHECK_PATH_ARG(0); // Đảm bảo argv[0] là String và tạo path_str_0
    
    // argv[1] (data) được chuyển đổi an toàn bằng to_string
    std::string data = to_string(argv[1]);
    bool append = (argc > 2) ? to_bool(argv[2]) : false;

    auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    std::ofstream file(path_str_0, mode);
    
    return Value(file && (file << data));
}

// --- Filesystem Operations (No Exceptions, ErrorCode only) ---

static Value file_exists(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    return Value(fs::exists(path_str_0, ec));
}

static Value is_directory(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    return Value(fs::is_directory(path_str_0, ec));
}

static Value list_dir(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    
    if (!fs::exists(path_str_0, ec) || !fs::is_directory(path_str_0, ec)) return Value(null_t{});

    auto arr = vm->get_heap()->new_array();
    
    auto dir_it = fs::directory_iterator(path_str_0, ec);
    if (ec) return Value(null_t{});

    for (const auto& entry : dir_it) {
        arr->push(Value(vm->get_heap()->new_string(entry.path().filename().string())));
    }
    return Value(arr);
}

static Value create_dir(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    return Value(fs::create_directories(path_str_0, ec));
}

static Value delete_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    return Value(fs::remove_all(path_str_0, ec) > 0);
}

static Value rename_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(2);
    CHECK_PATH_ARG(0); // Source
    CHECK_PATH_ARG(1); // Destination
    std::error_code ec;
    fs::rename(path_str_0, path_str_1, ec);
    return Value(!ec);
}

static Value copy_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(2);
    CHECK_PATH_ARG(0); // Source
    CHECK_PATH_ARG(1); // Destination
    std::error_code ec;
    fs::copy(path_str_0, path_str_1, 
             fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
    return Value(!ec);
}

static Value get_file_size(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    auto sz = fs::file_size(path_str_0, ec);
    if (ec) return Value(static_cast<int64_t>(-1));
    return Value(static_cast<int64_t>(sz));
}

static Value get_file_timestamp(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    auto ftime = fs::last_write_time(path_str_0, ec);
    if (ec) return Value(static_cast<int64_t>(-1));

    // C++20 Clock casting magic to get ms since epoch
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    return Value(static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count()
    ));
}

// --- Path Helpers ---

static Value get_file_name(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    fs::path p(path_str_0);
    return Value(vm->get_heap()->new_string(p.filename().string()));
}

static Value get_file_extension(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    fs::path p(path_str_0);
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    return Value(vm->get_heap()->new_string(ext));
}

static Value get_file_stem(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    fs::path p(path_str_0);
    return Value(vm->get_heap()->new_string(p.stem().string()));
}

static Value get_abs_path(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    CHECK_PATH_ARG(0);
    std::error_code ec;
    
    // Dòng này đã an toàn vì path_str_0 đảm bảo là con trỏ hợp lệ từ String Object
    fs::path p = fs::absolute(path_str_0, ec);
    
    if (ec) {
        vm->error(std::format("IO Error: Could not resolve absolute path for '{}'. Error: {}", path_str_0, ec.message()));
        return Value(null_t{});
    }
    
    return Value(vm->get_heap()->new_string(p.string()));
}

} // namespace meow::natives::io

namespace meow::stdlib {

module_t create_io_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("io");
    auto mod = heap->new_module(name, name);

    auto reg = [&](const char* n, native_t fn) {
        mod->set_export(heap->new_string(n), Value(fn));
    };

    using namespace meow::natives::io;
    
    reg("input", input);
    reg("read", read_file);
    reg("write", write_file);
    
    reg("fileExists", file_exists);
    reg("isDirectory", is_directory);
    reg("listDir", list_dir);
    reg("createDir", create_dir);
    reg("deleteFile", delete_file);
    reg("renameFile", rename_file);
    reg("copyFile", copy_file);
    
    reg("getFileSize", get_file_size);
    reg("getFileTimestamp", get_file_timestamp);
    reg("getFileName", get_file_name);
    reg("getFileExtension", get_file_extension);
    reg("getFileStem", get_file_stem);
    reg("getAbsolutePath", get_abs_path);

    return mod;
}

} // namespace meow::stdlib

// Dọn dẹp macros
#undef CHECK_ARGS
#undef CHECK_PATH_ARG