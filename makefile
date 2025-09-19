# Simple single-file build for cwtch.c

CC := clang
BUILD ?= release
ARCH ?= native

EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=

BIN := cwtch
SRC := cwtch.c

ifeq ($(BUILD),release)
  CFLAGS := -O3 -march=$(ARCH) -flto -DNDEBUG $(EXTRA_CFLAGS)
  LDFLAGS := -flto -fuse-ld=lld $(EXTRA_LDFLAGS)
else ifeq ($(BUILD),debug)
  CFLAGS := -Og -g3 -Wall -Wextra -fno-omit-frame-pointer $(EXTRA_CFLAGS)
  LDFLAGS := $(EXTRA_LDFLAGS)
else
  $(error BUILD must be 'release' or 'debug')
endif

.PHONY: all clean diag
all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

clean:
	rm -f $(BIN)

# Optional: build with vectorization diagnostics
diag: CFLAGS += -Rpass=loop-vectorize -Rpass=slp-vectorize \
               -Rpass-missed=loop-vectorize -Rpass-missed=slp-vectorize \
               -Rpass-analysis=loop-vectorize -Rpass-analysis=slp-vectorize \
               -fsave-optimization-record
diag: all

