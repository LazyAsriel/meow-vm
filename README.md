**Meow-VM**, một máy ảo register-based hiệu năng cao viết bằng **C++23**

> **Lưu ý:**
> * Để hiểu thiết kế hệ thống (Memory Model, GC Strategy, JIT), vui lòng đọc `docs/architecture.md`.
> * **Dành cho Debug:** Để phân tích implementation chi tiết và liền mạch, hãy tham khảo các file mã nguồn đã gộp (**Unified Source**) nằm trong thư mục `merged/`.
>   * *Chạy lệnh `./scripts/merge.sh` để sinh các file này nếu chưa có.*

---

## 🧭 Bản đồ định vị mã nguồn (Code Navigation Map)
Để hỗ trợ việc refactor hoặc fix bug, đây là vị trí các thành phần logic quan trọng:

### 1. Object Model & Memory (`include/meow/core/`)
Định nghĩa các cấu trúc dữ liệu cốt lõi.
👉 **Full Context:** Xem file `merged/include_meow_core.unified.cpp`

* **`value.h`**: Định nghĩa `Value` (NaN-boxing hoặc Union).
* **`shape.h`**: Hidden Classes & Transitions (quan trọng cho Property Access).
* **`hash_table.h`**: Open Addressing Hash Map (dùng cho Globals/Interning).
* **`array.h`**: Wrapper quanh `std::vector` với GC support.
* **`oop.h`**: Class, Instance, BoundMethod layouts.

### 2. VM Loop & Opcode Handlers (`src/vm/handlers/`)
Implementation của từng lệnh bytecode được chia nhỏ để dễ quản lý.
👉 **Full Context:** Xem file `merged/src_vm_handlers.unified.cpp` (hoặc `src_vm.unified.cpp` nếu gộp cả VM)

* **`flow_ops.h`**: `CALL`, `RETURN`, `JUMP`, `TAIL_CALL`. Logic tạo Stack Frame nằm ở đây.
* **`data_ops.h`**: `LOAD_CONST`, `MOVE`, `NEW_ARRAY`, `NEW_HASH`.
* **`oop_ops.h`**: `GET_PROP`, `SET_PROP` (Chứa logic **Inline Caching**).
* **`math_ops.h`**: Các toán tử số học (`ADD`, `SUB`...) có Fast Path cho `Int`.
* **`exception_ops.h`**: `THROW`, `SETUP_TRY` (Table-based EH).

---

*File này được thiết kế để cung cấp context thực thi nhanh nhất cho Developer. Chi tiết lý thuyết xem tại [docs/architecture.md](docs/architecture.md)*