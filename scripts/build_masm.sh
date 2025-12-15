#!/usr/bin/env bash
set -e

# =========================
# Kiểm tra tham số
# =========================
if [ -z "$1" ]; then
    echo "❌ Thiếu thư mục input!"
    echo "👉 Cách dùng: ./scripts/build_masm.sh <src_dir>"
    exit 1
fi

SRC_DIR="$(cd "$1" && pwd)"

# =========================
# Đường dẫn
# =========================
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_ROOT="$ROOT_DIR/dist"

MASM="$ROOT_DIR/build/release/bin/masm"

# =========================
# Kiểm tra masm
# =========================
if [ ! -x "$MASM" ]; then
    echo "❌ Không tìm thấy masm!"
    echo "👉 Build masm trước đã, bình tĩnh nào 😼"
    exit 1
fi

# =========================
# Xác định thư mục output
# =========================
REL_PATH="$(realpath --relative-to="$ROOT_DIR" "$SRC_DIR")"
OUT_DIR="$DIST_ROOT/$REL_PATH"

echo "📂 Input : $SRC_DIR"
echo "📦 Output: $OUT_DIR"

# =========================
# Dọn output tương ứng
# =========================
echo "🧹 Dọn output cũ..."
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# =========================
# Compile bằng masm (giữ cấu trúc thư mục)
# =========================
echo "⚙️  MASM compile (giữ nguyên cây thư mục)"

find "$SRC_DIR" -type f -name "*.meowc" | while read -r file; do
    rel_file="$(realpath --relative-to="$SRC_DIR" "$file")"
    out_file="$OUT_DIR/$rel_file"
    out_dir="$(dirname "$out_file")"

    mkdir -p "$out_dir"

    temp_asm="$out_dir/.$(basename "$file").tmp.asm"

    # Patch IMPORT/LOAD
    sed 's/\.meowb"/\.meowc"/g' "$file" > "$temp_asm"

    echo "🐱 masm $rel_file"
    "$MASM" "$temp_asm" "$out_file"

    rm "$temp_asm"
done

echo "✨ Xong. Output nằm tại:"
echo "   $OUT_DIR"
