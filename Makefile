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

# Lemon parser generator
LEMON := $(BUILD_DIR)/vendor/lemon
LEMON_TEMPLATE := vendor/lemon/lempar.c

# Generated parser (in build/gen/)
GRAMMAR_SRC := src/tick.y
GENPARSESRC := $(BUILD_DIR)/gen/tick_grammar.c

# Embedded runtime header
RUNTIME_HEADER := src/tick_runtime.h
RUNTIME_EMBEDDED := $(BUILD_DIR)/gen/tick_runtime_embedded.h

# Source files
HDRS := $(shell find src -name '*.h')
SRCS := $(shell find src -name '*.c')
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# Add generated parser object
GENPARSEOBJ := $(BUILD_DIR)/gen/tick_grammar.o

# Main target
BINARY := $(BUILD_DIR)/tick

.PHONY: all grammar clean format test tree compile

all: $(BINARY)

grammar: $(GENPARSEOBJ)

$(BINARY): $(RUNTIME_EMBEDDED) $(OBJS) $(GENPARSEOBJ) $(SRCS) $(HDRS)
	$(LD) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(GENPARSEOBJ)

$(BUILD_DIR)/src/codegen.o: src/codegen.c $(HDRS) $(RUNTIME_EMBEDDED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(GENPARSESRC): $(GRAMMAR_SRC) $(LEMON)
	@mkdir -p $(BUILD_DIR)/gen
	$(LEMON) -T$(LEMON_TEMPLATE) -d$(BUILD_DIR)/gen -p $(GRAMMAR_SRC)
	@mv $(BUILD_DIR)/gen/tick.c $(GENPARSESRC)
	@mv $(BUILD_DIR)/gen/tick.h $(BUILD_DIR)/gen/tick_grammar.h

$(GENPARSEOBJ): $(GENPARSESRC)
	$(CC) $(CFLAGS) -Wno-unused-parameter -Wno-unused-variable -c $(GENPARSESRC) -o $(GENPARSEOBJ)

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

$(LEMON):
	@mkdir -p $(dir $@)
	$(CC) -o $@ -std=c99 vendor/lemon/lemon.c

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(SRCS) $(HDRS)

tree:
	@tree -I vibe -I vendor

test: $(BINARY)
	@mkdir -p $(BUILD_DIR)/gen
	@echo "Testing hello.tick..."
	./build/tick emitc examples/hello.tick -o build/gen/hello
	@echo "Testing grammar.tick..."
	./build/tick emitc examples/grammar.tick -o build/gen/grammar

compile: $(BINARY)
	@mkdir -p $(BUILD_DIR)/compiled
	@test -n "$(SRC)" || (echo "Error: SRC variable not set. Usage: make compile SRC=path/to/file.tick" && exit 1)
	./build/tick emitc $(SRC) -o $(BUILD_DIR)/compiled/$(basename $(notdir $(SRC)))

.SUFFIXES:  # Disable built-in suffix rules (prevents yacc from running on .y files)
