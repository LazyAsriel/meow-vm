#!/bin/bash

# 1. Kiểm tra tham số đầu vào
if [ -z "$1" ]; then
    echo "Sử dụng: $0 <đường_dẫn_thư_mục>"
    exit 1
fi

TARGET_DIR="$1"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Lỗi: Thư mục '$TARGET_DIR' không tồn tại."
    exit 1
fi

OUTPUT_FILE="$TARGET_DIR/merged_source_full.txt"

# Xóa file cũ nếu tồn tại
> "$OUTPUT_FILE"

echo "Đang quét thư mục (đệ quy): $TARGET_DIR"
echo "Các đuôi file hỗ trợ: .h, .hpp, .c, .cpp"

# 2. Lệnh find nâng cao
# -type f: Chỉ tìm file
# \( ... \): Nhóm các điều kiện tên file (OR logic)
# sort: Sắp xếp đường dẫn để file trong cùng thư mục nằm gần nhau
find "$TARGET_DIR" -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.c" -o -name "*.cpp" \) | sort | while read -r filepath; do
    
    # Bỏ qua file output nếu nó nằm trong danh sách (phòng hờ)
    if [ "$filepath" == "$OUTPUT_FILE" ]; then
        continue
    fi

    echo "Đang xử lý: $filepath"

    # 3. Ghi vào file output
    {
        echo "================================================================================"
        echo " FILE PATH: $filepath"
        echo "================================================================================"
        
        # cat -n: Hiển thị nội dung kèm số dòng
        cat -n "$filepath"
        
        echo -e "\n\n" 
    } >> "$OUTPUT_FILE"

done

echo "----------------------------------------------------"
echo "Hoàn tất! File được lưu tại: $OUTPUT_FILE"