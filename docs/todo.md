**1. Loại bỏ Dynamic Global Lookup (Dễ)**

  * **Hiện tại:** `GET_GLOBAL` nhận một `name_idx` (chuỗi), VM phải hash chuỗi đó và tìm trong hash map global.
  * **Cải tiến:** Compiler phải giải quyết tất cả biến Global thành một mảng chỉ số (index).
      * VM sẽ có `std::vector<Value> globals_`.
      * OpCode đổi thành: `GET_GLOBAL <index>`. Truy cập mảng `globals_[index]` cực nhanh (O(1)) so với hash map.
      * **Compiler:** Phải xây dựng bảng symbol cho global scope.

**2. "Specialized Bytecode" thay vì Polymorphic Opcodes (Trung bình)**

  * **Hiện tại:** `OpCode::ADD` trong `MeowVM::run` gọi `op_dispatcher_->find(...)`. Nó phải kiểm tra kiểu của toán hạng (Int? Float? String?) *mỗi lần chạy*.
  * **Cải tiến:** Nếu Compiler biết kiểu dữ liệu (thông qua Type Checking hoặc Type Inference), hãy phát sinh opcode cụ thể:
      * `ADD_INT`: Cộng 2 thanh ghi int (không check kiểu, hoặc chỉ assert).
      * `ADD_FLOAT`: Cộng 2 thanh ghi float.
      * `CONCAT_STRING`: Nối chuỗi.
  * **VM:** Sẽ không cần `OperatorDispatcher` phức tạp nữa, chỉ cần thực thi trực tiếp phép `+`. Nếu sai kiểu -\> Crash hoặc Undefined Behavior (như C++), ép người viết code MeowScript phải đúng kiểu.

**3. Inline Caching cho Property Access (Khó - High Performance)**

  * **Hiện tại:** `GET_PROP` tìm chuỗi tên thuộc tính trong Hash Map của Instance/Class.
  * **Cải tiến:** Vì VM "không được thông minh", ta dùng kỹ thuật **Inline Cache**.
      * Opcode `GET_PROP` sẽ có thêm 1 slot trống trong bytecode để "nhớ" vị trí (offset) của thuộc tính lần truy cập trước.
      * Lần chạy đầu: Tìm Hash Map, lưu offset vào slot đó.
      * Lần chạy sau: Kiểm tra nhanh (nếu class không đổi), dùng offset đó truy cập thẳng vào mảng field, bỏ qua Hash Map.