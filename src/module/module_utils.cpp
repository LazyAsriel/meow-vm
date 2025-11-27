#include "module/module_utils.h"
#include "common/pch.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <limits.h>
#include <unistd.h>
#endif
#endif

namespace meow {

static inline std::string to_lower_copy(std::string s) noexcept {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::filesystem::path get_executable_dir() noexcept {
    try {
#if defined(_WIN32)
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
        if (len == 0) return std::filesystem::current_path();
        return std::filesystem::path(std::string(buf, static_cast<size_t>(len))).parent_path();
#elif defined(__APPLE__)
        uint32_t size = 0;
        if (_NSGetExecutablePath(nullptr, &size) != 0 && size == 0) return std::filesystem::current_path();
        std::vector<char> buf(size ? size : 1);
        if (_NSGetExecutablePath(buf.data(), &size) != 0) return std::filesystem::current_path();
        return std::filesystem::absolute(std::filesystem::path(buf.data())).parent_path();
#else
        char buf[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len == -1) return std::filesystem::current_path();
        buf[len] = '\0';
        return std::filesystem::path(std::string(buf, static_cast<size_t>(len))).parent_path();
#endif
    } catch (...) {
        return std::filesystem::current_path();
    }
}

std::filesystem::path normalize_path(const std::filesystem::path& p) noexcept {
    try {
        if (p.empty()) return p;
        return std::filesystem::absolute(p).lexically_normal();
    } catch (...) {
        return p;
    }
}

bool file_exists(const std::filesystem::path& p) noexcept {
    try {
        return std::filesystem::exists(p);
    } catch (...) {
        return false;
    }
}

std::string read_first_non_empty_line_trimmed(const std::filesystem::path& path) noexcept {
    try {
        std::ifstream in(path);
        if (!in) return std::string();
        std::string line;
        while (std::getline(in, line)) {
            // trim both ends
            auto ltrim = [](std::string& s) { s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); })); };
            auto rtrim = [](std::string& s) { s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end()); };
            rtrim(line);
            ltrim(line);
            if (!line.empty()) return line;
        }
    } catch (...) {
    }
    return std::string();
}

std::string expand_token(const std::string& raw, const std::string& token, const std::filesystem::path& replacement) noexcept {
    if (token.empty() || !raw.contains(token)) return raw;
    std::string out;
    out.reserve(raw.size() + replacement.string().size());
    size_t pos = 0;
    while (true) {
        size_t p = raw.find(token, pos);
        if (p == std::string::npos) {
            out.append(raw.substr(pos));
            break;
        }
        out.append(raw.substr(pos, p - pos));
        out.append(replacement.string());
        pos = p + token.size();
    }
    return out;
}

// -------------------- root detection with caching (thread-safe, keyed)
// --------------------
struct cache_key {
    std::string config_filename;
    std::string token;
    bool treat_bin_as_parent;
    bool operator==(const cache_key& o) const noexcept {
        return config_filename == o.config_filename && token == o.token && treat_bin_as_parent == o.treat_bin_as_parent;
    }
};
namespace {
struct key_hash {
    size_t operator()(cache_key const& k) const noexcept {
        std::hash<std::string> h;
        size_t r = h(k.config_filename);
        r = r * 1315423911u + h(k.token);
        r ^= static_cast<size_t>(k.treat_bin_as_parent) + 0x9e3779b97f4a7c15ULL + (r << 6) + (r >> 2);
        return r;
    }
};
static std::mutex s_cache_mutex;
static std::unordered_map<cache_key, std::filesystem::path, key_hash> s_root_cache;
}

std::filesystem::path detect_root_cached(const std::string& config_filename, const std::string& token, bool treat_bin_as_parent, std::function<std::filesystem::path()> exe_dir_provider) noexcept {
    try {
        cache_key k{config_filename, token, treat_bin_as_parent};
        {
            std::lock_guard<std::mutex> lk(s_cache_mutex);
            auto it = s_root_cache.find(k);
            if (it != s_root_cache.end()) return it->second;
        }

        std::filesystem::path exe_dir = exe_dir_provider();
        if (!config_filename.empty()) {
            std::filesystem::path config_path = exe_dir / config_filename;
            if (file_exists(config_path)) {
                std::string line = read_first_non_empty_line_trimmed(config_path);
                if (!line.empty()) {
                    std::string expanded = token.empty() ? line : expand_token(line, token, exe_dir);
                    std::filesystem::path result = normalize_path(std::filesystem::path(expanded));
                    {
                        std::lock_guard<std::mutex> lk(s_cache_mutex);
                        s_root_cache.emplace(k, result);
                    }
                    return result;
                }
            }
        }

        std::filesystem::path fallback = exe_dir;
        if (treat_bin_as_parent && exe_dir.filename() == "bin") fallback = exe_dir.parent_path();
        std::filesystem::path result = normalize_path(fallback);
        {
            std::lock_guard<std::mutex> lk(s_cache_mutex);
            s_root_cache.emplace(k, result);
        }
        return result;
    } catch (...) {
        return std::filesystem::current_path();
    }
}

// -------------------- default search roots helper --------------------
std::vector<std::filesystem::path> make_default_search_roots(const std::filesystem::path& root) noexcept {
    std::vector<std::filesystem::path> v;
    try {
        v.reserve(5);
        v.push_back(normalize_path(root));
        v.push_back(normalize_path(root / "lib"));
        v.push_back(normalize_path(root / "stdlib"));
        v.push_back(normalize_path(root / "bin" / "stdlib"));
        v.push_back(normalize_path(root / "bin"));
    } catch (...) {
    }
    return v;
}

std::string resolve_library_path_generic(const std::string& module_path, const std::string& importer, const std::string& entry_path, const std::vector<std::string>& forbidden_extensions,
                                         const std::vector<std::string>& candidate_extensions, const std::vector<std::filesystem::path>& search_roots, bool extra_relative_search) noexcept {
    try {
        std::filesystem::path candidate(module_path);
        std::string ext = candidate.extension().string();
        if (!ext.empty()) {
            std::string ext_l = to_lower_copy(ext);
            for (const auto& f : forbidden_extensions) {
                if (ext_l == to_lower_copy(f)) return "";
            }
            if (candidate.is_absolute() && file_exists(candidate)) return normalize_path(candidate).string();
        }

        std::vector<std::filesystem::path> to_try;
        to_try.reserve(8);

        if (candidate.extension().empty() && !candidate_extensions.empty()) {
            for (const auto& ce : candidate_extensions) {
                std::filesystem::path p = candidate;
                p.replace_extension(ce);
                to_try.push_back(p);
            }
        } else {
            to_try.push_back(candidate);
        }

        for (const auto& root : search_roots) {
            for (const auto& t : to_try) {
                std::filesystem::path p = root / t;
                if (file_exists(p)) return normalize_path(p).string();
            }
        }

        for (const auto& t : to_try) {
            if (file_exists(t)) return normalize_path(t).string();
        }

        if (extra_relative_search) {
            std::filesystem::path base_dir;
            if (importer == entry_path)
                base_dir = std::filesystem::path(entry_path);
            else
                base_dir = std::filesystem::path(importer).parent_path();

            for (const auto& t : to_try) {
                std::filesystem::path p = normalize_path(base_dir / t);
                if (file_exists(p)) return p.string();
            }
        }

        return "";
    } catch (...) {
        return "";
    }
}

std::string get_platform_library_extension() noexcept {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

std::string platform_last_error() noexcept {
#if defined(_WIN32)
    DWORD err = GetLastError();
    if (err == 0) return std::string();
    LPSTR buf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, nullptr);
    std::string s = buf ? std::string(buf) : std::string();
    if (buf) LocalFree(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
#else
    const char* e = dlerror();
    return e ? std::string(e) : std::string();
#endif
}

void* open_native_library(const std::string& path) noexcept {
#if defined(_WIN32)
    HMODULE h = LoadLibraryA(path.c_str());
    return reinterpret_cast<void*>(h);
#else
    // clear previous errors
    dlerror();
    void* h = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    return h;
#endif
}

void* get_native_symbol(void* handle, const char* symbol_name) noexcept {
    if (!handle || !symbol_name) return nullptr;
#if defined(_WIN32)
    FARPROC p = GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol_name);
    return reinterpret_cast<void*>(p);
#else
    dlerror();
    void* p = dlsym(handle, symbol_name);
    const char* err = dlerror();
    (void)err;
    return p;
#endif
}

void close_native_library(void* handle) noexcept {
    if (!handle) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

}
