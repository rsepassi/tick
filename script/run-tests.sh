#!/bin/sh

set -e

# Get variables from the environment, with defaults
COMPILER=${COMPILER:-./build/tick}
BUILD_DIR=${BUILD_DIR:-build}
CC=${CC:-clang}
# CFLAGS, LDFLAGS, STD_LIB, PLATFORM_LIB should be passed from the Makefile

# Ensure build directories exist
mkdir -p "$BUILD_DIR/gen" "$BUILD_DIR/out" "$BUILD_DIR/test/run" "$BUILD_DIR/test/err"

# Find all .tick files in test/run and test/err
RUN_TESTS=$(find test/run -name '*.tick')
ERR_TESTS=$(find test/err -name '*.tick')

for test_file in $RUN_TESTS; do
    base_name=$(basename "$test_file" .tick)
    expected_file=$(echo "$test_file" | sed 's/\.tick/\.expected/')
    output_c_base="$BUILD_DIR/test/run/$base_name"
    output_c="$output_c_base.c"
    output_exe="$BUILD_DIR/out/$base_name"
    output_log="$BUILD_DIR/gen/$base_name.log"
    output_err="$BUILD_DIR/gen/$base_name.err"

    # Step 1: Transpile Tick to C
    if ! "$COMPILER" emitc "$test_file" -o "$output_c_base" 2> "$output_err"; then
        printf "\033[31mERROR: Tick compilation failed for %s\033[0m\n" "$test_file"
        cat "$output_err"
        exit 1
    fi

    # Step 2: Compile C to executable
    # Check if test defines main, if not use the C wrapper
    if ! grep -q "pub let main" "$test_file"; then
        # Use the C wrapper that provides main() and calls test()
        if ! "$CC" $CFLAGS $LDFLAGS -I"$BUILD_DIR/test/run" -o "$output_exe" \
            "script/test_main.c" "$output_c" "$STD_LIB" "$PLATFORM_LIB" 2>> "$output_err"; then
            printf "\033[31mERROR: C compilation/linking failed for %s\033[0m\n" "$test_file"
            cat "$output_err"
            exit 1
        fi
    else
        # Test has its own main, compile normally
        if ! "$CC" $CFLAGS $LDFLAGS -o "$output_exe" "$output_c" "$STD_LIB" "$PLATFORM_LIB" 2>> "$output_err"; then
            printf "\033[31mERROR: C compilation/linking failed for %s\033[0m\n" "$test_file"
            cat "$output_err"
            exit 1
        fi
    fi

    # Step 3: Run the executable
    if ! "$output_exe" > "$output_log" 2>> "$output_err"; then
        printf "\033[31mERROR: Runtime failed for %s\033[0m\n" "$test_file"
        cat "$output_err"
        exit 1
    fi

    # Step 4: Compare output with expected (filtering out [DEBUG] lines)
    grep -v '^\[DEBUG\]' "$output_log" > "$output_log.filtered" 2>/dev/null || true
    if diff -u "$expected_file" "$output_log.filtered"; then
        printf "\033[32mPASS: %s\033[0m\n" "$test_file"
    else
        printf "\033[31mFAIL: %s (output mismatch)\033[0m\n" "$test_file"
        exit 1
    fi
done

for test_file in $ERR_TESTS; do
    base_name=$(basename "$test_file" .tick)
    expected_file=$(echo "$test_file" | sed 's/\.tick/\.expected/')
    output_c_base="$BUILD_DIR/test/err/$base_name"
    output_c="$output_c_base.c"
    output_err="$BUILD_DIR/gen/$base_name.err"

    # Step 1: Attempt to transpile Tick to C, capturing stderr
    # We expect this to fail, so we don't exit on non-zero status here.
    "$COMPILER" emitc "$test_file" -o "$output_c_base" > /dev/null 2> "$output_err" || true

    # Step 2: Compare error output with expected
    # Filter out malloc debug line and [DEBUG] lines which are non-deterministic
    grep -v 'malloc: nano zone abandoned' "$output_err" | grep -v '^\[DEBUG\]' > "$output_err.tmp" 2>/dev/null || true
    mv "$output_err.tmp" "$output_err"

    if diff -u "$expected_file" "$output_err"; then
        printf "\033[32mPASS: %s\033[0m\n" "$test_file"
    else
        printf "\033[31mFAIL: %s (error output mismatch)\033[0m\n" "$test_file"
        diff -u "$expected_file" "$output_err"
        exit 1
    fi
done

echo ""
printf "\033[32mAll tests passed!\033[0m\n"
