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
        vm->error("IO Error: Expected " #n " arguments."); \
        return Value(null_t{}); \
    }

// --- Basic IO ---

static Value input(Machine* vm, int argc, Value* argv) {
    if (argc > 0) {
        // Direct conversion, no intermediate string copy if possible
        std::print("{}", to_string(argv[0]));
        std::cout.flush();
    }
    
    std::string line;
    if (std::getline(std::cin, line)) {
        // Handle Windows CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return Value(vm->get_heap()->new_string(line));
    }
    return Value(null_t{});
}

static Value read_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    const char* path_str = argv[0].as_string()->c_str(); // Zero-copy access
    
    std::ifstream file(path_str, std::ios::binary | std::ios::ate);
    if (!file) return Value(null_t{});

    auto size = file.tellg();
    if (size == -1) return Value(null_t{});
    
    file.seekg(0);

    // Allocate string buffer directly via MemoryManager? 
    // Currently string wrapper copies data, so we read to std::string first.
    std::string content(static_cast<size_t>(size), '\0');
    if (file.read(content.data(), size)) {
        return Value(vm->get_heap()->new_string(content));
    }
    return Value(null_t{});
}

static Value write_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(2);
    const char* path = argv[0].as_string()->c_str();
    std::string data = to_string(argv[1]);
    bool append = (argc > 2) ? to_bool(argv[2]) : false;

    auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    std::ofstream file(path, mode);
    
    return Value(file && (file << data));
}

// --- Filesystem Operations (No Exceptions, ErrorCode only) ---

static Value file_exists(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::exists(argv[0].as_string()->c_str(), ec));
}

static Value is_directory(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::is_directory(argv[0].as_string()->c_str(), ec));
}

static Value list_dir(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    const char* path = argv[0].as_string()->c_str();
    std::error_code ec;
    
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) return Value(null_t{});

    auto arr = vm->get_heap()->new_array();
    
    // Use directory_iterator non-recursively
    auto dir_it = fs::directory_iterator(path, ec);
    if (ec) return Value(null_t{});

    for (const auto& entry : dir_it) {
        // Optimize: Convert path directly to MeowString
        // Note: entry.path().string() creates a temp std::string
        arr->push(Value(vm->get_heap()->new_string(entry.path().filename().string())));
    }
    return Value(arr);
}

static Value create_dir(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::create_directories(argv[0].as_string()->c_str(), ec));
}

static Value delete_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    return Value(fs::remove_all(argv[0].as_string()->c_str(), ec) > 0);
}

static Value rename_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(2);
    std::error_code ec;
    fs::rename(argv[0].as_string()->c_str(), argv[1].as_string()->c_str(), ec);
    return Value(!ec); // True if success (ec == 0)
}

static Value copy_file(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(2);
    std::error_code ec;
    fs::copy(argv[0].as_string()->c_str(), argv[1].as_string()->c_str(), 
             fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
    return Value(!ec);
}

static Value get_file_size(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    auto sz = fs::file_size(argv[0].as_string()->c_str(), ec);
    if (ec) return Value(static_cast<int64_t>(-1));
    return Value(static_cast<int64_t>(sz));
}

static Value get_file_timestamp(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    auto ftime = fs::last_write_time(argv[0].as_string()->c_str(), ec);
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
    // Path parsing without heavy allocation if possible, but fs::path usually allocates
    fs::path p(argv[0].as_string()->c_str());
    return Value(vm->get_heap()->new_string(p.filename().string()));
}

static Value get_file_extension(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    fs::path p(argv[0].as_string()->c_str());
    std::string ext = p.extension().string();
    // Remove dot if present
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    return Value(vm->get_heap()->new_string(ext));
}

static Value get_abs_path(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::error_code ec;
    fs::path p = fs::absolute(argv[0].as_string()->c_str(), ec);
    return Value(vm->get_heap()->new_string(p.string()));
}

} // namespace meow::natives::io

namespace meow::stdlib {

module_t create_io_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("io");
    auto mod = heap->new_module(name, name);

    // Helper lambda for cleaner registration
    auto reg = [&](const char* n, native_t fn) {
        // [Cite: 1] Using set_export to register native functions
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
    reg("getAbsolutePath", get_abs_path);

    return mod;
}

} // namespace meow::stdlib