# example-projects/consumer — a downstream project's own Makefile.
#
# The smallest complete thing that links doppler: find_package, one source
# file, two link modes. This wraps the two CMake commands you would otherwise
# type; `make help` lists everything.
#
# CMake is only one of the three ways this project is known to build — see
# README.md for the `cc` and `pkg-config` lines, which CI also exercises.

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

# Both link modes print the SAME thing. That is the point of the pair: the
# static and shared targets are two routes to one library, and a difference
# between them is a packaging bug rather than a program one.
run: build
	$(BUILD_DIR)/consumer_shared
	@if [ -x $(BUILD_DIR)/consumer_static ]; then \
	    echo; echo "── again, linked statically ──"; \
	    $(BUILD_DIR)/consumer_static; \
	else \
	    echo "note: no static target — this install ships no doppler::doppler-static"; \
	fi

# The program writes its output beside itself when run in place, so clean
# takes that too -- otherwise `make clean` leaves the interesting half behind.
clean:
	rm -rf $(BUILD_DIR) consumer.cf32

help:
	@echo "consumer — the smallest project that links doppler"
	@echo
	@echo "  make build              configure and compile"
	@echo "  make run                build, then run both link modes"
	@echo "  make clean              remove $(BUILD_DIR)/"
	@echo
	@echo "  PREFIX=<dir>            where doppler is installed"
	@echo "  CMAKE_BUILD_TYPE=<t>    default Release"
	@echo "  BUILD_DIR=<dir>         default build"
