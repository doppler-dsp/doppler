# example-projects/burst-pipeline — a downstream project's own Makefile.
#
# This is the file a copier runs first, so it is deliberately thin: CMake is
# the build system, and this wraps the two commands you would otherwise type.
# `make help` lists everything.
#
# Nothing here is doppler-specific tooling. Copy the directory, keep this
# file, and it keeps working against whatever prefix you install doppler to.

BUILD_DIR        ?= build
CMAKE_BUILD_TYPE ?= Release
# Where doppler is installed. Empty means "a system install CMake can already
# find"; set it to a `cmake --install` tree or an extracted release tarball:
#   make PREFIX=~/.local run
PREFIX           ?=

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
ifneq ($(strip $(PREFIX)),)
CMAKE_FLAGS += -DCMAKE_PREFIX_PATH=$(abspath $(PREFIX))
endif

.PHONY: all build run clean help
.DEFAULT_GOAL := build

all: build

# Configure every time rather than gating on CMakeCache.txt: re-running
# configure is cheap and idempotent, while a cached one silently ignores a
# changed PREFIX — which is the mistake this target exists to prevent.
build:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR)

# BOTH link modes, because the static one is where a missing transitive
# dependency actually shows up. The program self-validates and exits non-zero
# on any failed check, so `make run` succeeding is the whole test.
run: build
	$(BUILD_DIR)/burst_pipeline_shared
	@if [ -x $(BUILD_DIR)/burst_pipeline_static ]; then \
	    echo; echo "── again, linked statically ──"; \
	    $(BUILD_DIR)/burst_pipeline_static; \
	else \
	    echo "note: no static target — this install ships no doppler::doppler-static"; \
	fi

# The program writes its output beside itself when run in place, so clean
# takes that too -- otherwise `make clean` leaves the interesting half behind.
clean:
	rm -rf $(BUILD_DIR) burst_train.blue

help:
	@echo "burst-pipeline — a burst waveform end to end"
	@echo
	@echo "  make build              configure and compile"
	@echo "  make run                build, then run both link modes"
	@echo "  make clean              remove $(BUILD_DIR)/"
	@echo
	@echo "  PREFIX=<dir>            where doppler is installed"
	@echo "  CMAKE_BUILD_TYPE=<t>    default Release"
	@echo "  BUILD_DIR=<dir>         default build"
