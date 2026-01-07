#include "module/module_manager.h"

#include "pch.h"
#include <meow/core/module.h>
#include <meow/core/string.h>
#include <meow/memory/memory_manager.h>
#include <meow/memory/gc_disable_guard.h>
#include <meow/memory/gc_visitor.h>
#include "module/module_utils.h"
#include "bytecode/loader.h"

#include <fstream> 

// --- LOCAL MACROS (Chỉ dùng trong file này) ---
#define CHECK(expr) \
    { auto _res = (expr); if (_res.failed()) return _res.error(); }

#define TRY_VAL(var, expr) \
    auto var##_res = (expr); \
    if (var##_res.failed()) return var##_res.error(); \
    auto var = var##_res.value();
    
// Helper để map LoaderErrorCode sang ModuleErrorCode (đơn giản hóa)
// Nếu cần chi tiết hơn, bạn có thể mở rộng Status để chứa union error code
#define CHECK_LOADER(expr) \
    { auto _res = (expr); if (_res.failed()) return error(ModuleErrorCode::BYTECODE_LOAD_FAILED); }

#define TRY_LOADER(var, expr) \
    auto var##_res = (expr); \
    if (var##_res.failed()) return error(ModuleErrorCode::BYTECODE_LOAD_FAILED); \
    auto var = var##_res.value();
// ----------------------------------------------

namespace meow {

static void link_module_to_proto(module_t mod, proto_t proto, std::unordered_set<proto_t>& visited) {
    if (!proto || visited.contains(proto)) return;
    visited.insert(proto);

    proto->set_module(mod);

    Chunk& chunk = proto->get_chunk();
    for (size_t i = 0; i < chunk.get_pool_size(); ++i) {
        if (chunk.get_constant(i).is_proto()) {
            link_module_to_proto(mod, chunk.get_constant(i).as_proto(), visited);
        }
    }
}

ModuleManager::ModuleManager(MemoryManager* heap, Machine* vm) noexcept
    : heap_(heap), vm_(vm), entry_path_(nullptr) {}

Status<ModuleErrorCode> ModuleManager::error(ModuleErrorCode code, uint32_t line, uint32_t col) const {
    return Status<ModuleErrorCode>::make(code, ctx_.file_id, line, col);
}

Result<module_t, ModuleErrorCode> ModuleManager::load_module(string_t module_path_obj, string_t importer_path_obj) {
    if (!module_path_obj) {
        return error(ModuleErrorCode::INVALID_PATH);
    }

    std::string module_path = module_path_obj->c_str();
    std::string importer_path = importer_path_obj ? importer_path_obj->c_str() : "";

    // 1. Kiểm tra Cache
    if (auto it = module_cache_.find(module_path_obj); it != module_cache_.end()) {
        return it->second;
    }

    // 2. Resolve đường dẫn (Native & Bytecode)
    const std::vector<std::string> forbidden_extensions = {".meowb", ".meowc"};
    const std::vector<std::string> candidate_extensions = {get_platform_library_extension()};
    
    std::filesystem::path root_dir = detect_root_cached("meow-root", "$ORIGIN", true);
    std::vector<std::filesystem::path> search_roots = make_default_search_roots(root_dir);
    
    std::string resolved_native_path = resolve_library_path_generic(
        module_path, importer_path, entry_path_ ? entry_path_->c_str() : "", 
        forbidden_extensions, candidate_extensions, search_roots, true
    );

    // ---------------------------------------------------------
    // A. Tải Native Module (.dll, .so, .dylib)
    // ---------------------------------------------------------
    if (!resolved_native_path.empty()) {
        // Cập nhật context để báo lỗi chính xác file này
        ctx_.load(resolved_native_path);

        string_t resolved_native_path_obj = heap_->new_string(resolved_native_path);
        if (auto it = module_cache_.find(resolved_native_path_obj); it != module_cache_.end()) {
            module_cache_[module_path_obj] = it->second;
            return it->second;
        }

        void* handle = open_native_library(resolved_native_path);
        if (!handle) {
            // Note: platform_last_error() trả về string chi tiết, nhưng Status hiện tại là enum.
            // Trong thực tế, bạn có thể log error ra console ở đây.
            return error(ModuleErrorCode::NATIVE_LOAD_FAILED);
        }

        const char* factory_symbol_name = "CreateMeowModule";
        void* symbol = get_native_symbol(handle, factory_symbol_name);

        if (!symbol) {
            close_native_library(handle);
            return error(ModuleErrorCode::NATIVE_SYMBOL_MISSING);
        }

        using NativeModuleFactory = module_t (*)(Machine*, MemoryManager*);
        NativeModuleFactory factory = reinterpret_cast<NativeModuleFactory>(symbol);

        module_t native_module = nullptr;

        native_module = factory(vm_, heap_);
        if (!native_module) {
            return error(ModuleErrorCode::NATIVE_FACTORY_FAILED);
        }
        native_module->set_executed();

        module_cache_[module_path_obj] = native_module;
        module_cache_[resolved_native_path_obj] = native_module;
        return native_module;
    }
    
    // ---------------------------------------------------------
    // B. Tải Bytecode Module (.meowc)
    // ---------------------------------------------------------
    
    std::filesystem::path base_dir;
    if (importer_path_obj == entry_path_) { 
        base_dir = std::filesystem::path(entry_path_ ? entry_path_->c_str() : "").parent_path();
    } else {
        base_dir = std::filesystem::path(importer_path).parent_path();
    }

    std::filesystem::path binary_file_path_fs = normalize_path(base_dir / module_path);
    if (binary_file_path_fs.extension() != ".meowc") {
        binary_file_path_fs.replace_extension(".meowc");
    }

    std::string binary_file_path = binary_file_path_fs.string();
    ctx_.load(binary_file_path); // Update context filename

    string_t binary_file_path_obj = heap_->new_string(binary_file_path);

    if (auto it = module_cache_.find(binary_file_path_obj); it != module_cache_.end()) {
        module_cache_[module_path_obj] = it->second;
        return it->second;
    }

    // Đọc file nhị phân
    std::ifstream file(binary_file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        // Đã thử native và bytecode đều không được
        return error(ModuleErrorCode::FILE_NOT_FOUND);
    }
    
    std::streamsize size = file.tellg();
    if (size < 0) return error(ModuleErrorCode::FILE_READ_ERROR);

    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         return error(ModuleErrorCode::FILE_READ_ERROR);
    }
    file.close();

    GCDisableGuard guard(heap_);

    // Gọi Loader (Result-based)
    Loader loader(heap_, buffer, binary_file_path);
    TRY_LOADER(main_proto, loader.load_module());

    if (!main_proto) {
        return error(ModuleErrorCode::BYTECODE_LOAD_FAILED);
    }

    string_t filename_obj = heap_->new_string(binary_file_path_fs.filename().string());
    module_t meow_module = heap_->new_module(filename_obj, binary_file_path_obj, main_proto);

    // Link globals
    CHECK_LOADER(Loader::link_module(meow_module));

    std::unordered_set<proto_t> visited;
    link_module_to_proto(meow_module, main_proto, visited);

    module_cache_[module_path_obj] = meow_module;
    module_cache_[binary_file_path_obj] = meow_module;
    
    // Auto Inject 'native' module
    if (std::string(filename_obj->c_str()) != "native") {
        string_t native_name = heap_->new_string("native");
        
        // Gọi đệ quy, nếu thất bại thì bỏ qua (không bắt buộc phải có native nếu không dùng)
        // Hoặc có thể return lỗi nếu 'native' là bắt buộc. Ở đây ta chọn bỏ qua lỗi nhẹ.
        auto native_res = load_module(native_name, nullptr);
        if (native_res.ok()) {
            meow_module->import_all_global(native_res.value());
        }
    }
    
    return meow_module;
}

void ModuleManager::trace(GCVisitor& visitor) const noexcept {
    for (const auto& [key, mod] : module_cache_) {
        visitor.visit_object(key); 
        visitor.visit_object(mod); 
    }
}

} // namespace meow

// Cleanup macros
#undef CHECK
#undef TRY_VAL
#undef CHECK_LOADER
#undef TRY_LOADER