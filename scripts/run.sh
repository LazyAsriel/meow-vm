#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: ./run.sh <file_name> [release|debug]"
    exit 1
fi

FILE="$1"
MEOW_FILE="tests/$FILE.meow"
BYTECODE_FILE="tests/$FILE.meowb"

CONFIG="${2:-debug}"

if [ "$CONFIG" != "release" ] && [ "$CONFIG" != "debug" ]; then
    echo "Error: Invalid configuration option. Use 'release' or 'debug'."
    exit 1
fi

BUILD_PATH="build/$CONFIG/bin"

echo "Using configuration: $CONFIG"

"$BUILD_PATH/masm" "$MEOW_FILE" "$BYTECODE_FILE"

if [ $? -eq 0 ]; then
    echo "Build successful. Running VM..."
    "$BUILD_PATH/meow-vm" "$BYTECODE_FILE"
else
    echo "Build failed!"
    exit 1
fi