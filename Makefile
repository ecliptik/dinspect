# Makefile -- build dinspect.exe with Open Watcom (16-bit real-mode DOS).
#
# Toolchain resolution: uses $(WATCOM) if already set in the environment
# (a normal Open Watcom install exports this), otherwise falls back to a
# vendored copy under tools/watcom -- mirroring how the sibling doskutsu
# project resolves its DJGPP toolchain. Override with:
#   make WATCOM_PREFIX=/path/to/open-watcom
#
# Usage:
#   make            # build dinspect.exe
#   make clean

REPO_ROOT    := $(abspath .)
ORIG_PATH    := $(PATH)
WATCOM_PREFIX := $(if $(WATCOM),$(WATCOM),$(REPO_ROOT)/tools/watcom)

export WATCOM  := $(WATCOM_PREFIX)
export INCLUDE := $(WATCOM_PREFIX)/h
export PATH    := $(WATCOM_PREFIX)/binl:$(WATCOM_PREFIX)/binw:$(ORIG_PATH)

CC      := wcl
BUILD_DIR := build

# -bt=dos   target DOS
# -ms       small memory model
# -zq       quiet
# -w4       warnings on
# -k8192    8KB stack (default is a couple KB, too tight once static data
#           grows across phases; keep runtime stack-overflow checks on
#           rather than disabling them with -s)
CFLAGS_COMMON := -bt=dos -ms -zq -w4 -k8192 -fo=$(BUILD_DIR)/ -fr=$(BUILD_DIR)/

# -0  8086-baseline: everything except cpu386.c, for widest hardware
#     compatibility. See src/cpu386.h for why that file alone needs -5.
# -5  Pentium: cpu386.c uses PUSHFD/CPUID/RDTSC, which Watcom's inline
#     assembler refuses below this target -- it gates accepted mnemonics
#     at compile time, not just codegen. Safe here because cpu.c only
#     ever calls into cpu386.c after its own 8086-safe runtime checks
#     have confirmed the CPU supports what's being called.
CFLAGS_BASE := $(CFLAGS_COMMON) -0
CFLAGS_ADV  := $(CFLAGS_COMMON) -5

SRC_ADV  := src/cpu386.c
SRC_BASE := $(filter-out $(SRC_ADV),$(wildcard src/*.c))

OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_BASE)) \
        $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_ADV))

EXE := dinspect.exe

.PHONY: all clean

all: $(EXE)

$(BUILD_DIR)/cpu386.o: src/cpu386.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS_ADV) -fo=$@ src/cpu386.c

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS_BASE) -fo=$@ $<

$(EXE): $(OBJS)
	$(CC) $(CFLAGS_BASE) -fe=$(EXE) $(OBJS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(EXE)
	rm -rf $(BUILD_DIR)
