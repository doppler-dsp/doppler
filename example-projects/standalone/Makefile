# example-projects/standalone — a downstream project's own Makefile.
#
# The only example here that can build against doppler's BUILD TREE as well as
# an install, because it is what you reach for while hacking on doppler itself.
# `make help` lists everything.

BUILD_DIR        ?= build
CMAKE_BUILD_TYPE ?= Release
# Where doppler is installed (a `cmake --install` tree or an extracted release
# tarball). Empty means "a system install CMake can already find".
PREFIX           ?=
# ...or point at a doppler BUILD tree instead, which needs no install step:
#   make DOPPLER_BUILD_DIR=../../build run
DOPPLER_BUILD_DIR ?=
# Build-tree only: static (default, no runtime dependency) or shared.
LINK             ?= static

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
ifneq ($(strip $(DOPPLER_BUILD_DIR)),)
CMAKE_FLAGS += -DDOPPLER_BUILD_DIR=$(abspath $(DOPPLER_BUILD_DIR)) -DDOPPLER_LINK=$(LINK)
else ifneq ($(strip $(PREFIX)),)
CMAKE_FLAGS += -DCMAKE_PREFIX_PATH=$(abspath $(PREFIX))
endif

.PHONY: all build run run-python clean help
.DEFAULT_GOAL := build

all: build

# Configure every time rather than gating on CMakeCache.txt: re-running
# configure is cheap and idempotent, while a cached one silently ignores a
# changed PREFIX or DOPPLER_BUILD_DIR — the mistake this target prevents.
build:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR)

run: build
	$(BUILD_DIR)/awgn_example

# The same thing in Python, for comparison. It needs the wheel
# (`pip install doppler-dsp`) rather than the headers this project compiles
# against — two different ways to have doppler, and the C path above does not
# depend on this one working.
run-python:
	python3 example.py

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "standalone — one AWGN call, in C and in Python"
	@echo
	@echo "  make build                 configure and compile"
	@echo "  make run                   build, then run awgn_example"
	@echo "  make run-python            run example.py (needs the wheel)"
	@echo "  make clean                 remove $(BUILD_DIR)/"
	@echo
	@echo "  DOPPLER_BUILD_DIR=<dir>    build against a doppler build tree"
	@echo "  LINK=static|shared         build-tree link mode (default static)"
	@echo "  PREFIX=<dir>               ...or where doppler is installed"
	@echo "  CMAKE_BUILD_TYPE=<t>       default Release"
	@echo "  BUILD_DIR=<dir>            default build"
