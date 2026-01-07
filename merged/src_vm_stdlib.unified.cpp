// =============================================================================
//  FILE PATH: src/vm/stdlib/array_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/core/module.h>
     7	#include <meow/core/array.h> 
     8	#include <meow/cast.h>
     9	#include <format> 
    10	#include <algorithm>
    11	
    12	namespace meow::natives::array {
    13	
    14	constexpr size_t MAX_ARRAY_CAPACITY = 64 * 1024 * 1024; 
    15	
    16	#define CHECK_SELF() \
    17	    if (argc < 1 || !argv[0].is_array()) { \
    18	        vm->error("Array method expects 'this' to be an Array."); \
    19	        return Value(null_t{}); \
    20	    } \
    21	    array_t self = argv[0].as_array();
    22	
    23	static Value push(Machine* vm, int argc, Value* argv) {
    24	    CHECK_SELF();
    25	    if (self->size() + (argc - 1) >= MAX_ARRAY_CAPACITY) [[unlikely]] {
    26	        vm->error("Array size exceeded limit during push.");
    27	        return Value(null_t{});
    28	    }
    29	
    30	    for (int i = 1; i < argc; ++i) {
    31	        self->push(argv[i]);
    32	    }
    33	    return Value((int64_t)self->size());
    34	}
    35	
    36	static Value pop(Machine* vm, int argc, Value* argv) {
    37	    CHECK_SELF();
    38	    if (self->empty()) return Value(null_t{}); 
    39	    
    40	    Value val = self->back();
    41	    self->pop();
    42	    return val;
    43	}
    44	
    45	static Value clear(Machine* vm, int argc, Value* argv) {
    46	    CHECK_SELF();
    47	    self->clear();
    48	    return Value(null_t{});
    49	}
    50	
    51	static Value length(Machine* vm, int argc, Value* argv) {
    52	    CHECK_SELF();
    53	    return Value((int64_t)self->size());
    54	}
    55	
    56	static Value reserve(Machine* vm, int argc, Value* argv) {
    57	    CHECK_SELF();
    58	    if (argc < 2 || !argv[1].is_int()) return Value(null_t{});
    59	    int64_t cap = argv[1].as_int();
    60	    
    61	    if (cap > 0 && static_cast<size_t>(cap) < MAX_ARRAY_CAPACITY) {
    62	        self->reserve(static_cast<size_t>(cap));
    63	    }
    64	    return Value(null_t{});
    65	}
    66	
    67	static Value resize(Machine* vm, int argc, Value* argv) {
    68	    CHECK_SELF();
    69	    if (argc < 2 || !argv[1].is_int()) {
    70	        vm->error("resize expects an integer size.");
    71	        return Value(null_t{});
    72	    }
    73	
    74	    int64_t input_size = argv[1].as_int();
    75	    Value fill_val = (argc > 2) ? argv[2] : Value(null_t{});
    76	
    77	    if (input_size < 0) {
    78	        vm->error("New size cannot be negative.");
    79	        return Value(null_t{});
    80	    }
    81	
    82	    if (static_cast<size_t>(input_size) > MAX_ARRAY_CAPACITY) {
    83	        vm->error(std::format("New size too large ({}). Max allowed: {}", input_size, MAX_ARRAY_CAPACITY));
    84	        return Value(null_t{});
    85	    }
    86	    
    87	    size_t old_size = self->size();
    88	    size_t new_size = static_cast<size_t>(input_size);
    89	    self->resize(new_size);
    90	    
    91	    if (new_size > old_size && !fill_val.is_null()) {
    92	        for(size_t i = old_size; i < new_size; ++i) {
    93	            self->set(i, fill_val);
    94	        }
    95	    }
    96	
    97	    return Value(null_t{});
    98	}
    99	
   100	static Value slice(Machine* vm, int argc, Value* argv) {
   101	    CHECK_SELF();
   102	    
   103	    int64_t len = static_cast<int64_t>(self->size());
   104	    int64_t start = 0;
   105	    int64_t end = len;
   106	
   107	    if (argc >= 2 && argv[1].is_int()) {
   108	        start = argv[1].as_int();
   109	        if (start < 0) start += len;
   110	        if (start < 0) start = 0;
   111	        if (start > len) start = len;
   112	    }
   113	
   114	    if (argc >= 3 && argv[2].is_int()) {
   115	        end = argv[2].as_int();
   116	        if (end < 0) end += len;
   117	        if (end < 0) end = 0;
   118	        if (end > len) end = len;
   119	    }
   120	
   121	    if (start >= end) {
   122	        return Value(vm->get_heap()->new_array());
   123	    }
   124	
   125	    auto new_arr = vm->get_heap()->new_array();
   126	    new_arr->reserve(static_cast<size_t>(end - start));
   127	    
   128	    for (int64_t i = start; i < end; ++i) {
   129	        new_arr->push(self->get(static_cast<size_t>(i)));
   130	    }
   131	    
   132	    return Value(new_arr);
   133	}
   134	
   135	static Value reverse(Machine* vm, int argc, Value* argv) {
   136	    CHECK_SELF();
   137	    std::reverse(self->begin(), self->end());
   138	    return argv[0];
   139	}
   140	
   141	static Value forEach(Machine* vm, int argc, Value* argv) {
   142	    CHECK_SELF();
   143	    if (argc < 2) return Value(null_t{});
   144	    Value callback = argv[1];
   145	
   146	    for (size_t i = 0; i < self->size(); ++i) {
   147	        std::vector<Value> args = { self->get(i), Value((int64_t)i) };
   148	        vm->call_callable(callback, args);
   149	        if (vm->has_error()) return Value(null_t{});
   150	    }
   151	    return Value(null_t{});
   152	}
   153	
   154	static Value map(Machine* vm, int argc, Value* argv) {
   155	    CHECK_SELF();
   156	    if (argc < 2) return Value(null_t{});
   157	    Value callback = argv[1];
   158	
   159	    auto result_arr = vm->get_heap()->new_array();
   160	    result_arr->reserve(self->size());
   161	
   162	    for (size_t i = 0; i < self->size(); ++i) {
   163	        std::vector<Value> args = { self->get(i), Value((int64_t)i) };
   164	        Value res = vm->call_callable(callback, args);
   165	        
   166	        if (vm->has_error()) return Value(null_t{});
   167	        result_arr->push(res);
   168	    }
   169	    return Value(result_arr);
   170	}
   171	
   172	static Value filter(Machine* vm, int argc, Value* argv) {
   173	    CHECK_SELF();
   174	    if (argc < 2) return Value(null_t{});
   175	    Value callback = argv[1];
   176	
   177	    auto result_arr = vm->get_heap()->new_array();
   178	
   179	    for (size_t i = 0; i < self->size(); ++i) {
   180	        Value val = self->get(i);
   181	        std::vector<Value> args = { val, Value((int64_t)i) };
   182	        Value condition = vm->call_callable(callback, args);
   183	        if (vm->has_error()) return Value(null_t{});
   184	        
   185	        if (to_bool(condition)) {
   186	            result_arr->push(val);
   187	        }
   188	    }
   189	    return Value(result_arr);
   190	}
   191	
   192	static Value reduce(Machine* vm, int argc, Value* argv) {
   193	    CHECK_SELF();
   194	    if (argc < 2) return Value(null_t{});
   195	    Value callback = argv[1];
   196	    Value accumulator = (argc > 2) ? argv[2] : Value(null_t{});
   197	    
   198	    size_t start_index = 0;
   199	
   200	    if (argc < 3) {
   201	        if (self->empty()) {
   202	            vm->error("Reduce on empty array with no initial value.");
   203	            return Value(null_t{});
   204	        }
   205	        accumulator = self->get(0);
   206	        start_index = 1;
   207	    }
   208	
   209	    for (size_t i = start_index; i < self->size(); ++i) {
   210	        std::vector<Value> args = { accumulator, self->get(i), Value((int64_t)i) };
   211	        accumulator = vm->call_callable(callback, args);
   212	        if (vm->has_error()) return Value(null_t{});
   213	    }
   214	    return accumulator;
   215	}
   216	
   217	static Value find(Machine* vm, int argc, Value* argv) {
   218	    CHECK_SELF();
   219	    if (argc < 2) return Value(null_t{});
   220	    Value callback = argv[1];
   221	
   222	    for (size_t i = 0; i < self->size(); ++i) {
   223	        Value val = self->get(i);
   224	        std::vector<Value> args = { val, Value((int64_t)i) };
   225	        Value res = vm->call_callable(callback, args);
   226	        if (vm->has_error()) return Value(null_t{});
   227	        
   228	        if (to_bool(res)) return val;
   229	    }
   230	    return Value(null_t{});
   231	}
   232	
   233	static Value findIndex(Machine* vm, int argc, Value* argv) {
   234	    CHECK_SELF();
   235	    if (argc < 2) return Value((int64_t)-1);
   236	    Value callback = argv[1];
   237	
   238	    for (size_t i = 0; i < self->size(); ++i) {
   239	        Value val = self->get(i);
   240	        std::vector<Value> args = { val, Value((int64_t)i) };
   241	        Value res = vm->call_callable(callback, args);
   242	        if (vm->has_error()) return Value((int64_t)-1);
   243	        
   244	        if (to_bool(res)) return Value((int64_t)i);
   245	    }
   246	    return Value((int64_t)-1);
   247	}
   248	
   249	static Value sort(Machine* vm, int argc, Value* argv) {
   250	    CHECK_SELF();
   251	    
   252	    size_t n = self->size();
   253	    if (n < 2) return argv[0];
   254	
   255	    bool has_comparator = (argc > 1);
   256	    Value comparator = has_comparator ? argv[1] : Value(null_t{});
   257	
   258	    for (size_t i = 0; i < n - 1; i++) {
   259	        for (size_t j = 0; j < n - i - 1; j++) {
   260	            Value a = self->get(j);
   261	            Value b = self->get(j + 1);
   262	            bool swap = false;
   263	
   264	            if (has_comparator) {
   265	                std::vector<Value> args = { a, b };
   266	                Value res = vm->call_callable(comparator, args);
   267	                if (vm->has_error()) return Value(null_t{});
   268	                if (res.is_int() && res.as_int() > 0) swap = true; 
   269	                else if (res.is_float() && res.as_float() > 0) swap = true;
   270	            } else {
   271	                if (a.is_int() && b.is_int()) {
   272	                    if (a.as_int() > b.as_int()) swap = true;
   273	                } else if (a.is_float() || b.is_float()) {
   274	                    if (to_float(a) > to_float(b)) swap = true;
   275	                } else {
   276	                    if (std::string_view(to_string(a)) > std::string_view(to_string(b))) swap = true;
   277	                }
   278	            }
   279	
   280	            if (swap) {
   281	                self->set(j, b);
   282	                self->set(j + 1, a);
   283	            }
   284	        }
   285	    }
   286	    return argv[0];
   287	}
   288	
   289	} // namespace meow::natives::array
   290	
   291	namespace meow::stdlib {
   292	module_t create_array_module(Machine* vm, MemoryManager* heap) noexcept {
   293	    auto name = heap->new_string("array");
   294	    auto mod = heap->new_module(name, name);
   295	    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };
   296	
   297	    using namespace meow::natives::array;
   298	    reg("push", push);
   299	    reg("pop", pop);
   300	    reg("clear", clear);
   301	    reg("len", length);
   302	    reg("size", length); 
   303	    reg("length", length);
   304	    reg("resize", resize);
   305	    reg("reserve", reserve);
   306	    reg("slice", slice); 
   307	    
   308	    reg("map", map);
   309	    reg("filter", filter);
   310	    reg("reduce", reduce);
   311	    reg("forEach", forEach);
   312	    reg("find", find);
   313	    reg("findIndex", findIndex);
   314	    reg("reverse", reverse);
   315	    reg("sort", sort);
   316	
   317	    return mod;
   318	}
   319	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/io_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/cast.h>
     7	#include <meow/core/module.h>
     8	
     9	namespace meow::natives::io {
    10	
    11	namespace fs = std::filesystem;
    12	
    13	#define CHECK_ARGS(n) \
    14	    if (argc < n) [[unlikely]] { \
    15	        vm->error("IO Error: Expected at least " #n " arguments."); \
    16	        return Value(null_t{}); \
    17	    }
    18	
    19	#define CHECK_PATH_ARG(idx) \
    20	    if (argc <= idx || !argv[idx].is_string()) [[unlikely]] { \
    21	        vm->error(std::format("IO Error: Argument {} (Path) expects a String, but received {}.", idx, to_string(argv[idx]))); \
    22	        return Value(null_t{}); \
    23	    } \
    24	    const char* path_str_##idx = argv[idx].as_string()->c_str();
    25	
    26	
    27	static Value input(Machine* vm, int argc, Value* argv) {
    28	    if (argc > 0) {
    29	        std::print("{}", to_string(argv[0]));
    30	        std::cout.flush();
    31	    }
    32	    
    33	    std::string line;
    34	    if (std::getline(std::cin, line)) {
    35	        if (!line.empty() && line.back() == '\r') line.pop_back();
    36	        return Value(vm->get_heap()->new_string(line));
    37	    }
    38	    return Value(null_t{});
    39	}
    40	
    41	static Value read_file(Machine* vm, int argc, Value* argv) {
    42	    CHECK_ARGS(1);
    43	    CHECK_PATH_ARG(0);
    44	    
    45	    std::ifstream file(path_str_0, std::ios::binary | std::ios::ate);
    46	    if (!file) return Value(null_t{});
    47	
    48	    auto size = file.tellg();
    49	    if (size == -1) return Value(null_t{});
    50	    
    51	    file.seekg(0);
    52	
    53	    std::string content(static_cast<size_t>(size), '\0');
    54	    if (file.read(content.data(), size)) {
    55	        if (content.size() >= 3 && 
    56	            static_cast<unsigned char>(content[0]) == 0xEF && 
    57	            static_cast<unsigned char>(content[1]) == 0xBB && 
    58	            static_cast<unsigned char>(content[2]) == 0xBF) {
    59	            content.erase(0, 3);
    60	        }
    61	
    62	        return Value(vm->get_heap()->new_string(content));
    63	    }
    64	    return Value(null_t{});
    65	}
    66	
    67	static Value write_file(Machine* vm, int argc, Value* argv) {
    68	    CHECK_ARGS(2);
    69	    CHECK_PATH_ARG(0);
    70	    
    71	    std::string data = to_string(argv[1]);
    72	    bool append = (argc > 2) ? to_bool(argv[2]) : false;
    73	
    74	    auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    75	    std::ofstream file(path_str_0, mode);
    76	    
    77	    return Value(file && (file << data));
    78	}
    79	
    80	static Value file_exists(Machine* vm, int argc, Value* argv) {
    81	    CHECK_ARGS(1);
    82	    CHECK_PATH_ARG(0);
    83	    std::error_code ec;
    84	    return Value(fs::exists(path_str_0, ec));
    85	}
    86	
    87	static Value is_directory(Machine* vm, int argc, Value* argv) {
    88	    CHECK_ARGS(1);
    89	    CHECK_PATH_ARG(0);
    90	    std::error_code ec;
    91	    return Value(fs::is_directory(path_str_0, ec));
    92	}
    93	
    94	static Value list_dir(Machine* vm, int argc, Value* argv) {
    95	    CHECK_ARGS(1);
    96	    CHECK_PATH_ARG(0);
    97	    std::error_code ec;
    98	    
    99	    if (!fs::exists(path_str_0, ec) || !fs::is_directory(path_str_0, ec)) return Value(null_t{});
   100	
   101	    auto arr = vm->get_heap()->new_array();
   102	    
   103	    auto dir_it = fs::directory_iterator(path_str_0, ec);
   104	    if (ec) return Value(null_t{});
   105	
   106	    for (const auto& entry : dir_it) {
   107	        arr->push(Value(vm->get_heap()->new_string(entry.path().filename().string())));
   108	    }
   109	    return Value(arr);
   110	}
   111	
   112	static Value create_dir(Machine* vm, int argc, Value* argv) {
   113	    CHECK_ARGS(1);
   114	    CHECK_PATH_ARG(0);
   115	    std::error_code ec;
   116	    return Value(fs::create_directories(path_str_0, ec));
   117	}
   118	
   119	static Value delete_file(Machine* vm, int argc, Value* argv) {
   120	    CHECK_ARGS(1);
   121	    CHECK_PATH_ARG(0);
   122	    std::error_code ec;
   123	    return Value(fs::remove_all(path_str_0, ec) > 0);
   124	}
   125	
   126	static Value rename_file(Machine* vm, int argc, Value* argv) {
   127	    CHECK_ARGS(2);
   128	    CHECK_PATH_ARG(0); // Source
   129	    CHECK_PATH_ARG(1); // Destination
   130	    std::error_code ec;
   131	    fs::rename(path_str_0, path_str_1, ec);
   132	    return Value(!ec);
   133	}
   134	
   135	static Value copy_file(Machine* vm, int argc, Value* argv) {
   136	    CHECK_ARGS(2);
   137	    CHECK_PATH_ARG(0);
   138	    CHECK_PATH_ARG(1);
   139	    std::error_code ec;
   140	    fs::copy(path_str_0, path_str_1, 
   141	             fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
   142	    return Value(!ec);
   143	}
   144	
   145	static Value get_file_size(Machine* vm, int argc, Value* argv) {
   146	    CHECK_ARGS(1);
   147	    CHECK_PATH_ARG(0);
   148	    std::error_code ec;
   149	    auto sz = fs::file_size(path_str_0, ec);
   150	    if (ec) return Value(static_cast<int64_t>(-1));
   151	    return Value(static_cast<int64_t>(sz));
   152	}
   153	
   154	static Value get_file_timestamp(Machine* vm, int argc, Value* argv) {
   155	    CHECK_ARGS(1);
   156	    CHECK_PATH_ARG(0);
   157	    std::error_code ec;
   158	    auto ftime = fs::last_write_time(path_str_0, ec);
   159	    if (ec) return Value(static_cast<int64_t>(-1));
   160	
   161	    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
   162	        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
   163	    );
   164	    return Value(static_cast<int64_t>(
   165	        std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count()
   166	    ));
   167	}
   168	
   169	static Value get_file_name(Machine* vm, int argc, Value* argv) {
   170	    CHECK_ARGS(1);
   171	    CHECK_PATH_ARG(0);
   172	    fs::path p(path_str_0);
   173	    return Value(vm->get_heap()->new_string(p.filename().string()));
   174	}
   175	
   176	static Value get_file_extension(Machine* vm, int argc, Value* argv) {
   177	    CHECK_ARGS(1);
   178	    CHECK_PATH_ARG(0);
   179	    fs::path p(path_str_0);
   180	    std::string ext = p.extension().string();
   181	    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
   182	    return Value(vm->get_heap()->new_string(ext));
   183	}
   184	
   185	static Value get_file_stem(Machine* vm, int argc, Value* argv) {
   186	    CHECK_ARGS(1);
   187	    CHECK_PATH_ARG(0);
   188	    fs::path p(path_str_0);
   189	    return Value(vm->get_heap()->new_string(p.stem().string()));
   190	}
   191	
   192	static Value get_abs_path(Machine* vm, int argc, Value* argv) {
   193	    CHECK_ARGS(1);
   194	    CHECK_PATH_ARG(0);
   195	    std::error_code ec;
   196	    
   197	    fs::path p = fs::absolute(path_str_0, ec);
   198	    
   199	    if (ec) {
   200	        vm->error(std::format("IO Error: Could not resolve absolute path for '{}'. Error: {}", path_str_0, ec.message()));
   201	        return Value(null_t{});
   202	    }
   203	    
   204	    return Value(vm->get_heap()->new_string(p.string()));
   205	}
   206	
   207	} // namespace meow::natives::io
   208	
   209	namespace meow::stdlib {
   210	
   211	module_t create_io_module(Machine* vm, MemoryManager* heap) noexcept {
   212	    auto name = heap->new_string("io");
   213	    auto mod = heap->new_module(name, name);
   214	
   215	    auto reg = [&](const char* n, native_t fn) {
   216	        mod->set_export(heap->new_string(n), Value(fn));
   217	    };
   218	
   219	    using namespace meow::natives::io;
   220	    
   221	    reg("input", input);
   222	    reg("read", read_file);
   223	    reg("write", write_file);
   224	    
   225	    reg("fileExists", file_exists);
   226	    reg("isDirectory", is_directory);
   227	    reg("listDir", list_dir);
   228	    reg("createDir", create_dir);
   229	    reg("deleteFile", delete_file);
   230	    reg("renameFile", rename_file);
   231	    reg("copyFile", copy_file);
   232	    
   233	    reg("getFileSize", get_file_size);
   234	    reg("getFileTimestamp", get_file_timestamp);
   235	    reg("getFileName", get_file_name);
   236	    reg("getFileExtension", get_file_extension);
   237	    reg("getFileStem", get_file_stem);
   238	    reg("getAbsolutePath", get_abs_path);
   239	
   240	    return mod;
   241	}
   242	
   243	} // namespace meow::stdlib
   244	
   245	// Dọn dẹp macros
   246	#undef CHECK_ARGS
   247	#undef CHECK_PATH_ARG


// =============================================================================
//  FILE PATH: src/vm/stdlib/json_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/memory/gc_disable_guard.h>
     7	#include <meow/core/array.h>
     8	#include <meow/core/hash_table.h>
     9	#include <meow/core/string.h>
    10	#include <meow/core/module.h>
    11	#include <meow/cast.h>
    12	
    13	namespace meow::natives::json {
    14	
    15	class JsonParser {
    16	private:
    17	    std::string_view json_;
    18	    size_t pos_ = 0;
    19	    Machine* vm_;
    20	    bool has_error_ = false;
    21	
    22	    Value report_error() {
    23	        has_error_ = true;
    24	        return Value(null_t{});
    25	    }
    26	
    27	    char peek() const {
    28	        return pos_ < json_.length() ? json_[pos_] : '\0';
    29	    }
    30	
    31	    void advance() {
    32	        if (pos_ < json_.length()) pos_++;
    33	    }
    34	
    35	    void skip_whitespace() {
    36	        while (pos_ < json_.length() && std::isspace(static_cast<unsigned char>(json_[pos_]))) {
    37	            pos_++;
    38	        }
    39	    }
    40	
    41	    Value parse_value();
    42	    Value parse_object();
    43	    Value parse_array();
    44	    Value parse_string();
    45	    Value parse_number();
    46	    Value parse_true();
    47	    Value parse_false();
    48	    Value parse_null();
    49	
    50	public:
    51	    explicit JsonParser(Machine* vm) : vm_(vm) {}
    52	
    53	    Value parse(std::string_view str) {
    54	        json_ = str;
    55	        pos_ = 0;
    56	        has_error_ = false;
    57	        
    58	        skip_whitespace();
    59	        if (json_.empty()) return Value(null_t{});
    60	
    61	        Value result = parse_value();
    62	        
    63	        if (has_error_) return Value(null_t{});
    64	
    65	        skip_whitespace();
    66	        if (pos_ < json_.length()) {
    67	            return report_error();
    68	        }
    69	        return result;
    70	    }
    71	};
    72	
    73	Value JsonParser::parse_value() {
    74	    skip_whitespace();
    75	    if (pos_ >= json_.length()) return report_error();
    76	
    77	    char c = peek();
    78	    switch (c) {
    79	        case '{': return parse_object();
    80	        case '[': return parse_array();
    81	        case '"': return parse_string();
    82	        case 't': return parse_true();
    83	        case 'f': return parse_false();
    84	        case 'n': return parse_null();
    85	        default:
    86	            if (std::isdigit(c) || c == '-') {
    87	                return parse_number();
    88	            }
    89	            return report_error();
    90	    }
    91	}
    92	
    93	Value JsonParser::parse_object() {
    94	    advance();
    95	    
    96	    auto hash = vm_->get_heap()->new_hash();
    97	
    98	    skip_whitespace();
    99	    if (peek() == '}') {
   100	        advance();
   101	        return Value(hash);
   102	    }
   103	
   104	    while (true) {
   105	        skip_whitespace();
   106	        if (peek() != '"') return report_error();
   107	
   108	        Value key_val = parse_string();
   109	        if (has_error_) return key_val;
   110	
   111	        string_t key_str = key_val.as_string();
   112	
   113	        skip_whitespace();
   114	        if (peek() != ':') return report_error();
   115	        advance();
   116	
   117	        Value val = parse_value();
   118	        if (has_error_) return val;
   119	
   120	        hash->set(key_str, val);
   121	
   122	        skip_whitespace();
   123	        char next = peek();
   124	        if (next == '}') {
   125	            advance();
   126	            break;
   127	        }
   128	        if (next != ',') return report_error();
   129	        advance();
   130	    }
   131	    return Value(hash);
   132	}
   133	
   134	Value JsonParser::parse_array() {
   135	    advance();
   136	    
   137	    auto arr = vm_->get_heap()->new_array();
   138	
   139	    skip_whitespace();
   140	    if (peek() == ']') {
   141	        advance();
   142	        return Value(arr);
   143	    }
   144	
   145	    while (true) {
   146	        Value elem = parse_value();
   147	        if (has_error_) return elem;
   148	        
   149	        arr->push(elem);
   150	
   151	        skip_whitespace();
   152	        char next = peek();
   153	        if (next == ']') {
   154	            advance();
   155	            break;
   156	        }
   157	        if (next != ',') return report_error();
   158	        advance();
   159	    }
   160	    return Value(arr);
   161	}
   162	
   163	Value JsonParser::parse_string() {
   164	    advance();
   165	    std::string s;
   166	    s.reserve(32);
   167	
   168	    while (pos_ < json_.length() && peek() != '"') {
   169	        if (peek() == '\\') {
   170	            advance();
   171	            if (pos_ >= json_.length()) return report_error();
   172	            
   173	            char escaped = peek();
   174	            switch (escaped) {
   175	                case '"':  s += '"'; break;
   176	                case '\\': s += '\\'; break;
   177	                case '/':  s += '/'; break;
   178	                case 'b':  s += '\b'; break;
   179	                case 'f':  s += '\f'; break;
   180	                case 'n':  s += '\n'; break;
   181	                case 'r':  s += '\r'; break;
   182	                case 't':  s += '\t'; break;
   183	                case 'u': {
   184	                    advance(); // Skip 'u'
   185	                    if (pos_ + 4 > json_.length()) return report_error();
   186	
   187	                    unsigned int codepoint = 0;
   188	                    for (int i = 0; i < 4; ++i) {
   189	                        char h = peek();
   190	                        advance();
   191	                        codepoint <<= 4;
   192	                        if (h >= '0' && h <= '9') codepoint |= (h - '0');
   193	                        else if (h >= 'a' && h <= 'f') codepoint |= (10 + h - 'a');
   194	                        else if (h >= 'A' && h <= 'F') codepoint |= (10 + h - 'A');
   195	                        else return report_error();
   196	                    }
   197	
   198	                    if (codepoint <= 0x7F) {
   199	                        s += static_cast<char>(codepoint);
   200	                    } else if (codepoint <= 0x7FF) {
   201	                        s += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
   202	                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
   203	                    } else {
   204	                        s += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
   205	                        s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
   206	                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
   207	                    }
   208	                    continue;
   209	                }
   210	                default:
   211	                    s += escaped; break;
   212	            }
   213	        } else {
   214	            s += peek();
   215	        }
   216	        advance();
   217	    }
   218	
   219	    if (pos_ >= json_.length() || peek() != '"') {
   220	        return report_error();
   221	    }
   222	    advance();
   223	
   224	    return Value(vm_->get_heap()->new_string(s));
   225	}
   226	
   227	Value JsonParser::parse_number() {
   228	    size_t start = pos_;
   229	    if (peek() == '-') advance();
   230	
   231	    if (peek() == '0') {
   232	        advance();
   233	    } else if (std::isdigit(peek())) {
   234	        while (pos_ < json_.length() && std::isdigit(peek())) advance();
   235	    } else {
   236	        return report_error();
   237	    }
   238	
   239	    bool is_float = false;
   240	    if (pos_ < json_.length() && peek() == '.') {
   241	        is_float = true;
   242	        advance();
   243	        if (!std::isdigit(peek())) return report_error();
   244	        while (pos_ < json_.length() && std::isdigit(peek())) advance();
   245	    }
   246	
   247	    if (pos_ < json_.length() && (peek() == 'e' || peek() == 'E')) {
   248	        is_float = true;
   249	        advance();
   250	        if (pos_ < json_.length() && (peek() == '+' || peek() == '-')) advance();
   251	        if (!std::isdigit(peek())) return report_error();
   252	        while (pos_ < json_.length() && std::isdigit(peek())) advance();
   253	    }
   254	
   255	    std::string num_str(json_.substr(start, pos_ - start));
   256	    
   257	    if (is_float) {
   258	        return Value(std::stod(num_str));
   259	    } else {
   260	        return Value(static_cast<int64_t>(std::stoll(num_str)));
   261	    }
   262	}
   263	
   264	Value JsonParser::parse_true() {
   265	    if (json_.substr(pos_, 4) == "true") {
   266	        pos_ += 4;
   267	        return Value(true);
   268	    }
   269	    return report_error();
   270	}
   271	
   272	Value JsonParser::parse_false() {
   273	    if (json_.substr(pos_, 5) == "false") {
   274	        pos_ += 5;
   275	        return Value(false);
   276	    }
   277	    return report_error();
   278	}
   279	
   280	Value JsonParser::parse_null() {
   281	    if (json_.substr(pos_, 4) == "null") {
   282	        pos_ += 4;
   283	        return Value(null_t{});
   284	    }
   285	    return report_error();
   286	}
   287	
   288	// ============================================================================
   289	// 🖨️ JSON STRINGIFIER
   290	// ============================================================================
   291	
   292	static std::string escape_string(std::string_view s) {
   293	    std::ostringstream o;
   294	    o << '"';
   295	    for (unsigned char c : s) {
   296	        switch (c) {
   297	            case '"':  o << "\\\""; break;
   298	            case '\\': o << "\\\\"; break;
   299	            case '\b': o << "\\b"; break;
   300	            case '\f': o << "\\f"; break;
   301	            case '\n': o << "\\n"; break;
   302	            case '\r': o << "\\r"; break;
   303	            case '\t': o << "\\t"; break;
   304	            default:
   305	                if (c <= 0x1F) {
   306	                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
   307	                } else {
   308	                    o << c;
   309	                }
   310	        }
   311	    }
   312	    o << '"';
   313	    return o.str();
   314	}
   315	
   316	static std::string to_json_recursive(const Value& val, int indent_level, int tab_size) {
   317	    std::ostringstream ss;
   318	    std::string indent(indent_level * tab_size, ' ');
   319	    std::string inner_indent((indent_level + 1) * tab_size, ' ');
   320	    bool pretty = (tab_size > 0);
   321	    const char* newline = pretty ? "\n" : "";
   322	    const char* sep = pretty ? ": " : ":";
   323	
   324	    if (val.is_null()) {
   325	        ss << "null";
   326	    } 
   327	    else if (val.is_bool()) {
   328	        ss << (val.as_bool() ? "true" : "false");
   329	    } 
   330	    else if (val.is_int()) {
   331	        ss << val.as_int();
   332	    } 
   333	    else if (val.is_float()) {
   334	        ss << val.as_float();
   335	    } 
   336	    else if (val.is_string()) {
   337	        ss << escape_string(val.as_string()->c_str());
   338	    } 
   339	    else if (val.is_array()) {
   340	        array_t arr = val.as_array();
   341	        if (arr->empty()) {
   342	            ss << "[]";
   343	        } else {
   344	            ss << "[" << newline;
   345	            for (size_t i = 0; i < arr->size(); ++i) {
   346	                if (pretty) ss << inner_indent;
   347	                ss << to_json_recursive(arr->get(i), indent_level + 1, tab_size);
   348	                if (i + 1 < arr->size()) ss << ",";
   349	                ss << newline;
   350	            }
   351	            if (pretty) ss << indent;
   352	            ss << "]";
   353	        }
   354	    } 
   355	    else if (val.is_hash_table()) {
   356	        hash_table_t obj = val.as_hash_table();
   357	        if (obj->empty()) {
   358	            ss << "{}";
   359	        } else {
   360	            ss << "{" << newline;
   361	            size_t i = 0;
   362	            size_t size = obj->size();
   363	            for (auto it = obj->begin(); it != obj->end(); ++it) {
   364	                if (pretty) ss << inner_indent;
   365	                ss << escape_string(it->first->c_str()) << sep;
   366	                ss << to_json_recursive(it->second, indent_level + 1, tab_size);
   367	                if (i + 1 < size) ss << ",";
   368	                ss << newline;
   369	                i++;
   370	            }
   371	            if (pretty) ss << indent;
   372	            ss << "}";
   373	        }
   374	    } 
   375	    else {
   376	        ss << "\"<unsupported_type>\"";
   377	    }
   378	    
   379	    return ss.str();
   380	}
   381	
   382	static Value json_parse(Machine* vm, int argc, Value* argv) {
   383	    if (argc < 1 || !argv[0].is_string()) {
   384	        return Value(null_t{});
   385	    }
   386	
   387	    std::string_view json_str = argv[0].as_string()->c_str();
   388	    JsonParser parser(vm);
   389	    return parser.parse(json_str);
   390	}
   391	
   392	static Value json_stringify(Machine* vm, int argc, Value* argv) {
   393	    if (argc < 1) return Value(null_t{});
   394	    
   395	    int tab_size = 2;
   396	    if (argc > 1 && argv[1].is_int()) {
   397	        tab_size = static_cast<int>(argv[1].as_int());
   398	        if (tab_size < 0) tab_size = 0;
   399	    }
   400	    
   401	    std::string res = to_json_recursive(argv[0], 0, tab_size);
   402	    return Value(vm->get_heap()->new_string(res));
   403	}
   404	
   405	} // namespace meow::natives::json
   406	
   407	namespace meow::stdlib {
   408	
   409	module_t create_json_module(Machine* vm, MemoryManager* heap) noexcept {
   410	    auto name = heap->new_string("json");
   411	    auto mod = heap->new_module(name, name);
   412	
   413	    auto reg = [&](const char* n, native_t fn) {
   414	        mod->set_export(heap->new_string(n), Value(fn));
   415	    };
   416	
   417	    using namespace meow::natives::json;
   418	    reg("parse", json_parse);
   419	    reg("stringify", json_stringify);
   420	
   421	    return mod;
   422	}
   423	
   424	} // namespace meow::stdlib


// =============================================================================
//  FILE PATH: src/vm/stdlib/memory_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <cstdlib>
     4	#include <meow/machine.h>
     5	#include <meow/value.h>
     6	#include <meow/memory/memory_manager.h>
     7	
     8	namespace meow::stdlib {
     9	
    10	// malloc(size: int) -> pointer
    11	static Value malloc(Machine* vm, int argc, Value* argv) {
    12	    if (argc < 1 || !argv[0].is_int()) [[unlikely]] {
    13	        return Value(); 
    14	    }
    15	
    16	    size_t size = static_cast<size_t>(argv[0].as_int());
    17	    
    18	    if (size == 0) return Value();
    19	
    20	    void* buffer = std::malloc(size);
    21	    return Value(buffer);
    22	} 
    23	
    24	// free(ptr: pointer) -> null
    25	static Value free(Machine* vm, int argc, Value* argv) {
    26	    if (argc >= 1 && argv[0].is_pointer()) [[likely]] {
    27	        void* ptr = argv[0].as_pointer();
    28	        if (ptr) std::free(ptr);
    29	    }
    30	    return Value();
    31	}
    32	    
    33	module_t create_memory_module(Machine* vm, MemoryManager* heap) noexcept {
    34	    auto name = heap->new_string("memory");
    35	    auto mod = heap->new_module(name, name);
    36	    
    37	    auto reg = [&](const char* n, native_t fn) { 
    38	        mod->set_export(heap->new_string(n), Value(fn)); 
    39	    };
    40	
    41	    reg("malloc", malloc);
    42	    reg("free", free);
    43	
    44	    return mod;
    45	}
    46	
    47	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/object_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/memory/memory_manager.h>
     5	#include <meow/memory/gc_disable_guard.h>
     6	#include <meow/core/module.h>
     7	#include <meow/core/hash_table.h>
     8	#include <meow/core/array.h>
     9	
    10	namespace meow::natives::obj {
    11	
    12	#define CHECK_SELF() \
    13	    if (argc < 1 || !argv[0].is_hash_table()) [[unlikely]] { \
    14	        vm->error("Object method expects 'this' to be a Hash Table."); \
    15	        return Value(null_t{}); \
    16	    } \
    17	    hash_table_t self = argv[0].as_hash_table(); \
    18	
    19	static Value keys(Machine* vm, int argc, Value* argv) {
    20	    CHECK_SELF();
    21	    auto arr = vm->get_heap()->new_array();
    22	    arr->reserve(self->size());
    23	    for(auto it = self->begin(); it != self->end(); ++it) {
    24	        arr->push(Value(it->first));
    25	    }
    26	    return Value(arr);
    27	}
    28	
    29	static Value values(Machine* vm, int argc, Value* argv) {
    30	    CHECK_SELF();
    31	    auto arr = vm->get_heap()->new_array();
    32	    arr->reserve(self->size());
    33	    for(auto it = self->begin(); it != self->end(); ++it) {
    34	        arr->push(it->second);
    35	    }
    36	    return Value(arr);
    37	}
    38	
    39	static Value entries(Machine* vm, int argc, Value* argv) {
    40	    CHECK_SELF();
    41	    auto arr = vm->get_heap()->new_array();
    42	    arr->reserve(self->size());
    43	    
    44	    for(auto it = self->begin(); it != self->end(); ++it) {
    45	        auto pair = vm->get_heap()->new_array();
    46	        pair->push(Value(it->first));
    47	        pair->push(it->second);
    48	        arr->push(Value(pair));
    49	    }
    50	    return Value(arr);
    51	}
    52	
    53	static Value has(Machine* vm, int argc, Value* argv) {
    54	    CHECK_SELF();
    55	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    56	    return Value(self->has(argv[1].as_string()));
    57	}
    58	
    59	static Value len(Machine* vm, int argc, Value* argv) {
    60	    CHECK_SELF();
    61	    return Value((int64_t)self->size());
    62	}
    63	
    64	static Value merge(Machine* vm, int argc, Value* argv) {    
    65	    auto result = vm->get_heap()->new_hash();
    66	    
    67	    for (int i = 0; i < argc; ++i) {
    68	        if (argv[i].is_hash_table()) {
    69	            hash_table_t src = argv[i].as_hash_table();
    70	            for (auto it = src->begin(); it != src->end(); ++it) {
    71	                result->set(it->first, it->second);
    72	            }
    73	        }
    74	    }
    75	    return Value(result);
    76	}
    77	
    78	} // namespace
    79	
    80	namespace meow::stdlib {
    81	module_t create_object_module(Machine* vm, MemoryManager* heap) noexcept {
    82	    auto name = heap->new_string("object");
    83	    auto mod = heap->new_module(name, name);
    84	    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };
    85	
    86	    using namespace meow::natives::obj;
    87	    reg("keys", keys);
    88	    reg("values", values);
    89	    reg("entries", entries);
    90	    reg("has", has);
    91	    reg("len", len);
    92	    reg("merge", merge);
    93	    
    94	    return mod;
    95	}
    96	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/stdlib.h
// =============================================================================

     1	/**
     2	 * @file stdlib.h
     3	 * @brief Factory definitions for Standard Libraries
     4	 * @note  Zero-overhead abstractions.
     5	 */
     6	#pragma once
     7	
     8	#include <meow/common.h>
     9	
    10	namespace meow {
    11	    class Machine;
    12	    class MemoryManager;
    13	}
    14	
    15	namespace meow::stdlib {
    16	    // Factory functions - Return raw Module Object pointer
    17	    [[nodiscard]] module_t create_io_module(Machine* vm, MemoryManager* heap) noexcept;
    18	    [[nodiscard]] module_t create_system_module(Machine* vm, MemoryManager* heap) noexcept;
    19	    [[nodiscard]] module_t create_array_module(Machine* vm, MemoryManager* heap) noexcept;
    20	    [[nodiscard]] module_t create_string_module(Machine* vm, MemoryManager* heap) noexcept;
    21	    [[nodiscard]] module_t create_object_module(Machine* vm, MemoryManager* heap) noexcept;
    22	    [[nodiscard]] module_t create_json_module(Machine* vm, MemoryManager* heap) noexcept;
    23	    [[nodiscard]] module_t create_memory_module(Machine* vm, MemoryManager* heap) noexcept;
    24	}



// =============================================================================
//  FILE PATH: src/vm/stdlib/string_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/memory/memory_manager.h>
     5	#include <meow/memory/gc_disable_guard.h>
     6	#include <meow/core/module.h>
     7	#include <meow/core/array.h>
     8	#include <meow/cast.h>
     9	
    10	namespace meow::natives::str {
    11	
    12	#define CHECK_SELF() \
    13	    if (argc < 1 || !argv[0].is_string()) [[unlikely]] { \
    14	        vm->error("String method expects 'this' to be a String."); \
    15	        return Value(null_t{}); \
    16	    } \
    17	    string_t self_obj = argv[0].as_string(); \
    18	    std::string_view self(self_obj->c_str(), self_obj->size()); \
    19	
    20	static Value len(Machine* vm, int argc, Value* argv) {
    21	    CHECK_SELF();
    22	    return Value((int64_t)self.size());
    23	}
    24	
    25	static Value upper(Machine* vm, int argc, Value* argv) {
    26	    CHECK_SELF();
    27	    std::string s(self);
    28	    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    29	    return Value(vm->get_heap()->new_string(s));
    30	}
    31	
    32	static Value lower(Machine* vm, int argc, Value* argv) {
    33	    CHECK_SELF();
    34	    std::string s(self);
    35	    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    36	    return Value(vm->get_heap()->new_string(s));
    37	}
    38	
    39	static Value trim(Machine* vm, int argc, Value* argv) {
    40	    CHECK_SELF();
    41	    while (!self.empty() && std::isspace(self.front())) self.remove_prefix(1);
    42	    while (!self.empty() && std::isspace(self.back())) self.remove_suffix(1);
    43	    return Value(vm->get_heap()->new_string(self));
    44	}
    45	
    46	static Value contains(Machine* vm, int argc, Value* argv) {
    47	    CHECK_SELF();
    48	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    49	    string_t needle_obj = argv[1].as_string();
    50	    std::string_view needle(needle_obj->c_str(), needle_obj->size());
    51	    return Value(self.find(needle) != std::string::npos);
    52	}
    53	
    54	static Value startsWith(Machine* vm, int argc, Value* argv) {
    55	    CHECK_SELF();
    56	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    57	    string_t prefix_obj = argv[1].as_string();
    58	    std::string_view prefix(prefix_obj->c_str(), prefix_obj->size());
    59	    return Value(self.starts_with(prefix));
    60	}
    61	
    62	static Value endsWith(Machine* vm, int argc, Value* argv) {
    63	    CHECK_SELF();
    64	    if (argc < 2 || !argv[1].is_string()) return Value(false);
    65	    string_t suffix_obj = argv[1].as_string();
    66	    std::string_view suffix(suffix_obj->c_str(), suffix_obj->size());
    67	    return Value(self.ends_with(suffix));
    68	}
    69	
    70	static Value join(Machine* vm, int argc, Value* argv) {
    71	    CHECK_SELF();
    72	    if (argc < 2 || !argv[1].is_array()) return Value(vm->get_heap()->new_string(""));
    73	
    74	    array_t arr = argv[1].as_array();
    75	    std::ostringstream ss;
    76	    for (size_t i = 0; i < arr->size(); ++i) {
    77	        if (i > 0) ss << self;
    78	        Value item = arr->get(i);
    79	        if (item.is_string()) ss << item.as_string()->c_str();
    80	        else ss << to_string(item);
    81	    }
    82	    return Value(vm->get_heap()->new_string(ss.str()));
    83	}
    84	
    85	static Value split(Machine* vm, int argc, Value* argv) {
    86	    CHECK_SELF();
    87	    std::string_view delim = " ";
    88	    if (argc >= 2 && argv[1].is_string()) {
    89	        string_t d = argv[1].as_string();
    90	        delim = std::string_view(d->c_str(), d->size());
    91	    }
    92	
    93	    auto arr = vm->get_heap()->new_array();
    94	    
    95	    if (delim.empty()) {
    96	        for (char c : self) {
    97	            arr->push(Value(vm->get_heap()->new_string(&c, 1)));
    98	        }
    99	    } else {
   100	        size_t start = 0;
   101	        size_t end = self.find(delim);
   102	        while (end != std::string::npos) {
   103	            std::string_view token = self.substr(start, end - start);
   104	            arr->push(Value(vm->get_heap()->new_string(token)));
   105	            start = end + delim.length();
   106	            end = self.find(delim, start);
   107	        }
   108	        std::string_view last = self.substr(start);
   109	        arr->push(Value(vm->get_heap()->new_string(last)));
   110	    }
   111	    return Value(arr);
   112	}
   113	
   114	static Value replace(Machine* vm, int argc, Value* argv) {
   115	    CHECK_SELF();
   116	    if (argc < 3 || !argv[1].is_string() || !argv[2].is_string()) {
   117	        return argv[0];
   118	    }
   119	    string_t from_obj = argv[1].as_string();
   120	    string_t to_obj = argv[2].as_string();
   121	    
   122	    std::string_view from(from_obj->c_str(), from_obj->size());
   123	    std::string_view to(to_obj->c_str(), to_obj->size());
   124	    
   125	    std::string s(self);
   126	    size_t pos = s.find(from);
   127	    if (pos != std::string::npos) {
   128	        s.replace(pos, from.length(), to);
   129	    }
   130	    return Value(vm->get_heap()->new_string(s));
   131	}
   132	
   133	static Value indexOf(Machine* vm, int argc, Value* argv) {
   134	    CHECK_SELF();
   135	    if (argc < 2 || !argv[1].is_string()) return Value((int64_t)-1);
   136	    
   137	    string_t sub_obj = argv[1].as_string();
   138	    size_t start_pos = 0;
   139	    if (argc > 2 && argv[2].is_int()) {
   140	        int64_t p = argv[2].as_int();
   141	        if (p > 0) start_pos = static_cast<size_t>(p);
   142	    }
   143	
   144	    size_t pos = self.find(sub_obj->c_str(), start_pos);
   145	    if (pos == std::string::npos) return Value((int64_t)-1);
   146	    return Value((int64_t)pos);
   147	}
   148	
   149	static Value lastIndexOf(Machine* vm, int argc, Value* argv) {
   150	    CHECK_SELF();
   151	    if (argc < 2 || !argv[1].is_string()) return Value((int64_t)-1);
   152	    
   153	    string_t sub_obj = argv[1].as_string();
   154	    size_t pos = self.rfind(sub_obj->c_str());
   155	    if (pos == std::string::npos) return Value((int64_t)-1);
   156	    return Value((int64_t)pos);
   157	}
   158	
   159	static Value substring(Machine* vm, int argc, Value* argv) {
   160	    CHECK_SELF();
   161	    if (argc < 2 || !argv[1].is_int()) return argv[0];
   162	    
   163	    int64_t start = argv[1].as_int();
   164	    int64_t length = (int64_t)self.size();
   165	    
   166	    if (argc > 2 && argv[2].is_int()) {
   167	        length = argv[2].as_int();
   168	    }
   169	    
   170	    if (start < 0) start = 0;
   171	    if (start >= (int64_t)self.size()) return Value(vm->get_heap()->new_string(""));
   172	    
   173	    return Value(vm->get_heap()->new_string(self.substr(start, length)));
   174	}
   175	
   176	static Value slice_str(Machine* vm, int argc, Value* argv) {
   177	    CHECK_SELF();
   178	    int64_t len = (int64_t)self.size();
   179	    int64_t start = 0;
   180	    int64_t end = len;
   181	
   182	    if (argc >= 2 && argv[1].is_int()) {
   183	        start = argv[1].as_int();
   184	        if (start < 0) start += len;
   185	        if (start < 0) start = 0;
   186	    }
   187	    if (argc >= 3 && argv[2].is_int()) {
   188	        end = argv[2].as_int();
   189	        if (end < 0) end += len;
   190	    }
   191	    if (start >= end || start >= len) return Value(vm->get_heap()->new_string(""));
   192	    if (end > len) end = len;
   193	
   194	    return Value(vm->get_heap()->new_string(self.substr(start, end - start)));
   195	}
   196	
   197	static Value repeat(Machine* vm, int argc, Value* argv) {
   198	    CHECK_SELF();
   199	    if (argc < 2 || !argv[1].is_int()) return Value(vm->get_heap()->new_string(""));
   200	    int64_t count = argv[1].as_int();
   201	    if (count <= 0) return Value(vm->get_heap()->new_string(""));
   202	    
   203	    std::string res;
   204	    res.reserve(self.size() * count);
   205	    for(int i=0; i<count; ++i) res.append(self);
   206	    
   207	    return Value(vm->get_heap()->new_string(res));
   208	}
   209	
   210	static Value padLeft(Machine* vm, int argc, Value* argv) {
   211	    CHECK_SELF();
   212	    if (argc < 2 || !argv[1].is_int()) return argv[0];
   213	    int64_t target_len_i64 = argv[1].as_int();
   214	    if (target_len_i64 < 0) return argv[0];
   215	    size_t target_len = static_cast<size_t>(target_len_i64);
   216	
   217	    if (target_len <= self.size()) return argv[0];
   218	    
   219	    std::string_view pad_char = " ";
   220	    if (argc > 2 && argv[2].is_string()) {
   221	        string_t p = argv[2].as_string();
   222	        if (!p->empty()) pad_char = std::string_view(p->c_str(), p->size());
   223	    }
   224	
   225	    std::string res;
   226	    size_t needed_len = target_len - self.size();
   227	    while (res.size() < needed_len) res.append(pad_char);
   228	    res.resize(needed_len); 
   229	    res.append(self);
   230	    
   231	    return Value(vm->get_heap()->new_string(res));
   232	}
   233	
   234	static Value padRight(Machine* vm, int argc, Value* argv) {
   235	    CHECK_SELF();
   236	    if (argc < 2 || !argv[1].is_int()) return argv[0];
   237	    int64_t target_len_i64 = argv[1].as_int();
   238	    if (target_len_i64 < 0) return argv[0];
   239	    size_t target_len = static_cast<size_t>(target_len_i64);
   240	
   241	    if (target_len <= self.size()) return argv[0];
   242	    
   243	    std::string_view pad_char = " ";
   244	    if (argc > 2 && argv[2].is_string()) {
   245	        string_t p = argv[2].as_string();
   246	        if (!p->empty()) pad_char = std::string_view(p->c_str(), p->size());
   247	    }
   248	
   249	    std::string res(self);
   250	    while (res.size() < target_len) res.append(pad_char);
   251	    res.resize(target_len);
   252	    
   253	    return Value(vm->get_heap()->new_string(res));
   254	}
   255	
   256	static Value equalsIgnoreCase(Machine* vm, int argc, Value* argv) {
   257	    CHECK_SELF();
   258	    if (argc < 2 || !argv[1].is_string()) return Value(false);
   259	    string_t other = argv[1].as_string();
   260	    if (self.size() != other->size()) return Value(false);
   261	    
   262	    return Value(std::equal(self.begin(), self.end(), other->c_str(), 
   263	        [](char a, char b) { return tolower(a) == tolower(b); }));
   264	}
   265	
   266	static Value charAt(Machine* vm, int argc, Value* argv) {
   267	    CHECK_SELF();
   268	    if (argc < 2 || !argv[1].is_int()) return Value(vm->get_heap()->new_string(""));
   269	    int64_t idx = argv[1].as_int();
   270	    if (idx < 0 || idx >= (int64_t)self.size()) return Value(vm->get_heap()->new_string(""));
   271	    
   272	    char c = self[idx];
   273	    return Value(vm->get_heap()->new_string(&c, 1));
   274	}
   275	
   276	static Value charCodeAt(Machine* vm, int argc, Value* argv) {
   277	    CHECK_SELF();
   278	    if (argc < 2 || !argv[1].is_int()) return Value((int64_t)-1);
   279	    int64_t idx = argv[1].as_int();
   280	    if (idx < 0 || idx >= (int64_t)self.size()) return Value((int64_t)-1);
   281	    
   282	    return Value((int64_t)(unsigned char)self[idx]);
   283	}
   284	
   285	} // namespace meow::natives::str
   286	
   287	namespace meow::stdlib {
   288	module_t create_string_module(Machine* vm, MemoryManager* heap) noexcept {
   289	    auto name = heap->new_string("string");
   290	    auto mod = heap->new_module(name, name);
   291	    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };
   292	
   293	    using namespace meow::natives::str;
   294	    reg("len", len);
   295	    reg("size", len);
   296	    reg("length", len);
   297	    
   298	    reg("upper", upper);
   299	    reg("lower", lower);
   300	    reg("trim", trim);
   301	    
   302	    reg("contains", contains);
   303	    reg("startsWith", startsWith);
   304	    reg("endsWith", endsWith);
   305	    reg("join", join);
   306	    reg("split", split);
   307	    reg("replace", replace);
   308	    reg("indexOf", indexOf);
   309	    reg("lastIndexOf", lastIndexOf);
   310	    reg("substring", substring);
   311	    reg("slice", slice_str);
   312	    reg("repeat", repeat);
   313	    reg("padLeft", padLeft);
   314	    reg("padRight", padRight);
   315	    reg("equalsIgnoreCase", equalsIgnoreCase);
   316	    reg("charAt", charAt);
   317	    reg("charCodeAt", charCodeAt);
   318	    
   319	    return mod;
   320	}
   321	}


// =============================================================================
//  FILE PATH: src/vm/stdlib/system_lib.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "vm/stdlib/stdlib.h"
     3	#include <meow/machine.h>
     4	#include <meow/value.h>
     5	#include <meow/memory/memory_manager.h>
     6	#include <meow/cast.h>
     7	#include <meow/core/module.h>
     8	
     9	namespace meow::natives::sys {
    10	
    11	// system.argv()
    12	static Value get_argv(Machine* vm, int, Value*) {
    13	    const auto& cmd_args = vm->get_args().command_line_arguments_; 
    14	    auto arr = vm->get_heap()->new_array();
    15	    arr->reserve(cmd_args.size());
    16	    
    17	    for (const auto& arg : cmd_args) {
    18	        arr->push(Value(vm->get_heap()->new_string(arg)));
    19	    }
    20	    return Value(arr);
    21	}
    22	
    23	// system.exit(code)
    24	static Value exit_vm(Machine*, int argc, Value* argv) {
    25	    int code = 0;
    26	    if (argc > 0) code = static_cast<int>(to_int(argv[0]));
    27	    std::exit(code);
    28	    std::unreachable();
    29	}
    30	
    31	// system.exec(command)
    32	static Value exec_cmd(Machine* vm, int argc, Value* argv) {
    33	    if (argc < 1) [[unlikely]] return Value(static_cast<int64_t>(-1));
    34	    const char* cmd = argv[0].as_string()->c_str();
    35	    int code = std::system(cmd);
    36	    return Value(static_cast<int64_t>(code));
    37	}
    38	
    39	// system.time() -> ms
    40	static Value time_now(Machine*, int, Value*) {
    41	    auto now = std::chrono::system_clock::now();
    42	    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    43	    return Value(static_cast<int64_t>(ms));
    44	}
    45	
    46	// system.env(name)
    47	static Value get_env(Machine* vm, int argc, Value* argv) {
    48	    if (argc < 1) [[unlikely]] return Value(null_t{});
    49	    const char* val = std::getenv(argv[0].as_string()->c_str());
    50	    if (val) return Value(vm->get_heap()->new_string(val));
    51	    return Value(null_t{});
    52	}
    53	
    54	} // namespace meow::natives::sys
    55	
    56	namespace meow::stdlib {
    57	
    58	module_t create_system_module(Machine* vm, MemoryManager* heap) noexcept {
    59	    auto name = heap->new_string("system");
    60	    auto mod = heap->new_module(name, name);
    61	
    62	    auto reg = [&](const char* n, native_t fn) {
    63	        mod->set_export(heap->new_string(n), Value(fn));
    64	    };
    65	
    66	    using namespace meow::natives::sys;
    67	    reg("argv", get_argv);
    68	    reg("exit", exit_vm);
    69	    reg("exec", exec_cmd);
    70	    reg("time", time_now);
    71	    reg("env", get_env);
    72	
    73	    return mod;
    74	}
    75	
    76	} // namespace meow::stdlib


