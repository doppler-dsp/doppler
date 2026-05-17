# doppler-jm project Makefile
#
# Targets:
#   make              Configure + build (Release)
#   make test         CTest + pytest
#   make bench        C + Python benchmarks (output only)
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
# Prefer: explicit override > activated venv > local .venv > parent .venv > system python
PYTHON     ?= $(or \
  $(JUST_BUILDIT_PYTHON),\
  $(and $(VIRTUAL_ENV),$(VIRTUAL_ENV)/bin/python3),\
  $(shell test -x ../.venv/bin/python3     && echo $$(pwd)/../.venv/bin/python3),\
  $(shell test -x .venv/bin/python3        && echo $$(pwd)/.venv/bin/python3),\
  $(shell command -v python3 2>/dev/null))
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
	@$(PYTHON) -c "import numpy, pytest" 2>/dev/null || \
		{ echo "ERROR: numpy/pytest not found in $(PYTHON) — activate the project venv first"; exit 1; }
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

bench: build
	@for b in $(BUILD_DIR)/bench_*_core; do [ -x "$$b" ] && echo "--- $$b ---" && "$$b" && echo; done
	@for f in src/doppler_jm/benchmarks/bench_*.py; do \
		[ -f "$$f" ] && echo "--- $$f ---" && $(PYTHON) "$$f" && echo; \
	done

just-build: build
	mkdir -p $(JUST_BUILDIT_OUTPUT_DIR)
	cp -r src/doppler_jm $(JUST_BUILDIT_OUTPUT_DIR)/doppler_jm

docs:
	@command -v doxygen >/dev/null 2>&1 || 	  { echo "doxygen not found — install it first"; exit 1; }
	doxygen Doxyfile
	@echo "Docs written to docs/doxygen/html/index.html"

clean:
	rm -rf $(BUILD_DIR)
	find src -name "*.so" -o -name "*.pyd" | xargs rm -f 2>/dev/null; true

help:
	@echo ""
	@echo "doppler-jm build targets"
	@echo ""
	@echo "  make               Configure + build"
	@echo "  make test          Run CTest + pytest"
	@echo "  make bench         Run C + Python benchmarks"
	@echo "  make docs          Generate Doxygen API docs"
	@echo "  make clean         Remove build artifacts"
	@echo ""
