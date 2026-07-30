# iqtools project Makefile
#
# Targets:
#   make              Configure + build (Release)
#   make test         CTest + pytest
#   make bench        C + Python benchmarks; dated snapshot in benchmarks/history/
#   make just-build   PEP 517 hook for just-buildit
#   make clean        Remove build artifacts
#   make help         Show this message

ifeq ($(OS), Windows_NT)
SHELL      = cmd.exe
NPROC      ?= 4
PYTHON     ?= $(or $(JUST_BUILDIT_PYTHON),$(shell python -c "import sys,pathlib;print(pathlib.Path(sys.executable).as_posix())"))
else
SHELL      = /bin/sh
NPROC      ?= $(shell nproc 2>/dev/null || echo 4)
PYTHON     ?= $(or $(JUST_BUILDIT_PYTHON),$(shell python3 -c "import sys,pathlib;print(pathlib.Path(sys.executable).as_posix())" 2>/dev/null),$(shell python -c "import sys,pathlib;print(pathlib.Path(sys.executable).as_posix())" 2>/dev/null))
endif
BUILD_DIR  ?= build
BUILD_TYPE ?= Release

# On Windows (OS=Windows_NT is always set by the OS itself, regardless of
# shell), force the MinGW Makefiles generator so CMake uses gcc instead of
# MSVC.  MSVC does not support C99 float complex; gcc does.
ifeq ($(OS), Windows_NT)
CMAKE_GENERATOR ?= MinGW Makefiles
CMAKE_GEN_FLAG  := -G "$(CMAKE_GENERATOR)"
else
CMAKE_GEN_FLAG  :=
endif

.PHONY: all build test bench just-build docs clean help

all: build

$(BUILD_DIR)/CMakeCache.txt:
ifeq ($(OS), Windows_NT)
	$(PYTHON) -c "import numpy" 2>nul || $(PYTHON) -m pip install numpy
	$(PYTHON) -c "import pytest" 2>nul || $(PYTHON) -m pip install pytest

else
	@$(PYTHON) -c "import numpy" 2>/dev/null || $(PYTHON) -m pip install numpy
	@$(PYTHON) -c "import pytest" 2>/dev/null || $(PYTHON) -m pip install pytest

endif
	cmake -B $(BUILD_DIR) -S . \
		$(CMAKE_GEN_FLAG) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DPython3_EXECUTABLE=$(PYTHON) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

compile_commands.json: $(BUILD_DIR)/CMakeCache.txt
	cp $(BUILD_DIR)/compile_commands.json $@

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --parallel $(NPROC)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure
ifeq ($(OS), Windows_NT)
	$(PYTHON) -c "import subprocess,sys; r=subprocess.run([sys.executable,'-m','pytest','src/','-v']); sys.exit(0 if r.returncode in(0,5) else r.returncode)"
else
	$(PYTHON) -m pytest src/ -v; ret=$$?; \
		[ $$ret -eq 0 ] || [ $$ret -eq 5 ] || exit $$ret
endif

bench:
	just-makeit bench

coverage:
	cmake -B $(BUILD_DIR)/cov -S . $(CMAKE_GEN_FLAG) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_FLAGS="--coverage -O0" \
		-DPython3_EXECUTABLE=$(PYTHON)
	cmake --build $(BUILD_DIR)/cov --parallel $(NPROC)
	ctest --test-dir $(BUILD_DIR)/cov --output-on-failure
	lcov --capture --directory $(BUILD_DIR)/cov \
		--output-file $(BUILD_DIR)/cov/coverage.info \
		--ignore-errors inconsistent 2>/dev/null || \
	  lcov --capture --directory $(BUILD_DIR)/cov \
		--output-file $(BUILD_DIR)/cov/coverage.info
	lcov --remove $(BUILD_DIR)/cov/coverage.info '/usr/*' '*/tests/*' \
		--output-file $(BUILD_DIR)/cov/coverage_filtered.info \
		--ignore-errors unused
	mkdir -p docs/coverage/c
	genhtml $(BUILD_DIR)/cov/coverage_filtered.info \
		--output-directory docs/coverage/c
	@echo "C coverage: docs/coverage/c/index.html"
	$(PYTHON) -m pytest src/ \
		--cov=iqtools \
		--cov-report=html:docs/coverage/python \
		--cov-report=term-missing
	@echo "Python coverage: docs/coverage/python/index.html"

just-build:
	cmake -B $(BUILD_DIR) -S . \
		$(CMAKE_GEN_FLAG) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DPython3_EXECUTABLE=$(JUST_BUILDIT_PYTHON) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build $(BUILD_DIR) --parallel $(NPROC)
	mkdir -p $(JUST_BUILDIT_OUTPUT_DIR)
	cp -r src/iqtools $(JUST_BUILDIT_OUTPUT_DIR)/iqtools

docs: build
	@command -v doxygen >/dev/null 2>&1 || \
	  { echo "doxygen not found — install it first"; exit 1; }
	doxygen Doxyfile
	@echo "C API docs: docs/doxygen/html/index.html"
	@command -v zensical >/dev/null 2>&1 || \
	  { echo "zensical not found — uv add --dev zensical mkdocstrings-python"; exit 1; }
	zensical build
	@echo "Python API docs: site/index.html"

clean:
	rm -rf $(BUILD_DIR) site docs/coverage docs/doxygen
	find src -name "*.so" -o -name "*.pyd" | xargs rm -f 2>/dev/null; true

help:
	@echo ""
	@echo "iqtools build targets"
	@echo ""
	@echo "  make               Configure + build"
	@echo "  make test          Run CTest + pytest"
	@echo "  make bench         Run C + Python benchmarks"
	@echo "  make coverage      C (lcov) + Python (pytest-cov) coverage reports"
	@echo "  make docs          Doxygen (C) + Zensical (Python) API docs"
	@echo "  make clean         Remove build artifacts"
	@echo ""
