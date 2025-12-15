## 1. Module "native" (Các hàm toàn cục)

Đây là các hàm cơ bản nhất, được nạp tự động vào môi trường global khi VM khởi động. Cậu có thể gọi chúng ở bất kỳ đâu mà không cần `import`.

### print(...)
* **Cách dùng:** `print(value1, value2, ...)`
* **Mục đích:** In một hoặc nhiều giá trị ra màn hình console. Các giá trị sẽ được ngăn cách bởi một khoảng trắng và tự động xuống dòng ở cuối.

### typeof(value)
* **Cách dùng:** `local_type = typeof(some_value)`
* **Mục đích:** Trả về một chuỗi (string) mô tả kiểu dữ liệu của giá trị đầu vào (ví dụ: "null", "int", "real", "bool", "string", "array", "object", "function", "class", "instance").

### len(value)
* **Cách dùng:** `local_length = len(some_value)`
* **Mục đích:** Lấy "độ dài" của một đối tượng.
    * Nếu là **String**: Trả về số lượng ký tự.
    * Nếu là **Array**: Trả về số lượng phần tử.
    * Nếu là **Object**: Trả về số lượng cặp key-value.
    * Các kiểu khác: Trả về -1.

### int(value)
* **Cách dùng:** `local_int = int(some_value)`
* **Mục đích:** Chuyển đổi một giá trị (ví dụ: Real, String, Bool) sang kiểu số nguyên (Int).

### real(value)
* **Cách dùng:** `local_real = real(some_value)`
* **Mục đích:** Chuyển đổi một giá trị (ví dụ: Int, String, Bool) sang kiểu số thực (Real).

### bool(value)
* **Cách dùng:** `local_bool = bool(some_value)`
* **Mục đích:** Chuyển đổi một giá trị sang kiểu logic (Bool) dựa trên quy tắc "truthy" và "falsy" (ví dụ: `0`, `null`, `""` là `false`; các giá trị khác là `true`).

### str(value)
* **Cách dùng:** `local_string = str(some_value)`
* **Mục đích:** Chuyển đổi một giá trị sang biểu diễn chuỗi (String).

### assert(condition, message)
* **Cách dùng:** `assert(x > 10, "Giá trị x phải lớn hơn 10")`
* **Mục đích:** Kiểm tra một điều kiện. Nếu `condition` là `false` (falsy), chương trình sẽ dừng lại và báo lỗi với `message` (nếu có). Nếu `condition` là `true`, không có gì xảy ra.

### ord(character)
* **Cách dùng:** `ascii_code = ord("A")`
* **Mục đích:** Yêu cầu đầu vào là một chuỗi có *đúng 1 ký tự*. Trả về mã ASCII (Int) của ký tự đó.

### char(code)
* **Cách dùng:** `character = char(65)`
* **Mục đích:** Yêu cầu đầu vào là một số nguyên (Int) trong khoảng [0, 255]. Trả về chuỗi (String) có 1 ký tự tương ứng với mã ASCII đó.

### range(...)
* **Cách dùng:**
    * `range(stop)`: Tạo mảng từ 0 đến (stop - 1).
    * `range(start, stop)`: Tạo mảng từ start đến (stop - 1).
    * `range(start, stop, step)`: Tạo mảng từ start đến (stop - 1), với mỗi bước nhảy là `step` (step có thể âm).
* **Mục đích:** Tạo ra một mảng (Array) chứa một dãy số nguyên.

---

## 2. Module "io"

Các hàm liên quan đến việc đọc/ghi file và tương tác với hệ thống file. Cậu cần `import "io"` để sử dụng.

### io.input(prompt)
* **Cách dùng:** `user_name = io.input("Nhập tên của bạn: ")`
* **Mục đích:** Hiển thị một `prompt` (chuỗi, tùy chọn) ra console và chờ người dùng nhập một dòng. Trả về chuỗi (String) mà người dùng đã nhập.

### io.read(path)
* **Cách dùng:** `content = io.read("data.txt")`
* **Mục đích:** Đọc toàn bộ nội dung của file tại đường dẫn `path` và trả về dưới dạng một chuỗi (String). Trả về `null` nếu đọc thất bại.

### io.write(path, data, append)
* **Cách dùng:**
    * `io.write("log.txt", "Hello", true)` (Ghi nối đuôi)
    * `io.write("config.txt", "value=10", false)` (Ghi đè)
* **Mục đích:** Ghi `data` (String, Int, Real, Bool) vào file tại đường dẫn `path`.
    * `append` (Bool, tùy chọn, mặc định là `false`): Nếu là `true`, dữ liệu sẽ được ghi nối vào cuối file. Nếu là `false`, file sẽ bị ghi đè.
* **Trả về:** `true` nếu ghi thành công, `false` nếu thất bại.

### io.fileExists(path)
* **Cách dùng:** `if (io.fileExists("config.json")) { ... }`
* **Mục đích:** Kiểm tra xem một file (hoặc thư mục) có tồn tại tại đường dẫn `path` hay không. Trả về `true` hoặc `false`.

### io.isDirectory(path)
* **Cách dùng:** `if (io.isDirectory("my_folder")) { ... }`
* **Mục đích:** Kiểm tra xem đường dẫn `path` có phải là một thư mục hay không. Trả về `true` hoặc `false`.

### io.listDir(path)
* **Cách dùng:** `files = io.listDir(".")`
* **Mục đích:** Trả về một mảng (Array) chứa tên (String) của tất cả các file và thư mục con bên trong thư mục `path`. Trả về `null` nếu `path` không hợp lệ.

### io.createDir(path)
* **Cách dùng:** `io.createDir("data/logs")`
* **Mục đích:** Tạo một thư mục tại `path`. Hàm này sẽ tự động tạo luôn các thư mục cha nếu chúng chưa tồn tại. Trả về `true` nếu thành công, `false` nếu thất bại.

### io.deleteFile(path)
* **Cách dùng:** `io.deleteFile("temp.log")`
* **Mục đích:** Xóa file (hoặc thư mục rỗng) tại đường dẫn `path`. Trả về `true` nếu thành công, `false` nếu thất bại.

### io.getFileTimestamp(path)
* **Cách dùng:** `last_mod = io.getFileTimestamp("main.meow")`
* **Mục đích:** Trả về một số nguyên (Int) là mốc thời gian (timestamp) lần cuối file bị sửa đổi. Trả về -1 nếu lỗi.

### io.getFileSize(path)
* **Cách dùng:** `size = io.getFileSize("image.png")`
* **Mục đích:** Trả về kích thước của file (tính bằng byte) dưới dạng số nguyên (Int). Trả về -1 nếu lỗi.

### io.renameFile(source, destination)
* **Cách dùng:** `io.renameFile("old_name.txt", "new_name.txt")`
* **Mục đích:** Đổi tên hoặc di chuyển file từ `source` sang `destination`. Trả về `true` nếu thành công, `false` nếu thất bại.

### io.copyFile(source, destination)
* **Cách dùng:** `io.copyFile("data.db", "data_backup.db")`
* **Mục đích:** Sao chép file từ `source` sang `destination` (sẽ ghi đè nếu `destination` đã tồn tại). Trả về `true` nếu thành công, `false` nếu thất bại.

### io.getFileName(path)
* **Cách dùng:** `name = io.getFileName("src/core/main.meow")` (Kết quả: "main.meow")
* **Mục đích:** Trả về phần tên file (kèm đuôi) từ một đường dẫn.

### io.getFileStem(path)
* **Cách dùng:** `name = io.getFileStem("src/core/main.meow")` (Kết quả: "main")
* **Mục đích:** Trả về phần tên file (không kèm đuôi) từ một đường dẫn.

### io.getFileExtension(path)
* **Cách dùng:** `ext = io.getFileExtension("src/core/main.meow")` (Kết quả: "meow")
* **Mục đích:** Trả về phần đuôi mở rộng (không bao gồm dấu '.') từ một đường dẫn.

### io.getAbsolutePath(path)
* **Cách dùng:** `full_path = io.getAbsolutePath("../data")`
* **Mục đích:** Chuyển đổi một đường dẫn tương đối (như `.` hoặc `..`) thành đường dẫn tuyệt đối trên hệ thống.

---

## 3. Module "system"

Các hàm liên quan đến hệ thống và tiến trình (process) đang chạy. Cậu cần `import "system"` để sử dụng.

### system.argv()
* **Cách dùng:** `arguments = system.argv()`
* **Mục đích:** Trả về một mảng (Array) chứa các tham số dòng lệnh (command-line arguments) đã được truyền cho máy ảo MeowVM khi nó được gọi.

### system.exit(code)
* **Cách dùng:** `system.exit(1)`
* **Mục đích:** Buộc chương trình MeowScript kết thúc ngay lập tức. `code` (Int, tùy chọn, mặc định là 0) là mã trạng thái trả về cho hệ điều hành.

### system.exec(command)
* **Cách dùng:** `exit_code = system.exec("ls -l")` (trên Linux/macOS)
* **Mục đích:** Thực thi một lệnh của hệ điều hành (giống như cậu gõ trong Terminal/CMD). Trả về mã thoát (Int) của lệnh đó sau khi nó chạy xong.

---

## 4. Module "json"

Cung cấp các hàm để làm việc với định dạng dữ liệu JSON. Cậu cần `import "json"` để sử dụng.

### json.parse(jsonString)
* **Cách dùng:** `data = json.parse('{ "name": "Meo", "age": 9 }')`
* **Mục đích:** Phân tích một chuỗi (String) có định dạng JSON và chuyển đổi nó thành các cấu trúc dữ liệu của MeowScript (Object, Array, String, Int, Real, Bool, Null). Trả về `null` nếu chuỗi JSON bị lỗi cú pháp.

### json.stringify(value, tabSize)
* **Cách dùng:** `json_string = json.stringify(my_object, 2)`
* **Mục đích:** Chuyển đổi một giá trị (thường là Object hoặc Array) của MeowScript thành một chuỗi (String) có định dạng JSON.
    * `tabSize` (Int, tùy chọn, mặc định là 2): Số lượng dấu cách dùng để thụt lề, giúp chuỗi JSON dễ đọc hơn.

---

## 5. Module "array" (Đồng thời là phương thức của Array)

Các hàm này được đăng ký cho module "array" và cũng được tự động gắn làm phương thức cho tất cả các đối tượng Array.

* **Cách dùng (dạng module):** `import "array"; array.push(my_arr, 1, 2)`
* **Cách dùng (dạng phương thức):** `my_arr.push(1, 2)` (Khuyến khích dùng cách này)

### arr.push(...)
* **Cách dùng:** `arr.push(value1, value2, ...)`
* **Mục đích:** Thêm một hoặc nhiều giá trị vào cuối mảng. Trả về độ dài mới của mảng.

### arr.pop()
* **Cách dùng:** `last_item = arr.pop()`
* **Mục đích:** Xóa và trả về phần tử cuối cùng của mảng. Trả về `null` nếu mảng rỗng.

### arr.slice(start, end)
* **Cách dùng:** `sub_array = arr.slice(1, 3)`
* **Mục đích:** Trích xuất một mảng con (mới) từ chỉ số `start` (bao gồm) đến chỉ số `end` (không bao gồm).
    * `start` (Int, tùy chọn, mặc định là 0): Hỗ trợ chỉ số âm (đếm từ cuối).
    * `end` (Int, tùy chọn, mặc định là hết mảng): Hỗ trợ chỉ số âm.

### arr.map(callback)
* **Cách dùng:** `doubled = [1, 2, 3].map(func(x) { return x * 2; })`
* **Mục đích:** Tạo ra một mảng (Array) mới. Hàm `callback` (Function) sẽ được gọi cho từng phần tử, và giá trị mà `callback` trả về sẽ được đưa vào mảng mới.

### arr.filter(callback)
* **Cách dùng:** `evens = [1, 2, 3, 4].filter(func(x) { return x % 2 == 0; })`
* **Mục đích:** Tạo ra một mảng (Array) mới chỉ chứa các phần tử mà khi gọi `callback` với phần tử đó, nó trả về giá trị `true` (truthy).

### arr.reduce(callback, initialValue)
* **Cách dùng:** `sum = [1, 2, 3].reduce(func(acc, val) { return acc + val; }, 0)`
* **Mục đích:** "Giảm" mảng thành một giá trị duy nhất. Hàm `callback` nhận 2 tham số (biến tích lũy `acc`, giá trị hiện tại `val`) và trả về giá trị tích lũy tiếp theo. `initialValue` là giá trị khởi tạo cho `acc`.

### arr.forEach(callback)
* **Cách dùng:** `[1, 2].forEach(func(val, index) { print(index, val); })`
* **Mục đích:** Gọi hàm `callback` (Function) cho từng phần tử trong mảng (nhận 2 tham số: `value`, `index`). Không trả về gì cả (`null`).

### arr.find(callback)
* **Cách dùng:** `found = [1, 2, 3].find(func(x) { return x > 1; })` (Kết quả: 2)
* **Mục đích:** Trả về *giá trị* của phần tử đầu tiên trong mảng mà làm cho `callback` trả về `true` (truthy). Trả về `null` nếu không tìm thấy.

### arr.findIndex(callback)
* **Cách dùng:** `found_idx = [1, 2, 3].findIndex(func(x) { return x > 1; })` (Kết quả: 1)
* **Mục đích:** Trả về *chỉ số* (Int) của phần tử đầu tiên trong mảng mà làm cho `callback` trả về `true` (truthy). Trả về -1 nếu không tìm thấy.

### arr.reverse()
* **Cách dùng:** `arr.reverse()`
* **Mục đích:** Đảo ngược thứ tự các phần tử trong mảng (thay đổi trực tiếp mảng gốc). Trả về chính mảng đó.

### arr.sort(compare_func)
* **Cách dùng:**
    * `arr.sort()` (Sắp xếp số hoặc chuỗi theo thứ tự tăng dần)
    * `arr.sort(func(a, b) { return b - a; })` (Sắp xếp số giảm dần)
* **Mục đích:** Sắp xếp các phần tử của mảng (thay đổi trực tiếp mảng gốc).
    * Nếu không có `compare_func`, nó tự động so sánh số hoặc chuỗi.
    * `compare_func` (Function, tùy chọn): Nhận 2 giá trị `a` và `b`. Nếu trả về số âm, `a` đứng trước `b`. Nếu trả về số dương, `b` đứng trước `a`.

### arr.reserve(capacity)
* **Cách dùng:** `arr.reserve(100)`
* **Mục đích:** Yêu cầu mảng cấp phát trước bộ nhớ để chứa `capacity` (Int) phần tử. Giúp tăng tốc độ nếu cậu biết trước cần `push` nhiều phần tử.

### arr.resize(newSize, fillValue)
* **Cách dùng:** `arr.resize(10, null)`
* **Mục đích:** Thay đổi kích thước mảng thành `newSize` (Int).
    * Nếu `newSize` lớn hơn kích thước hiện tại, các ô mới sẽ được lấp đầy bằng `fillValue` (Tùy chọn, mặc định là `null`).
    * Nếu `newSize` nhỏ hơn, các phần tử cuối sẽ bị cắt bỏ.

### arr.size() (hoặc getter `arr.length`)
* **Cách dùng:** `count = arr.size()` hoặc `count = arr.length`
* **Mục đích:** Trả về số lượng phần tử (Int) trong mảng.

---

## 6. Module "object" (Đồng thời là phương thức của Object)

Tương tự như Array, các hàm này cũng được gắn làm phương thức cho tất cả các đối tượng Object.

### obj.keys()
* **Cách dùng:** `keys_array = my_obj.keys()`
* **Mục đích:** Trả về một mảng (Array) chứa tất cả các key (String) của object.

### obj.values()
* **Cách dùng:** `values_array = my_obj.values()`
* **Mục đích:** Trả về một mảng (Array) chứa tất cả các value của object.

### obj.entries()
* **Cách dùng:** `entries_array = my_obj.entries()`
* **Mục đích:** Trả về một mảng (Array), trong đó mỗi phần tử là một mảng con 2 phần tử `[key, value]`.

### obj.has(key)
* **Cách dùng:** `if (my_obj.has("config")) { ... }`
* **Mục đích:** Kiểm tra xem object có chứa `key` (String) hay không. Trả về `true` hoặc `false`.

### obj.merge(...)
* **Cách dùng:** `new_obj = {}.merge(obj1, obj2, ...)`
* **Mục đích:** Tạo ra một object (Object) mới bằng cách gộp tất cả các key-value từ các object được truyền vào. Nếu có key trùng lặp, giá trị của object bên phải (cuối cùng) sẽ thắng.
    * *Lưu ý:* Hàm này (theo file `object.cpp`) dường như được thiết kế để gọi từ module `object.merge(obj1, obj2)` thay vì `obj1.merge(obj2)`, mặc dù nó được đăng ký làm phương thức.

---

## 7. Module "string" (Đồng thời là phương thức của String)

Các hàm này cũng được gắn làm phương thức cho tất cả các giá trị String.

### str.split(delimiter)
* **Cách dùng:** `parts = "a-b-c".split("-")` (Kết quả: `["a", "b", "c"]`)
* **Mục đích:** Tách chuỗi thành một mảng (Array) dựa trên `delimiter` (String, tùy chọn, mặc định là " "). Nếu `delimiter` là chuỗi rỗng (`""`), nó sẽ tách thành mảng các ký tự.

### str.join(array)
* **Cách dùng:** `text = "-".join(["a", "b", "c"])` (Kết quả: "a-b-c")
* **Mục đích:** *Lưu ý: Cách dùng này ngược với các ngôn ngữ khác.* Chuỗi `str` (ở đây là "-") được dùng làm ký tự nối các phần tử trong `array` lại thành một chuỗi (String) duy nhất.

### str.upper()
* **Cách dùng:** `upper_text = "Hello".upper()`
* **Mục đích:** Trả về chuỗi mới đã được chuyển thành chữ hoa.

### str.lower()
* **Cách dùng:** `lower_text = "Hello".lower()`
* **Mục đích:** Trả về chuỗi mới đã được chuyển thành chữ thường.

### str.trim()
* **Cách dùng:** `clean_text = "  hello  ".trim()`
* **Mục đích:** Trả về chuỗi mới đã bị cắt bỏ các khoảng trắng (space, tab, ...) ở đầu và cuối.

### str.startsWith(prefix)
* **Cách dùng:** `is_http = "http://".startsWith("http")` (Kết quả: `true`)
* **Mục đích:** Kiểm tra xem chuỗi có bắt đầu bằng `prefix` (String) hay không.

### str.endsWith(suffix)
* **Cách dùng:** `is_meow = "main.meow".endsWith(".meow")` (Kết quả: `true`)
* **Mục đích:** Kiểm tra xem chuỗi có kết thúc bằng `suffix` (String) hay không.

### str.replace(from, to)
* **Cách dùng:** `new_text = "a-b-c".replace("-", "/")` (Kết quả: "a/b-c")
* **Mục đích:** Trả về chuỗi mới trong đó *lần xuất hiện đầu tiên* của `from` (String) được thay thế bằng `to` (String).

### str.contains(substring)
* **Cách dùng:** `has_err = "Error: 404".contains("Error")`
* **Mục đích:** Kiểm tra xem chuỗi có chứa `substring` (String) hay không.

### str.indexOf(substring, start)
* **Cách dùng:** `pos = "a-b-a".indexOf("a", 1)` (Kết quả: 4)
* **Mục đích:** Tìm chỉ số (Int) của `substring` (String) trong chuỗi.
    * `start` (Int, tùy chọn, mặc định là 0): Bắt đầu tìm từ chỉ số này.
* **Trả về:** -1 nếu không tìm thấy.

### str.lastIndexOf(substring)
* **Cách dùng:** `pos = "a-b-a".lastIndexOf("a")` (Kết quả: 4)
* **Mục đích:** Tìm chỉ số (Int) của `substring` (String) trong chuỗi, nhưng tìm từ *cuối chuỗi* ngược về đầu. Trả về -1 nếu không tìm thấy.

### str.substring(start, length)
* **Cách dùng:** `sub = "Hello".substring(1, 3)` (Kết quả: "ell")
* **Mục đích:** Trả về chuỗi con bắt đầu từ `start` (Int) và có độ dài `length` (Int, tùy chọn, mặc định là đến hết chuỗi).

### str.slice(start, end)
* **Cách dùng:** `sub = "Hello".slice(1, 3)` (Kết quả: "el")
* **Mục đích:** Trả về chuỗi con từ chỉ số `start` (Int, bao gồm) đến `end` (Int, không bao gồm). Hỗ trợ chỉ số âm (đếm từ cuối).

### str.repeat(count)
* **Cách dùng:** `stars = "*".repeat(5)` (Kết quả: "*****")
* **Mục đích:** Trả về chuỗi mới được lặp lại `count` (Int) lần.

### str.padLeft(length, char)
* **Cách dùng:** `code = "5".padLeft(3, "0")` (Kết quả: "005")
* **Mục đích:** Đệm vào bên trái chuỗi bằng `char` (String, tùy chọn, mặc định là " ") cho đến khi chuỗi đạt độ dài `length` (Int).

### str.padRight(length, char)
* **Cách dùng:** `name = "Meo".padRight(5, ".")` (Kết quả: "Meo..")
* **Mục đích:** Đệm vào bên phải chuỗi bằng `char` (String, tùy chọn, mặc định là " ") cho đến khi chuỗi đạt độ dài `length` (Int).

### str.equalsIgnoreCase(otherString)
* **Cách dùng:** `"Meo".equalsIgnoreCase("meo")` (Kết quả: `true`)
* **Mục đích:** So sánh 2 chuỗi không phân biệt chữ hoa/thường.

### str.charAt(index)
* **Cách dùng:** `char = "Meo".charAt(1)` (Kết quả: "e")
* **Mục đích:** Trả về chuỗi (String) chứa 1 ký tự tại `index` (Int). Trả về chuỗi rỗng nếu `index` ngoài phạm vi.

### str.charCodeAt(index)
* **Cách dùng:** `code = "Meo".charCodeAt(1)` (Kết quả: 101)
* **Mục đích:** Trả về mã ASCII (Int) của ký tự tại `index` (Int). Trả về -1 nếu `index` ngoài phạm vi.

### str.size() (hoặc getter `str.length`)
* **Cách dùng:** `count = str.size()` hoặc `count = str.length`
* **Mục đích:** Trả về số lượng ký tự (Int) trong chuỗi.

Ngoài import "io" thì ta vẫn có thể import { specifier } from "io", import * as namespace from "io" hoặc import "io" để import all và tràn vào môi trường toàn cục