# Danh sách opcode (theo *meow_vm.cpp*). 

Mình liệt kê **mỗi opcode — làm gì** và **tham số** nó nhận (kiểu/ý nghĩa). Mình *không* giải thích cách hoạt động bên trong — chỉ chức năng và định dạng bytecode thôi như bạn yêu cầu. 😼✨

---

## Lưu ý chung

* Các register / chỉ số thường là `uint16_t` (u16).
* Hầu hết các opcode bắt đầu bằng 1 byte mã lệnh, sau đó là các trường dữ liệu (u16, i64, f64, ...).
* Giá trị `0xFFFF` (u16) được dùng như sentinel (ví dụ: return-void / no-ret).
* Nhiều opcode truy xuất hằng số bằng **constant index** (`u16`) — đó là chỉ số vào bảng hằng của chunk.
* Tài liệu tham khảo mã nguồn: meow_vm.cpp. 

---

## LOAD / MOVE

* **LOAD_CONST** — Load một hằng vào register.

  * Tham số: `dst: u16` (register đích), `const_idx: u16` (index vào bảng constant).
* **LOAD_NULL** — Gán `null` vào register.

  * Tham số: `dst: u16`.
* **LOAD_TRUE** — Gán `true` vào register.

  * Tham số: `dst: u16`.
* **LOAD_FALSE** — Gán `false` vào register.

  * Tham số: `dst: u16`.
* **MOVE** — Sao chép giá trị từ register này sang register khác.

  * Tham số: `dst: u16`, `src: u16`.
* **LOAD_INT** — Load số nguyên 64-bit vào register.

  * Tham số: `dst: u16`, `value: i64 (8 bytes)`.
* **LOAD_FLOAT** — Load số thực (double) vào register.

  * Tham số: `dst: u16`, `value: f64 (8 bytes)`.

---

## Toán tử nhị phân (cùng dạng tham số)

Các opcode dưới đây đều thực hiện toán tử nhị phân và có cùng định dạng tham số:
`dst: u16`, `r1: u16`, `r2: u16` (dst = r1 OP r2).

* **ADD**, **SUB**, **MUL**, **DIV**, **MOD**, **POW**
* **EQ**, **NEQ**, **GT**, **GE**, **LT**, **LE**
* **BIT_AND**, **BIT_OR**, **BIT_XOR**, **LSHIFT**, **RSHIFT**

---

## Toán tử đơn (unary)

Định dạng: `dst: u16`, `src: u16`.

* **NEG** — phủ định số.
* **NOT** — logic NOT.
* **BIT_NOT** — bitwise NOT.

---

## GLOBALS

* **GET_GLOBAL** — Lấy biến global của module hiện tại vào register.

  * Tham số: `dst: u16`, `name_idx: u16` (index constant chứa tên string).
* **SET_GLOBAL** — Đặt giá trị cho global trong module hiện tại.

  * Tham số: `name_idx: u16`, `src: u16` (register chứa giá trị).

---

## UPVALUES / CLOSURE

* **GET_UPVALUE** — Lấy giá trị upvalue (của function) vào register.

  * Tham số: `dst: u16`, `uv_idx: u16` (chỉ số upvalue trong function).
* **SET_UPVALUE** — Gán giá trị cho upvalue.

  * Tham số: `uv_idx: u16`, `src: u16`.
* **CLOSURE** — Tạo closure từ một proto constant và đặt vào register. (Phiên bản bytecode ghi chỉ số proto)

  * Tham số: `dst: u16`, `proto_idx: u16` (index constant chứa proto).
* **CLOSE_UPVALUES** — Đóng upvalues từ một chỉ số register trở lên.

  * Tham số: `last_reg: u16`.

---

## NHẢY (JUMP)

* **JUMP** — Nhảy tới địa chỉ (absolute offset trong chunk).

  * Tham số: `target: u16` (offset mã lệnh trong chunk).
* **JUMP_IF_FALSE** — Nếu register là falsy thì nhảy.

  * Tham số: `reg: u16`, `target: u16`.
* **JUMP_IF_TRUE** — Nếu register là truthy thì nhảy.

  * Tham số: `reg: u16`, `target: u16`.

---

## CALL / RETURN

* **CALL** — Gọi hàm có trả về.

  * Tham số: `dst: u16` (register đích cho return hoặc `0xFFFF` nếu không muốn giá trị), `fn_reg: u16` (register chứa callee), `arg_start: u16` (index register bắt đầu args), `argc: u16` (số arg).
  * Ghi chú: nếu `dst == 0xFFFF` thì ret không được lưu (treat as void/ignored).
* **CALL_VOID** — Gọi hàm không lấy giá trị trả về.

  * Tham số: `fn_reg: u16`, `arg_start: u16`, `argc: u16`.
* **RETURN** — Trả về từ hàm.

  * Tham số: `ret_reg_idx: u16` (`0xFFFF` nghĩa là trả `null`).

---

## CẤU TRÚC DỮ LIỆU

* **NEW_ARRAY** — Tạo mảng từ một dãy register.

  * Tham số: `dst: u16`, `start_idx: u16` (register bắt đầu), `count: u16` (số phần tử).
* **NEW_HASH** — Tạo hash table từ các cặp key/value trong register (key phải là string).

  * Tham số: `dst: u16`, `start_idx: u16`, `count: u16` (số cặp; cặp lưu liên tiếp: key,val).
* **GET_INDEX** — Lấy `src[key]` vào `dst`. Hỗ trợ array, hash, string.

  * Tham số: `dst: u16`, `src_reg: u16`, `key_reg: u16`.
* **SET_INDEX** — Gán `src[key] = val`. Hỗ trợ array, hash.

  * Tham số: `src_reg: u16`, `key_reg: u16`, `val_reg: u16`.
* **GET_KEYS** — Trả về mảng các key/index của object (hash/array/string).

  * Tham số: `dst: u16`, `src_reg: u16`.
* **GET_VALUES** — Trả về mảng các giá trị của object (hash/array/string).

  * Tham số: `dst: u16`, `src_reg: u16`.

---

## OOP (LỚP / INSTANCE / PROP)

* **NEW_CLASS** — Tạo class mới (với tên lấy từ constant).

  * Tham số: `dst: u16`, `name_idx: u16`.
* **NEW_INSTANCE** — Tạo instance từ class ở một register.

  * Tham số: `dst: u16`, `class_reg: u16`.
* **GET_PROP** — Lấy thuộc tính/method của object/module/instance.

  * Tham số: `dst: u16`, `obj_reg: u16`, `name_idx: u16`.
* **SET_PROP** — Đặt property trên instance.

  * Tham số: `obj_reg: u16`, `name_idx: u16`, `val_reg: u16`.
* **SET_METHOD** — Gán method vào class.

  * Tham số: `call_reg: u16` (register chứa class), `name_idx: u16`, `method_reg: u16` (function).
* **INHERIT** — Thiết lập superclass cho một class.

  * Tham số: `sub_reg: u16`, `super_reg: u16`.
* **GET_SUPER** — Lấy method của superclass và bind với `this` vào `dst`. (Giả định receiver ở R0).

  * Tham số: `dst: u16`, `name_idx: u16`.

---

## TRY / THROW (Exception)

* **THROW** — Ném ngoại lệ (nội dung: register).

  * Tham số: `reg: u16`.
* **SETUP_TRY** — Đăng ký handler catch: lưu địa chỉ catch (u16 target/offset) và register để lưu biến lỗi.

  * Tham số: `target: u16` (offset mã lệnh trong chunk), `error_reg: u16` (register để VM lưu lại object lỗi khi bắt được, 0xFFFF nếu không cần biến lỗi).
* **POP_TRY** — Bỏ handler try hiện tại.

  * Tham số: *không có*.

---

## MODULE / IMPORT / EXPORT

* **IMPORT_MODULE** — Import (load) module, và (nếu cần) execute `@main` của module. Trả module object vào `dst`.

  * Tham số: `dst: u16`, `path_idx: u16` (constant chứa đường dẫn string).
* **EXPORT** — Đánh dấu giá trị ở register là export của module hiện tại.

  * Tham số: `name_idx: u16`, `src_reg: u16`.
* **GET_EXPORT** — Lấy export từ một module object.

  * Tham số: `dst: u16`, `mod_reg: u16`, `name_idx: u16`.
* **IMPORT_ALL** — Nhập tất cả export từ module (module object phải có).

  * Tham số: `src_idx: u16` (register chứa module).

---

## KHÁC

* **HALT** — Dừng VM / kết thúc thực thi.

  * Tham số: *không có*.