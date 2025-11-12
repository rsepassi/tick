# Tick compiler Makefile

# Build configuration
RELEASE ?= 0

# Compiler and flags
CC := clang
LD := clang
CFLAGS := -std=c11 -Wall -Werror -Wpedantic -Wextra \
					-Wvla -Wno-gnu-zero-variadic-macro-arguments \
					-Isrc

# Debug vs Release flags
ifeq ($(RELEASE),0)
	CFLAGS += -O1 -DTICK_DEBUG -g3 -fno-omit-frame-pointer \
						-fsanitize=address,undefined
	LDFLAGS += -fsanitize=address,undefined
else
	CFLAGS += -O2
endif

# Build directory
BUILD_DIR := build

# Lemon parser generator
LEMON := $(BUILD_DIR)/vendor/lemon
LEMON_TEMPLATE := vendor/lemon/lempar.c

# Generated parser (in build/gen/)
GRAMMAR_SRC := src/tick.y
GENPARSESRC := $(BUILD_DIR)/gen/tick_grammar.c

# Source files
HDRS := $(shell find src -name '*.h')
SRCS := $(shell find src -name '*.c')
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# Add generated parser object
GENPARSEOBJ := $(BUILD_DIR)/gen/tick_grammar.o

# Main target
BINARY := $(BUILD_DIR)/tick

.PHONY: all grammar clean format test tree
.SUFFIXES:  # Disable built-in suffix rules (prevents yacc from running on .y files)

all: $(BINARY)

grammar: $(GENPARSEOBJ)

$(BINARY): $(OBJS) $(GENPARSEOBJ) $(SRCS) $(HDRS)
	$(LD) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(GENPARSEOBJ)

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
	@echo "Testing hello.tick..."
	./build/tick emitc examples/hello.tick -o build/hello
	@echo "Testing grammar.tick..."
	./build/tick emitc examples/grammar.tick -o build/grammar
