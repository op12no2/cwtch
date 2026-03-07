
# Configurable paths
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := .

# Output binary
TARGET := $(BIN_DIR)/cwtch

# Compiler settings
CC := clang
BASE_CFLAGS := -Wall -Wextra -O3 -flto -march=native
CFLAGS := $(BASE_CFLAGS) -MMD -MP
LDFLAGS := -flto -lm -lpthread

# Windows cross-compile settings
WIN_CC := clang --target=x86_64-w64-mingw32 -fuse-ld=lld
WIN_TARGET := $(BIN_DIR)/cwtch.exe
WIN_BUILD_DIR := build-win
WIN_BASE_CFLAGS := -Wall -Wextra -O3 -flto -march=native
WIN_CFLAGS := $(WIN_BASE_CFLAGS) -MMD -MP
WIN_LDFLAGS := -flto

# Debug settings (for valgrind/gdb)
DEBUG_CFLAGS := -Wall -Wextra -O1 -g -MMD -MP
DEBUG_LDFLAGS := -lm -lpthread

# Find all .c files in source directory
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
WIN_OBJS := $(patsubst $(SRC_DIR)/%.c,$(WIN_BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
WIN_DEPS := $(WIN_OBJS:.o=.d)

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

# Windows cross-compile
win: $(WIN_TARGET)

$(WIN_TARGET): $(WIN_OBJS) | $(BIN_DIR)
	$(WIN_CC) $(WIN_OBJS) -o $@ $(WIN_LDFLAGS)

$(WIN_BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(WIN_BUILD_DIR)
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

$(WIN_BUILD_DIR):
	mkdir -p $(WIN_BUILD_DIR)

# PGO settings
PGO_DIR := build-pgo
PGO_PROFRAW := $(PGO_DIR)/default_%m.profraw
PGO_PROFDATA := $(PGO_DIR)/merged.profdata
PGO_WORKLOAD := ./cwtch "bench 16"

WIN_PGO_DIR := build-pgo-win
WIN_PGO_PROFRAW := $(WIN_PGO_DIR)/default_%m.profraw
WIN_PGO_PROFDATA := $(WIN_PGO_DIR)/merged.profdata
WIN_PGO_WORKLOAD := ./cwtch.exe "bench 16"

# PGO build (Linux)
pgo:
	@echo "=== PGO Step 1: Instrumented build ==="
	rm -rf $(PGO_DIR) $(BUILD_DIR) $(TARGET)
	mkdir -p $(PGO_DIR)
	mkdir -p $(BUILD_DIR)
	$(CC) $(BASE_CFLAGS) -fprofile-generate=$(PGO_DIR) $(SRCS) -o $(TARGET) $(LDFLAGS)
	@echo "=== PGO Step 2: Running workload ==="
	LLVM_PROFILE_FILE="$(PGO_PROFRAW)" $(PGO_WORKLOAD)
	@echo "=== PGO Step 3: Merging profiles ==="
	llvm-profdata merge -output=$(PGO_PROFDATA) $(PGO_DIR)/*.profraw
	@echo "=== PGO Step 4: Optimized build ==="
	$(CC) $(BASE_CFLAGS) -fprofile-use=$(PGO_PROFDATA) $(SRCS) -o $(TARGET) $(LDFLAGS)
	rm -rf $(PGO_DIR)
	@echo "=== PGO build complete ==="

# PGO build (Windows cross-compile, runs .exe via WSL2)
pgo-win:
	@echo "=== PGO Step 1: Instrumented build ==="
	rm -rf $(WIN_PGO_DIR) $(WIN_BUILD_DIR) $(WIN_TARGET)
	mkdir -p $(WIN_PGO_DIR)
	mkdir -p $(WIN_BUILD_DIR)
	$(WIN_CC) $(WIN_BASE_CFLAGS) -fprofile-generate=$(WIN_PGO_DIR) $(SRCS) -o $(WIN_TARGET) $(WIN_LDFLAGS)
	@echo "=== PGO Step 2: Running workload ==="
	LLVM_PROFILE_FILE="$(WIN_PGO_PROFRAW)" $(WIN_PGO_WORKLOAD)
	@echo "=== PGO Step 3: Merging profiles ==="
	llvm-profdata merge -output=$(WIN_PGO_PROFDATA) $(WIN_PGO_DIR)/*.profraw
	@echo "=== PGO Step 4: Optimized build ==="
	$(WIN_CC) $(WIN_BASE_CFLAGS) -fprofile-use=$(WIN_PGO_PROFDATA) $(SRCS) -o $(WIN_TARGET) $(WIN_LDFLAGS)
	rm -rf $(WIN_PGO_DIR)
	@echo "=== PGO build complete ==="

# Clean
clean:
	rm -rf $(BUILD_DIR) $(WIN_BUILD_DIR) $(PGO_DIR) $(WIN_PGO_DIR) $(TARGET) $(WIN_TARGET)

# Rebuild
rebuild: clean all

# Debug build (for valgrind/gdb)
debug: clean
	mkdir -p $(BUILD_DIR)
	$(CC) $(DEBUG_CFLAGS) $(SRCS) -o $(TARGET) $(DEBUG_LDFLAGS)

-include $(DEPS)
-include $(WIN_DEPS)
