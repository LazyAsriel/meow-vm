**Meow-VM**, một máy ảo register-based hiệu năng cao viết bằng **C++23**

> **Lưu ý:**
> * Để hiểu thiết kế hệ thống (Memory Model, GC Strategy, JIT), vui lòng đọc `docs/architecture.md`.
> * Để phân tích implementation chi tiết, hãy tham khảo các file `merged_source_full.txt` (đã gộp source) trong `include/meow/core/` và `src/vm/handlers/`.
> 
> 

---

##🧭 Bản đồ định vị mã nguồn (Code Navigation Map) Để hỗ trợ việc refactor hoặc fix bug, đây là vị trí các thành phần logic quan trọng:

###1. Object Model & Memory (`include/meow/core/`)Định nghĩa các cấu trúc dữ liệu cốt lõi (xem `src/vm/handlers/merged_source_full.txt` trong thư mục này để có cái nhìn toàn cảnh):

* **`value.h`**: Định nghĩa `Value`.
* **`shape.h`**: Hidden Classes & Transitions (quan trọng cho Property Access).
* **`hash_table.h`**: Open Addressing Hash Map (dùng cho Globals/Interning).
* **`array.h`**: Wrapper quanh `std::vector` với GC support.
* **`oop.h`**: Class, Instance, BoundMethod layouts.

###2. VM Loop & Opcode Handlers (`src/vm/handlers/`)Implementation của từng lệnh bytecode được chia nhỏ để dễ quản lý (xem `src/vm/handlers/merged_source_full.txt`):

* **`flow_ops.h`**: `CALL`, `RETURN`, `JUMP`, `TAIL_CALL`. Logic tạo Stack Frame nằm ở đây.
* **`data_ops.h`**: `LOAD_CONST`, `MOVE`, `NEW_ARRAY`, `NEW_HASH`.
* **`oop_ops.h`**: `GET_PROP`, `SET_PROP` (Chứa logic **Inline Caching**).
* **`math_ops.h`**: Các toán tử số học (`ADD`, `SUB`...) có Fast Path cho `Int`.
* **`exception_ops.h`**: `THROW`, `SETUP_TRY` (Table-based EH).

---

*File này được thiết kế để cung cấp context thực thi nhanh nhất cho Developer và AI. Chi tiết lý thuyết xem tại [docs/architecture.md](docs/architecture.md)*