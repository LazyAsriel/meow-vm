# Tạo thư mục dist
mkdir -p dist

# Duyệt qua các file nguồn
for file in tests/build-stage40/*.meowb; do
    # Lấy tên file gốc (bỏ đuôi)
    filename=$(basename "$file" .meowb)
    
    # [QUAN TRỌNG] Tạo một file tạm (.asm), dùng sed để thay thế:
    # Tất cả chuỗi .meowb" thành .meowc" trong code
    # Điều này đảm bảo các lệnh IMPORT/LOAD file sẽ trỏ đúng sang file bytecode mới
    sed 's/\.meowb"/\.meowc"/g' "$file" > "dist/$filename.temp.asm"

    # Compile từ file tạm (.temp.asm) ra file bytecode (.meowc)
    if ./build/release/bin/masm "dist/$filename.temp.asm" "dist/$filename.meowc"; then
        echo "✅ Patched & Compiled: $filename.meowb -> dist/$filename.meowc"
    else
        echo "❌ Lỗi khi compile: $filename"
    fi

    # Xóa file tạm cho gọn nhà cửa
    rm "dist/$filename.temp.asm"
done

echo "------------------------------------------------"
echo "🚀 Chạy thử main..."
./build/release/bin/meow-vm -b dist/main.meowc
