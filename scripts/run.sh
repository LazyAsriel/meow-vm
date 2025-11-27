#!/bin/bash

# Kiểm tra tham số
if [ -z "$1" ]; then
    echo "Usage: ./run.sh <file_name>"
    exit 1
fi

FILE="$1"
MEOW_FILE="tests/$FILE.meow"
BYTECODE_FILE="tests/$FILE.meowb"

# Chạy masm để build bytecode
build/fast-debug/bin/masm "$MEOW_FILE" "$BYTECODE_FILE"

# Nếu build thành công thì chạy VM
if [ $? -eq 0 ]; then
    build/fast-debug/bin/meow-vm "$BYTECODE_FILE"
else
    echo "Build failed!"
    exit 1
fi
