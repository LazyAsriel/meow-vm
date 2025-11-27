#include "module/module_manager.h"

#include "common/pch.h"
#include "core/objects/module.h"
#include "core/objects/string.h"
#include "memory/memory_manager.h"
#include "module/module_utils.h"
#include "vm/meow_engine.h"
#include "bytecode/binary_loader.h"

namespace meow {

ModuleManager::ModuleManager(MemoryManager* heap, MeowEngine* engine) noexcept
    : heap_(heap), engine_(engine) {}

module_t ModuleManager::load_module(string_t module_path_obj, string_t importer_path_obj) {
    if (!module_path_obj || !importer_path_obj) {
        throw std::runtime_error("ModuleManager::load_module: Đường dẫn module hoặc importer là null.");
    }

    std::string module_path = module_path_obj->c_str();
    std::string importer_path = importer_path_obj->c_str();

    if (auto it = module_cache_.find(module_path_obj); it != module_cache_.end()) {
        return it->second;
    }
    const std::vector<std::string> forbidden_extensions = {".meow", ".meowb"};
    
    const std::vector<std::string> candidate_extensions = {get_platform_library_extension()};
    std::filesystem::path root_dir =
        detect_root_cached("meow-root", "$ORIGIN", true); //
    std::vector<std::filesystem::path> search_roots = make_default_search_roots(root_dir);
    std::string resolved_native_path = resolve_library_path_generic( //
        module_path, importer_path, entry_path_ ? entry_path_->c_str() : "", forbidden_extensions,
        candidate_extensions, search_roots, true);

    if (!resolved_native_path.empty()) {
        string_t resolved_native_path_obj = heap_->new_string(resolved_native_path);
        if (auto it = module_cache_.find(resolved_native_path_obj); it != module_cache_.end()) {
            module_cache_[module_path_obj] = it->second;
            return it->second;
        }

        void* handle = open_native_library(resolved_native_path);
        if (!handle) {
            std::string err_detail = platform_last_error();
            throw std::runtime_error("Không thể tải thư viện native '" + resolved_native_path +
                                     "': " + err_detail);
        }

        const char* factory_symbol_name = "CreateMeowModule";
        void* symbol = get_native_symbol(handle, factory_symbol_name);

        if (!symbol) {
            std::string err_detail = platform_last_error();
            close_native_library(handle);
            throw std::runtime_error("Không tìm thấy biểu tượng (symbol) '" + std::string(factory_symbol_name) +
                                     "' trong thư viện native '" + resolved_native_path +
                                     "': " + err_detail);
        }

        using NativeModuleFactory = module_t (*)(MeowEngine*, MemoryManager*);
        NativeModuleFactory factory = reinterpret_cast<NativeModuleFactory>(symbol);

        module_t native_module = nullptr;
        try {
            native_module = factory(engine_, heap_);
            if (!native_module) {
                throw std::runtime_error("Hàm factory của module native '" + resolved_native_path +
                                         "' trả về null.");
            }
            native_module->set_executed();
        } catch (const std::exception& e) {
            close_native_library(handle);
            throw std::runtime_error(
                "Ngoại lệ C++ khi gọi hàm factory của module native '" +
                resolved_native_path + "': " + e.what());
        } catch (...) {
            close_native_library(handle);
            throw std::runtime_error(
                "Ngoại lệ không xác định khi gọi hàm factory của module native '" +
                resolved_native_path + "'.");
        }

        module_cache_[module_path_obj] = native_module;
        module_cache_[resolved_native_path_obj] = native_module;
        return native_module;
    }
    
    std::filesystem::path base_dir;
    if (importer_path_obj == entry_path_) { 
        base_dir = std::filesystem::path(entry_path_ ? entry_path_->c_str() : "").parent_path();
    } else {
        base_dir = std::filesystem::path(importer_path).parent_path();
    }

    std::filesystem::path binary_file_path_fs = normalize_path(base_dir / module_path);

    if (binary_file_path_fs.extension() != ".meowb") {
        binary_file_path_fs.replace_extension(".meowb");
    }

    std::string binary_file_path = binary_file_path_fs.string();
    string_t binary_file_path_obj = heap_->new_string(binary_file_path);

    if (auto it = module_cache_.find(binary_file_path_obj); it != module_cache_.end()) {
        module_cache_[module_path_obj] = it->second;
        return it->second;
    }

    std::ifstream file(binary_file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Không thể mở tệp module (đã thử native và bytecode '" + 
                                 binary_file_path + "')");
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         throw std::runtime_error("Không thể đọc tệp bytecode: " + binary_file_path);
    }
    
    file.close();

    proto_t main_proto = nullptr;
    try {
        BinaryLoader loader(heap_, buffer);
        main_proto = loader.load_module();
    } catch (const BinaryLoaderError& e) {
        throw std::runtime_error("Tệp bytecode bị hỏng hoặc không hợp lệ: " + 
                                 binary_file_path + " - Lỗi: " + e.what());
    }

    if (!main_proto) {
        throw std::runtime_error("BinaryLoader trả về proto null mà không ném lỗi cho tệp: " + 
                                 binary_file_path);
    }

    string_t filename_obj = heap_->new_string(binary_file_path_fs.filename().string());
    module_t meow_module = heap_->new_module(filename_obj, binary_file_path_obj, main_proto);

    // BinaryLoader loader(heap_, buffer);
    // auto load_result = loader.load();

    // if (!load_result) {
    //     const auto& err = load_result.error();
    //     throw std::runtime_error(std::format(
    //         "Tệp bytecode hỏng: {} \nTại offset: {} \nLỗi: {}", 
    //         binary_file_path, err.offset, err.message
    //     ));
    // }

    // ModuleData data = *load_result;

    // string_t filename_obj = heap_->new_string(binary_file_path_fs.filename().string());
    
    // module_t meow_module = heap_->new_module(
    //     filename_obj, 
    //     binary_file_path_obj, 
    //     data.main_function, 
    //     data.globals_count
    // );

    module_cache_[module_path_obj] = meow_module;
    module_cache_[binary_file_path_obj] = meow_module;
    
    return meow_module;
}

}