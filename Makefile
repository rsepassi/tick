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

# Source files
HDRS := $(shell find src -name '*.h')
SRCS := $(shell find src -name '*.c')
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# Main target
BINARY := $(BUILD_DIR)/tick

.PHONY: all clean format test tree

all: $(BINARY)

$(BINARY): $(OBJS) $(SRCS) $(HDRS)
	$(LD) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: %.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/vendor/lemon:
	@mkdir -p $(dir $@)
	$(CC) -o $@ -std=c99 vendor/lemon/lemon.c

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(SRCS) $(HDRS)

tree:
	@tree -I vibe -I vendor

test: $(BINARY)
	./build/tick emitc examples/hello.tick -o build/hello
