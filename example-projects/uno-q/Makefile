# example-projects/uno-q — a downstream project's own Makefile.
#
# Deliberately thin: CMake is the build system, and this wraps the commands
# you would otherwise type. `make help` lists everything.

BUILD_DIR        ?= build
CMAKE_BUILD_TYPE ?= Release
# Where doppler is installed. Empty means "a system install CMake can already
# find"; set it to a `cmake --install` tree or an extracted release tarball:
#   make PREFIX=~/.local run
PREFIX           ?=
# Extra compiler flags for THIS program, e.g. CPU_FLAGS=-mcpu=cortex-a53 on
# an Arduino UNO Q, where -march=native resolves to a generic ARMv8 target
# (see README.md). doppler itself is tuned when IT is built, not here.
CPU_FLAGS        ?=

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
ifneq ($(strip $(PREFIX)),)
CMAKE_FLAGS += -DCMAKE_PREFIX_PATH=$(abspath $(PREFIX))
endif
ifneq ($(strip $(CPU_FLAGS)),)
CMAKE_FLAGS += "-DCMAKE_C_FLAGS=$(CPU_FLAGS)"
endif

.PHONY: all build run clean help
.DEFAULT_GOAL := build

all: build

# Configure every time rather than gating on CMakeCache.txt: re-running
# configure is cheap and idempotent, while a cached one silently ignores a
# changed PREFIX or CPU_FLAGS.
build:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR)

# The self-test, in BOTH link modes: the static one is where a missing
# transitive dependency shows up. It checks its own result and exits
# non-zero on any failure, so `make run` succeeding is the whole test.
run: build
	$(BUILD_DIR)/uno_q_shared
	@if [ -x $(BUILD_DIR)/uno_q_static ]; then \
	    echo; echo "── again, linked statically ──"; \
	    $(BUILD_DIR)/uno_q_static; \
	else \
	    echo "note: no static target — this install ships no doppler::doppler-static"; \
	fi

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "uno-q — an RTL-SDR receive front end (cu8 -> U8ToF32 -> DDC -> PSD)"
	@echo
	@echo "  make build              configure and compile"
	@echo "  make run                build, then self-test in both link modes"
	@echo "  make clean              remove $(BUILD_DIR)/"
	@echo
	@echo "  PREFIX=<dir>            where doppler is installed"
	@echo "  CPU_FLAGS=<flags>       e.g. -mcpu=cortex-a53"
	@echo "  CMAKE_BUILD_TYPE=<t>    default Release"
	@echo "  BUILD_DIR=<dir>         default build"
	@echo
	@echo "  live input: $(BUILD_DIR)/uno_q_shared [--offset HZ ...] -   (cu8 on stdin)"
