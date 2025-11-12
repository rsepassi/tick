#!/bin/sh
# Compile Tick source to static library (.a)

set -eu

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.tick>" >&2
    exit 1
fi

SRC="$1"
COMPILER="${COMPILER:-./build/tick}"
BUILD_DIR="${BUILD_DIR:-build}"
INTERMEDIATE_DIR="$BUILD_DIR/compiled"
OUTPUT_DIR="$BUILD_DIR/out"
CC="${CC:-clang}"
AR="${AR:-llvm-ar}"

# Validate source file exists
if [ ! -f "$SRC" ]; then
    echo "Error: Source file '$SRC' not found" >&2
    exit 1
fi

# Validate compiler exists
if [ ! -x "$COMPILER" ]; then
    echo "Error: Compiler '$COMPILER' not found or not executable" >&2
    exit 1
fi

# Extract base name
BASENAME=$(basename "$SRC" .tick)

# Create output directories
mkdir -p "$INTERMEDIATE_DIR"
mkdir -p "$OUTPUT_DIR"

"$COMPILER" emitc "$SRC" -o "$INTERMEDIATE_DIR/$BASENAME"

"$CC" -std=c11 -Wpedantic \
    -Wall -Wextra -Werror -Wvla \
    -g3 -O1 -fno-omit-frame-pointer \
    -ffreestanding \
    -I "$INTERMEDIATE_DIR" \
    -o "$INTERMEDIATE_DIR/$BASENAME.o" \
    -c "$INTERMEDIATE_DIR/$BASENAME.c"

"$AR" rcs "$OUTPUT_DIR/lib$BASENAME.a" "$INTERMEDIATE_DIR/$BASENAME.o"
