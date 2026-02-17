
# Configurable paths
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := .

# Output binary
TARGET := $(BIN_DIR)/cwtch

# Compiler settings
CC := clang
CFLAGS := -Wall -Wextra -O3 -flto -march=native -MMD -MP
LDFLAGS := -flto -lm

# Debug settings (for valgrind/gdb)
DEBUG_CFLAGS := -Wall -Wextra -O1 -g -MMD -MP
DEBUG_LDFLAGS := -lm

# Find all .c files in source directory
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Rebuild
rebuild: clean all

# Debug build (for valgrind/gdb)
debug: clean
	mkdir -p $(BUILD_DIR)
	$(CC) $(DEBUG_CFLAGS) $(SRCS) -o $(TARGET) $(DEBUG_LDFLAGS)

# ──────────────────────────────────────────────────────────────────
# WebAssembly targets
# Requires: source ~/emsdk/emsdk_env.sh
# ──────────────────────────────────────────────────────────────────

EMCC := emcc
WASM_NAME    ?= Lozza
WASM_VERSION ?= 11
WASM_OUTPUT  ?= lozza.js
WASM_CFLAGS  := -O3 -msimd128 -Wall -DENGINE_NAME='"$(WASM_NAME)"' -DVERSION='"$(WASM_VERSION)"'
WASM_LDFLAGS := -lm
WASM_MEMORY  := -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576

wasm: $(SRCS) wasm/pre.js wasm/post.js
	$(EMCC) $(WASM_CFLAGS) $(SRCS) -o $(WASM_OUTPUT) \
	  $(WASM_LDFLAGS) $(WASM_MEMORY) \
	  -sSINGLE_FILE=1 \
	  -sENVIRONMENT=node,worker \
	  -sINVOKE_RUN=0 \
	  -sEXIT_RUNTIME=1 \
	  -sEXPORTED_FUNCTIONS='["_main","_wasm_init","_wasm_exec","_malloc","_free"]' \
	  -sEXPORTED_RUNTIME_METHODS='["callMain","stringToUTF8","lengthBytesUTF8"]' \
	  --pre-js wasm/pre.js \
	  --post-js wasm/post.js

.PHONY: all clean rebuild debug wasm

# Include generated dependency files
-include $(DEPS)
