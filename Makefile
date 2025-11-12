# Tick compiler Makefile

# Build configuration
RELEASE ?= 0

# Build directory
BUILD_DIR := build

# Compiler and flags
CC := clang
LD := clang
CFLAGS := -std=c11 -Wall -Werror -Wpedantic -Wextra \
					-Wvla -Wno-gnu-zero-variadic-macro-arguments \
					-Isrc -I$(BUILD_DIR)/gen

# Debug vs Release flags
ifeq ($(RELEASE),0)
	CFLAGS += -O1 -DTICK_DEBUG -g3 -fno-omit-frame-pointer \
						-fsanitize=address,undefined
	LDFLAGS += -fsanitize=address,undefined
else
	CFLAGS += -O2
endif

.PHONY: all grammar clean format test tree compile-lib compile-exe runtime
COMPILER := $(BUILD_DIR)/tick
all: $(COMPILER)

# ==============================================================================
# Tick compiler
# ==============================================================================
# Source files (exclude runtime - it's built separately)
HDRS := $(shell find src -name '*.h')
SRCS := $(shell find src -name '*.c' -not -path 'src/runtime/*')
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# Embedded runtime header
RUNTIME_HEADER := src/runtime/std/tick_runtime.h
RUNTIME_EMBEDDED := $(BUILD_DIR)/gen/tick_runtime_embedded.h

# Add generated parser object
GENPARSEOBJ := $(BUILD_DIR)/gen/tick_grammar.o

$(COMPILER): $(RUNTIME_EMBEDDED) $(OBJS) $(GENPARSEOBJ) $(SRCS) $(HDRS)
	$(LD) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(GENPARSEOBJ)

$(BUILD_DIR)/src/codegen.o: src/codegen.c $(HDRS) $(RUNTIME_EMBEDDED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(RUNTIME_EMBEDDED): $(RUNTIME_HEADER)
	@mkdir -p $(BUILD_DIR)/gen
	@echo "// Generated from $(RUNTIME_HEADER)" > $@
	@echo "#ifndef TICK_RUNTIME_EMBEDDED_H_" >> $@
	@echo "#define TICK_RUNTIME_EMBEDDED_H_" >> $@
	@echo "" >> $@
	@echo "static const u8 tick_runtime_header[] = {" >> $@
	@od -An -tx1 -v $(RUNTIME_HEADER) | sed 's/ *\([0-9a-f][0-9a-f]\)/0x\1,/g' | sed '$$s/,$$//' >> $@
	@echo "};" >> $@
	@echo "" >> $@
	@echo "static const usz tick_runtime_header_len = sizeof(tick_runtime_header);" >> $@
	@echo "" >> $@
	@echo "#endif  // TICK_RUNTIME_EMBEDDED_H_" >> $@

# ==============================================================================
# Tick grammer
# ==============================================================================
grammar: $(GENPARSEOBJ)

# Lemon parser generator
LEMON := $(BUILD_DIR)/vendor/lemon
LEMON_TEMPLATE := vendor/lemon/lempar.c

# Generated parser (in build/gen/)
GRAMMAR_SRC := src/grammar.y
GENPARSESRC := $(BUILD_DIR)/gen/tick_grammar.c

$(GENPARSESRC): $(GRAMMAR_SRC) $(LEMON)
	@mkdir -p $(BUILD_DIR)/gen
	$(LEMON) -T$(LEMON_TEMPLATE) -d$(BUILD_DIR)/gen -p $(GRAMMAR_SRC)
	@mv $(BUILD_DIR)/gen/$(notdir $(GRAMMAR_SRC:.y=.c)) $(GENPARSESRC)
	@mv $(BUILD_DIR)/gen/$(notdir $(GRAMMAR_SRC:.y=.h)) $(BUILD_DIR)/gen/tick_grammar.h

$(GENPARSEOBJ): $(GENPARSESRC)
	$(CC) $(CFLAGS) -Wno-unused-parameter -Wno-unused-variable -c $(GENPARSESRC) -o $(GENPARSEOBJ)

$(LEMON):
	@mkdir -p $(dir $@)
	$(CC) -o $@ -std=c99 vendor/lemon/lemon.c

# ==============================================================================
# Use the tick compiler and the C compiler to compile tick libraries and
# executables
# ==============================================================================
# Runtime library flags
RUNTIME_CFLAGS := -std=c11 -Wpedantic -Wall -Werror -Wextra -Wvla -O2 \
									-Isrc/runtime/std
STD_CFLAGS := $(RUNTIME_CFLAGS) -ffreestanding

# Standard library (freestanding)
STD_SRCS := $(wildcard src/runtime/std/*.c)
STD_OBJS := $(patsubst src/runtime/std/%.c,$(BUILD_DIR)/runtime/std/%.o,$(STD_SRCS))
STD_LIB := $(BUILD_DIR)/runtime/libtick_std.a

# Platform library (platform-specific)
PLATFORM_SRCS := $(wildcard src/runtime/platform/*.c)
PLATFORM_OBJS := $(patsubst src/runtime/platform/%.c,$(BUILD_DIR)/runtime/platform/%.o,$(PLATFORM_SRCS))
PLATFORM_LIB := $(BUILD_DIR)/runtime/libtick_platform.a

# Runtime libraries
runtime: $(STD_LIB) $(PLATFORM_LIB)

# Standard library (freestanding)
$(BUILD_DIR)/runtime/std/%.o: src/runtime/std/%.c
	@mkdir -p $(dir $@)
	$(CC) $(STD_CFLAGS) -c -o $@ $<

$(STD_LIB): $(STD_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

# Platform library (platform-specific)
$(BUILD_DIR)/runtime/platform/%.o: src/runtime/platform/%.c
	@mkdir -p $(dir $@)
	$(CC) $(RUNTIME_CFLAGS) -c -o $@ $<

$(PLATFORM_LIB): $(PLATFORM_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

compile-lib: $(COMPILER) $(STD_LIB) $(PLATFORM_LIB)
	COMPILER=$(COMPILER) BUILD_DIR=$(BUILD_DIR) CC=$(CC) AR=$(AR) \
		STD_LIB=$(STD_LIB) PLATFORM_LIB=$(PLATFORM_LIB) \
		./script/compile-lib.sh $(SRC)

compile-exe: $(COMPILER) $(STD_LIB) $(PLATFORM_LIB)
	COMPILER=$(COMPILER) BUILD_DIR=$(BUILD_DIR) CC=$(CC) AR=$(AR) LD=$(LD) \
		STD_LIB=$(STD_LIB) PLATFORM_LIB=$(PLATFORM_LIB) \
		./script/compile-exe.sh $(SRC)

# ==============================================================================
# Tasks
# ==============================================================================
clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(SRCS) $(HDRS)

tree:
	@tree -I vibe -I vendor

test: $(COMPILER)
	@mkdir -p $(BUILD_DIR)/gen
	@echo "Testing hello.tick..."
	./build/tick emitc test/hello.tick -o build/gen/hello
	@echo "Testing grammar.tick..."
	./build/tick emitc test/grammar.tick -o build/gen/grammar

.SUFFIXES:  # Disable built-in suffix rules (prevents yacc from running on .y files)
