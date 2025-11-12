#!/bin/sh
# Compile Tick source to executable

set -eu

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.tick>" >&2
    exit 1
fi

SRC="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
INTERMEDIATE_DIR="$BUILD_DIR/compiled"
OUTPUT_DIR="$BUILD_DIR/out"
LD="${LD:-clang}"

# Validate source file exists
if [ ! -f "$SRC" ]; then
    echo "Error: Source file '$SRC' not found" >&2
    exit 1
fi

# Extract base name
BASENAME=$(basename "$SRC" .tick)

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "==> Step 1: Building static library"
BUILD_DIR="$BUILD_DIR" "$SCRIPT_DIR/compile-lib.sh" "$SRC"

echo ""
echo "==> Step 2: Linking executable"
# Link with runtime libraries: platform-specific then freestanding
STD_LIB="${STD_LIB:-$BUILD_DIR/libtick_std.a}"
PLATFORM_LIB="${PLATFORM_LIB:-$BUILD_DIR/libtick_platform.a}"
"$LD" -o "$OUTPUT_DIR/$BASENAME" "$OUTPUT_DIR/lib$BASENAME.a" "$PLATFORM_LIB" "$STD_LIB"

echo "==> Success! Executable created at: $OUTPUT_DIR/$BASENAME"
ls -lh "$OUTPUT_DIR/$BASENAME"
file "$OUTPUT_DIR/$BASENAME"
