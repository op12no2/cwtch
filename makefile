# Compiler & basic config
CC := clang
BUILD ?= release
ARCH ?= native

EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=

# Detect platform
UNAME_S := $(shell uname -s)
# MSYS2/MinGW environments report MINGW*/MSYS*/CYGWIN*; Windows shells often have OS=Windows_NT
IS_WINDOWS := $(filter Windows_NT,$(OS))
IS_MINGW   := $(findstring MINGW,$(UNAME_S))
IS_MSYS    := $(findstring MSYS,$(UNAME_S))
IS_CYGWIN  := $(findstring CYGWIN,$(UNAME_S))

# If any of these are non-empty, treat as Windows target (MSYS2/MinGW/Cygwin)
ifneq ($(or $(IS_WINDOWS),$(IS_MINGW),$(IS_MSYS),$(IS_CYGWIN)),)
  PLATFORM := windows
else
  PLATFORM := linux
endif

# Executable name
ifeq ($(PLATFORM),windows)
  EXEEXT := .exe
else
  EXEEXT :=
endif

BIN := cwtch$(EXEEXT)
SRC := cwtch.c

# Base flags
ifeq ($(BUILD),release)
  CFLAGS  := -O3 -march=$(ARCH) -flto -DNDEBUG -pthread $(EXTRA_CFLAGS)
  LDFLAGS := -flto -fuse-ld=lld $(EXTRA_LDFLAGS)
else
  ifeq ($(BUILD),dev)
    CFLAGS  := -O3 -march=native -flto -pthread $(EXTRA_CFLAGS)   # keep asserts
    LDFLAGS := -flto -fuse-ld=lld $(EXTRA_LDFLAGS)
  else
    ifeq ($(BUILD),debug)
      CFLAGS  := -Og -g3 -Wall -Wextra -fno-omit-frame-pointer -pthread $(EXTRA_CFLAGS)
      LDFLAGS := $(EXTRA_LDFLAGS)
    else
      $(error BUILD must be 'release', 'dev', or 'debug')
    endif
  endif
endif

# Windows-only: link winpthread statically; keep everything else dynamic
ifeq ($(PLATFORM),windows)
  LDFLAGS += -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
endif

.PHONY: all clean diag
all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

clean:
	rm -f $(BIN)

# Optional: build with vectorization diagnostics
diag: CFLAGS += -Rpass=loop-vectorize 
diag: all

