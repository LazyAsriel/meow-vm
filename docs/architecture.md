# 🏛️ MEOW-VM ARCHITECTURE & INTERNALS

> **Phiên bản:** 1.0 (Draft)  
> **Ngôn ngữ:** C++23  
> **Kiến trúc:** Register-based VM + Template JIT (x64)  
> **Mục tiêu:** Máy ảo ngôn ngữ động hiệu năng cao, tối ưu hóa bộ nhớ.

---

## 1. 🗺️ Tổng quan hệ thống (System Overview)

`meow-vm` không chỉ là một trình thông dịch (interpreter) đơn thuần, mà là một hệ thống runtime hoàn chỉnh bao gồm:
1.  **Compiler Chain:** Từ Source (`.meow`) -> Assembly (`masm`) -> Bytecode (`.meowc`).
2.  **Smart Loader:** Linker tĩnh giúp tối ưu hóa truy cập global và constant ngay thời điểm load.
3.  **High-Performance Runtime:**
    * **Direct Threaded Code:** Dispatch lệnh cực nhanh bằng `[[clang::musttail]]`.
    * **JIT Compiler:** Biến mã nóng (hot code) thành mã máy x64 native.
    * **Generational GC:** Quản lý bộ nhớ tự động hiệu quả, giảm pause time.

---

## 2. 🌳 Cấu trúc thư mục (Project Tree)

Đây là bản đồ toàn bộ mã nguồn của dự án:

root/
├── .clang-format           # Quy tắc định dạng code (Google Style + Custom)
├── .gitignore              # File, thư mục bỏ qua của Git
├── CMakeLists.txt          # Cấu hình build chính (Root)
├── CMakePresets.json       # Cấu hình preset build (Debug/Release)
├── benchmarks/             # [Test] Các bài test hiệu năng
│   ├── dispatch_bench.cpp  # Test tốc độ dispatch instruction
│   ├── vm_vs_native.cpp    # So sánh tốc độ VM vs C++ thuần
│   └── make_chunk.h        # Helper tạo bytecode thủ công
├── docs/                   # Tài liệu hướng dẫn (Stdlib, Lang spec)
├── include/meow/           # [Interface] Các header file công khai
│   ├── cast.h              # Hàm chuyển đổi kiểu (Safe Casting)
│   ├── config.h.in         # Template versioning
│   ├── definitions.h       # Các định nghĩa kiểu dữ liệu cơ bản (ValueType)
│   ├── machine.h           # Class chính điều khiển máy ảo
│   ├── value.h             # Cấu trúc dữ liệu Value (NaN Boxing)
│   ├── compiler/           # Interface Compiler
│   │   ├── chunk.h         # Mảng bytecode và constant pool
│   │   ├── disassemble.h   # Công cụ dịch ngược bytecode -> text
│   │   └── op_codes.h      # Danh sách các lệnh (Instruction Set)
│   ├── core/               # Các Object Types
│   │   ├── array.h         # Mảng động (Meow Array)
│   │   ├── function.h      # Function, Closure, Proto, Upvalue
│   │   ├── hash_table.h    # Hash Map cho Object
│   │   ├── meow_object.h   # Base class cho mọi object GC quản lý
│   │   ├── module.h        # Hệ thống Module (Import/Export)
│   │   ├── objects.h       # Tổng hợp các loại object
│   │   ├── oop.h           # Class, Instance, BoundMethod
│   │   ├── shape.h         # Hidden Class (Shape) & Transitions
│   │   └── string.h        # String object & String Interning
│   ├── diagnostics/        # Hệ thống báo lỗi
│   └── memory/             # Quản lý bộ nhớ
│       ├── garbage_collector.h # Interface GC
│       ├── gc_visitor.h    # Interface cho việc duyệt object
│       └── memory_manager.h# Quản lý cấp phát và Heap
├── langs/                  # File ngôn ngữ (Localization)
├── libs/                   # Thư viện phụ trợ (Tự viết/Vendor)
├── scripts/                # Script tiện ích (Build, Run, Format)
├── src/                    # [Implementation] Mã nguồn thực thi
│   ├── pch.h               # Precompiled Header (Tăng tốc build)
│   ├── cli/                # Giao diện dòng lệnh (meow-vm.exe)
│   ├── compiler/           # Logic Compiler & Loader
│   │   ├── disassemble.cpp # Triển khai Disassembler
│   │   └── loader.cpp      # Đọc và link file .meowc
│   ├── core/               # Logic của các Object (Shape, Object tracing)
│   ├── debug/              # Utilities debug (print)
│   ├── jit/                # [JIT] Just-In-Time Compiler
│   │   ├── jit_compiler.h  # Interface JIT
│   │   └── x64/            # Backend cho kiến trúc x64
│   │       ├── compiler.cpp# Logic biên dịch Bytecode -> Machine Code
│   │       ├── emitter.cpp # Bộ phát mã máy (Assembly Emitter)
│   │       └── ...
│   ├── memory/             # Triển khai GC
│   │   ├── generational_gc.cpp # GC thế hệ (Young/Old Gen)
│   │   ├── mark_sweep_gc.cpp   # GC Mark-Sweep cổ điển (Fallback)
│   │   └── memory_manager.cpp  # Logic cấp phát
│   ├── module/             # Quản lý Module
│   │   ├── module_manager.cpp  # Cache và load module
│   │   └── module_utils.cpp    # Tiện ích đường dẫn (Path utils)
│   ├── runtime/            # Thành phần Runtime
│   │   ├── execution_context.h # Stack, Frame pointer
│   │   ├── operator_dispatcher.cpp # Xử lý toán tử (+, -, *, /) đa hình
│   │   └── call_frame.h    # Cấu trúc Stack Frame
│   ├── tools/masm/         # [Assembler Tool]
│   │   ├── src/            # Lexer, Assembler implementation
│   │   └── include/        # Assembler headers
│   └── vm/                 # Core VM Engine
│       ├── interpreter.cpp # Vòng lặp chính (Interpreter Loop)
│       ├── lifecycle.cpp   # Khởi tạo và hủy VM
│       ├── builtins.cpp    # Các hàm native (print, len, typeof...)
│       ├── handlers/       # [Handlers] Xử lý từng OpCode
│       │   ├── data_ops.h  # Load, Move
│       │   ├── math_ops.h  # Arithmetic, Bitwise
│       │   ├── flow_ops.h  # Jump, Call, Return
│       │   ├── memory_ops.h# Global, Upvalue, Closure
│       │   ├── module_ops.h# Import/Export
│       │   └── oop_ops.h   # Class, Property (Inline Cache)
│       └── stdlib/         # Thư viện chuẩn (C++)
│           ├── array_lib.cpp
│           ├── io_lib.cpp
│           ├── string_lib.cpp
│           └── ...
└── tests/                  # Test cases (.meow source & .meowb binary)

-----

## 3\. 🧠 Kiến trúc chi tiết (Detailed Architecture)

### 3.1. Memory Model (Mô hình bộ nhớ)

  * **NaN Boxing (64-bit):** Giá trị (`Value`) chỉ tốn 8 bytes.
      * `Double`: IEEE 754 chuẩn.
      * `Int/Bool/Null`: Dùng các bit NaN để đánh dấu (Tagging).
      * `Pointer`: Con trỏ 48-bit được nhúng vào payload của NaN.
  * **Heap & Allocator:**
      * Sử dụng **Arena Allocator** để cấp phát nhanh (bump pointer).
      * **String Interning:** Chuỗi giống nhau chỉ lưu 1 bản sao (tiết kiệm RAM, so sánh nhanh).

### 3.2. Garbage Collector (GC)

  * **Chiến lược:** **Generational GC** (Thế hệ).
      * **Young Gen:** Chứa object mới sinh. Thu gom thường xuyên (Minor GC).
      * **Old Gen:** Chứa object sống lâu. Thu gom ít hơn (Major GC).
      * **Remembered Set & Write Barrier:** Theo dõi các tham chiếu từ Old -\> Young để tránh quét toàn bộ Heap.

### 3.3. Execution Engine (Bộ máy thực thi)

  * **Stack:** VM dùng một mảng `Value` lớn làm Stack (`ExecutionContext::stack_`).
  * **Call Frame:** Mỗi hàm gọi tạo ra một `CallFrame` trỏ vào vùng Stack của nó.
  * **Interpreter Loop:**
      * **Argument Threading:** Truyền trực tiếp `regs`, `constants` vào hàm handler để tối ưu thanh ghi CPU.
      * **Computed Goto:** Dùng `dispatch_table` và `[[clang::musttail]]` để nhảy tới lệnh tiếp theo mà không cần `return` hay `break`.

### 3.4. JIT Compiler (x64)

  * **Type:** **Template JIT** (Copy đoạn mã máy có sẵn ghép lại).
  * **Register Mapping:** 5 thanh ghi ảo đầu tiên của VM (`R0`-`R4`) được map cứng vào thanh ghi vật lý (`RBX`, `R12`-`R15`) để tốc độ truy cập cực nhanh.
  * **Optimizations:**
      * **Instruction Fusion:** Gộp lệnh so sánh (`CMP`) và nhảy (`JCC`) thành một khối.
      * **Loop Peeling/Rotation:** Tối ưu hóa vòng lặp bằng cách xoay cấu trúc nhảy.
      * **Fast Path:** Sinh mã máy chuyên biệt cho trường hợp `Int32` (cộng trừ nhân chia nhanh hơn Double).

### 3.5. Object System (OOP)

  * **Hidden Classes (Shapes):** Thay vì dùng Hash Map cho mọi object, VM dùng `Shape` để map tên thuộc tính sang offset mảng.
  * **Inline Caching (IC):** Tại các điểm truy cập thuộc tính (`GET_PROP`), VM cache lại `Shape` và `Offset`.
      * *Lần đầu:* Tra cứu chậm -\> Lưu kết quả vào Cache tại chỗ (trong bytecode).
      * *Lần sau:* Kiểm tra nhanh `Shape` -\> Nếu khớp -\> Lấy giá trị ngay lập tức (O(1)).

### 3.6. Native Extension & FFI
MeowVM hỗ trợ mở rộng không giới hạn thông qua C++.
* **Dynamic Loading:** Tự động load `.dll` (Windows) hoặc `.so` (Linux/macOS) nếu tìm thấy file tương ứng trong đường dẫn import.
* **Symbol Resolution:** VM tìm kiếm entry point `CreateMeowModule` để khởi tạo module.
* **Bridge:** Hàm C++ (`native_t`) nhận trực tiếp mảng `Value* argv`, cho phép thao tác dữ liệu VM với chi phí chuyển đổi gần như bằng 0.

### 3.7. Exception Handling
Mô hình xử lý lỗi dựa trên Stack Unwinding:
* **Table-based Try-Catch:** Opcode `SETUP_TRY` ghi lại trạng thái Stack và Instruction Pointer (IP) vào bảng handler.
* **Unwinding:** Khi `THROW`, VM tìm handler gần nhất, đóng các `Open Upvalue` (để tránh memory leak), lùi Stack Frame và nhảy tới `catch_ip`.

-----

## 4\. 🔄 Luồng dữ liệu (Data Flow Pipeline)

1.  **Source Code (`.meow`)**
      * Code người dùng viết.
2.  **Assembler (`masm`)**
      * Lexer -\> Tokenizer -\> Parser -\> Code Gen.
      * Output: Binary file `.meowc` (chứa Header, Constant Pool, Bytecode).
3.  **VM Loader**
      * Đọc `.meowc`.
      * **Static Linking:** Vá các lệnh `GET_GLOBAL` để trỏ trực tiếp vào index bộ nhớ (bỏ qua bước tra cứu tên chuỗi lúc runtime).
4.  **Runtime Execution**
      * Khởi tạo `Machine`, `GC`, `Context`.
      * Load `native` modules (print, io...).
      * Chạy `Interpreter` hoặc `JIT` tùy cấu hình.

-----

## 5\. 🛠️ Quy tắc phát triển (Development Guidelines)

### Code Style

  * Sử dụng **Google C++ Style Guide**.
  * Indent: 4 spaces.
  * Column Limit: 200 ký tự (cho thoải mái).
  * Luôn dùng `clang-format` trước khi commit.

### Performance Rules

1.  **Hạn chế `std::function`:** Dùng function pointer hoặc template để tránh overhead.
2.  **Branch Prediction:** Dùng `[[likely]]` và `[[unlikely]]` cho các nhánh điều kiện quan trọng.
3.  **Memory:** Tránh cấp phát (`new`/`malloc`) trong vòng lặp chính của VM.
4.  **Inline:** Sử dụng `[[gnu::always_inline]]` cho các hàm handler nhỏ.

-----

*Tài liệu này được cập nhật tự động dựa trên source code phiên bản `0.1.0`.*