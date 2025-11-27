#pragma once
// module_loader_utils.h
// Cross-platform, generic loader helpers (functions only, no classes).
// Style: snake_case, caller-provided module-related strings (no hardcoding).
// C++17

#include "common/pch.h"

namespace meow {

// --- filesystem / string helpers ---
std::filesystem::path get_executable_dir() noexcept;  // absolute dir of current executable
                                                      // (fallback: current_path)
std::filesystem::path normalize_path(const std::filesystem::path& p) noexcept;
bool file_exists(const std::filesystem::path& p) noexcept;
std::string read_first_non_empty_line_trimmed(const std::filesystem::path& path) noexcept;
std::string expand_token(const std::string& raw, const std::string& token, const std::filesystem::path& replacement) noexcept;

// --- root detection (generic) ---
// config_filename: filename to look for next to exe_dir (caller decides, e.g.
// "meow-root" or "") token: expansion token (caller decides, e.g. "$ORIGIN" or
// "") treat_bin_as_parent: if exe_dir filename == "bin" treat
// exe_dir.parent_path() as fallback root exe_dir_provider: optionally injected
// (useful for tests) Returns absolute normalized path. Thread-safe (internally
// caches results keyed by inputs).
std::filesystem::path detect_root_cached(const std::string& config_filename, const std::string& token, bool treat_bin_as_parent = true,
                                         std::function<std::filesystem::path()> exe_dir_provider = get_executable_dir) noexcept;

// Build a set of common search roots from a detected root (caller can still
// pass any roots). This is only a helper to produce typical patterns (root,
// root/lib, root/stdlib, root/bin/stdlib, root/bin).
std::vector<std::filesystem::path> make_default_search_roots(const std::filesystem::path& root) noexcept;

// --- library resolution (fully generic) ---
// module_path: import string (may be relative or absolute, with or without ext)
// importer: the importer file path (full path) used to resolve relative imports
// entry_path: entry path string used as special-case base (caller-defined
// meaning) forbidden_extensions: extensions which indicate "language files" (do
// not try native), lowercase expected but function is case-insensitive
// candidate_extensions: list of extensions to try if module_path has no
// extension (order matters) search_roots: additional absolute directories to
// search (in order) extra_relative_search: attempt base_dir(importer)/module
// candidates as last resort Returns absolute normalized existing path, or empty
// string if not found.
std::string resolve_library_path_generic(const std::string& module_path, const std::string& importer, const std::string& entry_path, const std::vector<std::string>& forbidden_extensions,
                                         const std::vector<std::string>& candidate_extensions, const std::vector<std::filesystem::path>& search_roots, bool extra_relative_search = true) noexcept;

// Return platform typical single library extension (helper only)
std::string get_platform_library_extension() noexcept;

// --- platform errors ---
std::string platform_last_error() noexcept;  // human-friendly last error from
                                             // OS loader (may be empty)

// --- native library open/get_symbol/close (C-style functions, no classes) ---
// open_native_library returns a platform handle as void* (NULL on failure).
// - On POSIX: returns result of dlopen()
// - On Windows: returns HMODULE cast to void*
// get_native_symbol returns pointer to symbol or nullptr if not found
// close_native_library closes/frees the handle
void* open_native_library(const std::string& path) noexcept;
void* get_native_symbol(void* handle, const char* symbol_name) noexcept;
void close_native_library(void* handle) noexcept;

}
