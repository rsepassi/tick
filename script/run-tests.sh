#!/bin/sh

set -e

# Get variables from the environment, with defaults
COMPILER=${COMPILER:-./build/tick}
BUILD_DIR=${BUILD_DIR:-build}
CC=${CC:-clang}
# CFLAGS, LDFLAGS, STD_LIB, PLATFORM_LIB should be passed from the Makefile

# Ensure build directories exist
mkdir -p "$BUILD_DIR/gen" "$BUILD_DIR/out" "$BUILD_DIR/test/run" "$BUILD_DIR/test/err"

# Check if running a single test
SINGLE_TEST=""
if [ -n "$TEST" ]; then
    SINGLE_TEST="test/$TEST.tick"
    if [ ! -f "$SINGLE_TEST" ]; then
        printf "\033[31mERROR: Test file not found: %s\033[0m\n" "$SINGLE_TEST"
        exit 1
    fi
    printf "\033[36mRunning single test: %s\033[0m\n" "$SINGLE_TEST"
fi

# Find all .tick files in test/run and test/err
if [ -n "$SINGLE_TEST" ]; then
    # Run only the specified test
    if echo "$SINGLE_TEST" | grep -q "^test/run/"; then
        RUN_TESTS="$SINGLE_TEST"
        ERR_TESTS=""
    else
        RUN_TESTS=""
        ERR_TESTS="$SINGLE_TEST"
    fi
else
    # Run all tests
    RUN_TESTS=$(find test/run -name '*.tick' | sort)
    ERR_TESTS=$(find test/err -name '*.tick' | sort)
fi

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
    if [ -n "$SINGLE_TEST" ]; then
        # For single test, show output directly and don't fail on non-zero exit
        printf "\033[36m--- Output from %s ---\033[0m\n" "$test_file"
        "$output_exe" 2>&1 || true
        printf "\033[36m--- End of output ---\033[0m\n"
    else
        # For batch tests, capture and compare output
        if ! "$output_exe" > "$output_log" 2>&1; then
            printf "\033[31mERROR: Runtime failed for %s\033[0m\n" "$test_file"
            cat "$output_log"
            exit 1
        fi

        # Compare output with expected (filtering out [DEBUG] lines)
        grep -v '^\[DEBUG\]' "$output_log" > "$output_log.filtered" 2>/dev/null || true
        if diff -u "$expected_file" "$output_log.filtered"; then
            printf "\033[32mPASS: %s\033[0m\n" "$test_file"
        else
            printf "\033[31mFAIL: %s (output mismatch)\033[0m\n" "$test_file"
            exit 1
        fi
    fi
done

for test_file in $ERR_TESTS; do
    base_name=$(basename "$test_file" .tick)
    expected_file=$(echo "$test_file" | sed 's/\.tick/\.expected/')
    output_c_base="$BUILD_DIR/test/err/$base_name"
    output_c="$output_c_base.c"
    output_exe="$BUILD_DIR/out/$base_name"
    output_err="$BUILD_DIR/gen/$base_name.err"

    # Step 1: Attempt to transpile Tick to C, capturing stderr
    if [ -n "$SINGLE_TEST" ]; then
        # For single test, show all output directly
        printf "\033[36m--- Compiler output for %s ---\033[0m\n" "$test_file"
        if "$COMPILER" emitc "$test_file" -o "$output_c_base" 2>&1; then
            # Compilation succeeded - this is a runtime error test
            printf "\033[36m--- Compilation succeeded, compiling and running ---\033[0m\n"

            # Check if test defines main, if not use the C wrapper
            if ! grep -q "pub let main" "$test_file"; then
                "$CC" $CFLAGS $LDFLAGS -I"$BUILD_DIR/test/err" -o "$output_exe" \
                    "script/test_main.c" "$output_c" "$STD_LIB" "$PLATFORM_LIB" 2>&1 || true
            else
                "$CC" $CFLAGS $LDFLAGS -o "$output_exe" "$output_c" "$STD_LIB" "$PLATFORM_LIB" 2>&1 || true
            fi

            printf "\033[36m--- Runtime output ---\033[0m\n"
            { "$output_exe" 2>&1 ; } 2>/dev/null || true
        fi
        printf "\033[36m--- End of output ---\033[0m\n"
    else
        # For batch tests, capture and compare error output
        if "$COMPILER" emitc "$test_file" -o "$output_c_base" > /dev/null 2> "$output_err"; then
            # Compilation succeeded - this is a runtime error test
            # Compile the C code
            if ! grep -q "pub let main" "$test_file"; then
                "$CC" $CFLAGS $LDFLAGS -I"$BUILD_DIR/test/err" -o "$output_exe" \
                    "script/test_main.c" "$output_c" "$STD_LIB" "$PLATFORM_LIB" 2>> "$output_err" || true
            else
                "$CC" $CFLAGS $LDFLAGS -o "$output_exe" "$output_c" "$STD_LIB" "$PLATFORM_LIB" 2>> "$output_err" || true
            fi

            # Run and capture output (expect failure)
            # Suppress shell's "Abort trap" message
            { "$output_exe" > "$output_err" 2>&1 ; } 2>/dev/null || true
        fi

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
    fi
done

echo ""
printf "\033[32mAll tests passed!\033[0m\n"
