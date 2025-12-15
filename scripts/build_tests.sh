#!/usr/bin/env bash
set -e

# =========================
# Kiểm tra tham số
# =========================
if [ -z "$1" ]; then
    echo "❌ Thiếu stage!"
    echo "👉 Cách dùng: ./scripts/build_tests.sh <stage>"
    exit 1
fi

STAGE="$1"

# =========================
# Cấu hình đường dẫn
# =========================
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COMPILER_DIR="$ROOT_DIR/compiler"
TEST_SRC="$COMPILER_DIR/builds/build-stage${STAGE}"
DIST_DIR="$ROOT_DIR/dist"

MASM="$ROOT_DIR/build/release/bin/masm"
MEOW_VM="$ROOT_DIR/build/release/bin/meow-vm"

# =========================
# Build compiler theo stage
# =========================
echo "🐱 Build compiler stage $STAGE..."
cd "$COMPILER_DIR"
./scripts/build.sh "$STAGE"
./scripts/meow.sh -s "$STAGE"

# =========================
# Chuẩn bị dist
# =========================
echo "🧹 Dọn dẹp dist cũ..."
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# =========================
# Compile test files
# =========================
echo "⚙️  Compile tests từ: $TEST_SRC"

for file in "$TEST_SRC"/*.meowb; do
    filename="$(basename "$file" .meowb)"
    temp_asm="$DIST_DIR/$filename.temp.asm"
    out_bytecode="$DIST_DIR/$filename.meowc"

    # Patch IMPORT/LOAD
    sed 's/\.meowb"/\.meowc"/g' "$file" > "$temp_asm"

    if "$MASM" "$temp_asm" "$out_bytecode"; then
        echo "✅ Compiled: $filename.meowb → $filename.meowc"
    else
        echo "❌ Compile lỗi: $filename"
        exit 1
    fi

    rm "$temp_asm"
done

# =========================
# Chạy thử main
# =========================
echo "------------------------------------------------"
echo "🚀 Chạy thử main.meowc"
"$MEOW_VM" -b "$DIST_DIR/main.meowc"
