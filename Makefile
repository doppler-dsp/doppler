# doppler — project wrapper Makefile
#
# All build artifacts go to BUILD_DIR/ (never to the repo root).
#
# Overrides (pass on the command line or export from the environment):
#   BUILD_DIR    Build directory            (default: build)
#   BUILD_TYPE   CMake build type           (default: Release)
#   NPROC        Parallel build jobs        (default: nproc || 4)
#
# Examples:
#   make                          # configure + build
#   make test                     # run CTest suite (native/ unit tests)
#   make pyext                    # build Python C extensions
#   make python-test              # pytest
#   make bench                    # run C + Python benchmarks, save JSON to benchmarks/history/
#   make bench-python             # Python benchmarks only
#   make bench-c                  # C benchmarks only
#   make test-all                 # CTest + pytest
#   make debug                    # clean + Debug build
#   make release                  # clean + Release build
#   make clean                    # remove build/ and Python .so files
#   make help                     # show this message

SHELL        = /bin/sh
BUILD_DIR   ?= build
BUILD_TYPE  ?= Release
PREFIX      ?= /usr/local
PYEXT_DIR   ?= src/doppler
PY_BUILD_DIR ?= $(BUILD_DIR)
RUST_DIR    ?= ffi/rust
DOCKER_IMAGE ?= doppler
NPROC       ?= $(shell nproc 2>/dev/null || \
                       sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
CTEST       := ctest
CMAKE       := cmake
# Python executable used when building extensions with `make pyext`.
# Defaults to the uv-managed venv Python so the extension suffix always
# matches the active interpreter.  Override on the command line:
#   make pyext PYTHON_EXECUTABLE=/usr/bin/python3.13
PYTHON_EXECUTABLE ?= $(shell uv run python -c \
    'import sys; print(sys.executable)' 2>/dev/null || which python3)
PYTHON_EXECUTABLE := $(or $(JUST_BUILDIT_PYTHON),$(PYTHON_EXECUTABLE))
# Extra cmake args passed through to every configure step.
# Example: make build CMAKE_ARGS="-DUSE_FFTW=OFF"
CMAKE_ARGS  ?=
# C flags used by the `blazing` target (-march=native enables all CPU
# extensions: SSE2/AVX on x86-64, NEON/SVE on AArch64, etc.)
BLAZING_CFLAGS ?= -march=native

# ── MSYS2: use the MinGW-native cmake, not the MSYS POSIX cmake ─────────────
# /usr/bin/cmake.exe is the MSYS build; it does not understand Windows drive-
# letter paths (C:/...) so every compiler-steering trick fails.
# /$(_MSYSTEM_LC)/bin/cmake.exe is the MinGW native build; it speaks Windows
# paths natively and auto-detects the correct MinGW GCC from PATH without any
# extra -D flags.
# Install (if missing):  pacman -S mingw-w64-ucrt-x86_64-cmake
# make(1) inherits the shell environment, so $(MSYSTEM) is available here.
ifneq ($(filter UCRT64 MINGW64 MINGW32 CLANG64,$(MSYSTEM)),)
  _MSYSTEM_LC  := $(shell echo '$(MSYSTEM)' | tr '[:upper:]' '[:lower:]')
  _MINGW_CMAKE := $(shell test -x /$(_MSYSTEM_LC)/bin/cmake && \
                          echo /$(_MSYSTEM_LC)/bin/cmake)
  ifneq ($(_MINGW_CMAKE),)
    CMAKE := $(_MINGW_CMAKE)
  else
    $(warning MSYS2: /$(_MSYSTEM_LC)/bin/cmake not found; \
              using /usr/bin/cmake (POSIX build — compiler detection may \
              fail). Fix: pacman -S \
              mingw-w64-$(subst 64,x86_64,$(subst 32,i686,$(_MSYSTEM_LC)))\
              -cmake)
  endif
endif

.PHONY: all build test pyext \
        wheel just-build python-test rust-test test-all docs-build docs-serve gen-c-api doxygen \
        specan record-demo \
        bench bench-python bench-c \
        debug release blazing bump-version check-version tag-release \
        test-examples clean help

# ── default ──────────────────────────────────────────────────────────────────
all: build

# ── configure (internal) ─────────────────────────────────────────────────────
$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) \
		$(CMAKE_ARGS)

# ── build ─────────────────────────────────────────────────────────────────────
build: $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)

# ── test (CTest — native/ unit tests) ────────────────────────────────────────
# Requires BUILD_PYTHON=ON (pyext configures native/ which registers tests).
test:
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

# ── pyext ─────────────────────────────────────────────────────────────────────
# Build Python C extensions into src/doppler/**/.
# Re-configures with BUILD_PYTHON=ON (default is OFF for C-only builds).
pyext:
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) \
		-DBUILD_PYTHON=ON \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)

# ── just-build ────────────────────────────────────────────────────────────────
# PEP 517 build hook for just-buildit.
# just-buildit sets JUST_BUILDIT_OUTPUT_DIR and JUST_BUILDIT_PYTHON before
# calling this target. The package tree is copied there to be packaged.
just-build: pyext
	mkdir -p $(JUST_BUILDIT_OUTPUT_DIR)
	cp -r $(PYEXT_DIR) $(JUST_BUILDIT_OUTPUT_DIR)/doppler

# ── test-examples ─────────────────────────────────────────────────────────────
# Smoke-test the standalone C examples (DSP only — streaming examples require
# a live transmitter and are excluded from automated runs).
EXAMPLE_BIN_DIR := $(BUILD_DIR)/native/examples
test-examples: build
	@echo "Running C example smoke tests..."
	@for ex in nco_demo fir_demo hbdecim_demo fft_demo; do \
	    printf "  %-20s" "$$ex"; \
	    if $(EXAMPLE_BIN_DIR)/$$ex > /dev/null 2>&1; then \
	        echo "PASS"; \
	    else \
	        echo "FAIL"; exit 1; \
	    fi; \
	done
	@echo "Running Python example smoke tests..."
	@for ex in examples/python/fir_demo.py; do \
	    printf "  %-20s" "$$ex"; \
	    if uv run python $$ex > /dev/null 2>&1; then \
	        echo "PASS"; \
	    else \
	        echo "FAIL"; exit 1; \
	    fi; \
	done
	@echo "All example smoke tests passed."

# ── test-all ──────────────────────────────────────────────────────────────────
test-all: test test-examples python-test

# ── python-test ───────────────────────────────────────────────────────────────
python-test:
	uv run pytest src/ -v

# ── bench ─────────────────────────────────────────────────────────────────────
# Run benchmarks and save dated JSON snapshots to benchmarks/history/.
# Results are committed so perf regressions are visible in git history.
#
# Usage:
#   make bench                    # C + Python (default)
#   make bench-python             # Python only
#   make bench-c                  # C only
#   make bench BENCH_TAG=v1.2.3   # version-tagged snapshot
BENCH_TAG    ?= $(shell date -u +%Y%m%dT%H%M%SZ)
BENCH_JSON    = benchmarks/history/$(BENCH_TAG).json
BENCH_C_JSON  = benchmarks/history/$(BENCH_TAG)-c.json
BENCH_DIRS   := $(shell find src/doppler -type d -name benchmarks | sort | tr '\n' ' ')
BENCH_C_BINS := $(shell find $(BUILD_DIR)/native/src -name 'bench_*' \
                    -type f -not -name '*.o' -not -name '*.d' | sort)

bench: bench-python bench-c

bench-python:
	@mkdir -p benchmarks/history
	uv run pytest $(BENCH_DIRS) \
		--benchmark-only \
		--benchmark-json=$(BENCH_JSON) \
		--benchmark-columns=min,mean,stddev,ops,rounds \
		--benchmark-sort=mean
	@echo "Saved: $(BENCH_JSON)"

bench-c: build
	@mkdir -p benchmarks/history
	uv run python benchmarks/c_bench_json.py \
	    --build-type $(BUILD_TYPE) \
	    $(BENCH_C_BINS) \
	    > $(BENCH_C_JSON)
	@echo "Saved: $(BENCH_C_JSON)"


# ── rust-test ─────────────────────────────────────────────────────────────────
rust-test: build
	cargo test --manifest-path $(RUST_DIR)/Cargo.toml

# ── docs ──────────────────────────────────────────────────────────────────────
docs-build: gen-c-api
	uv run zensical build --clean

docs-serve: gen-c-api
	uv run zensical serve

gen-c-api:
	rm -rf docs/c-api .mkdoxy
	uv run mkdocs build -f mkdocs-capi.yml
	cp -r .mkdoxy/doppler/c-api docs/c-api
	rm -rf .mkdoxy

# ── doxygen ───────────────────────────────────────────────────────────────────
# Generates XML (consumed by mkdocstrings) and HTML in docs/doxygen/.
# HTML_OUTPUT and XML_OUTPUT are relative to the Doxyfile location.
doxygen:
	doxygen Doxyfile

# ── specan ────────────────────────────────────────────────────────────────────
specan:
	uv run doppler-specan

record-demo:
	uv run python -m doppler_specan.record_demo \
	    --frames 120 --fft-size 512 \
	    -o docs/specan/frames.json

# ── debug / release ───────────────────────────────────────────────────────────
debug: clean
	$(MAKE) build BUILD_TYPE=Debug

release: clean
	$(MAKE) build BUILD_TYPE=Release

blazing: clean
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		"-DCMAKE_C_FLAGS=$(BLAZING_CFLAGS)" \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)

# ── bump-version ──────────────────────────────────────────────────────────────
# Update version in all five places atomically.
#   make bump-version VERSION=0.2.1
bump-version:
ifndef VERSION
	@echo "usage: make bump-version VERSION=<x.y.z>"
	@exit 1
endif
	sed -i 's/^version = "[^"]*"/version = "$(VERSION)"/' pyproject.toml
	sed -i 's/^version = "[^"]*"/version = "$(VERSION)"/' src/specan/pyproject.toml
	sed -i 's/^version = "[^"]*"/version = "$(VERSION)"/' src/cli/pyproject.toml
	sed -i "s/^version = \"[0-9.]*/version = \"$$(echo $(VERSION) | sed 's/[^0-9.].*//g')/" $(RUST_DIR)/Cargo.toml
	sed -i "s/^project(doppler VERSION [0-9.]*/project(doppler VERSION $$(echo $(VERSION) | sed 's/[^0-9.].*//g')/" CMakeLists.txt
	@echo "Bumped to $(VERSION) in pyproject.toml, specan, cli, Cargo.toml, CMakeLists.txt"
	@echo "Next: review CHANGELOG.md, commit, then: make tag-release VERSION=$(VERSION)"

# ── check-version ─────────────────────────────────────────────────────────────
# Verify that all five version locations agree.  Run before tagging.
check-version:
	@PY=$$(grep '^version' pyproject.toml | head -1 | sed 's/version = "\(.*\)"/\1/'); \
	 CM=$$(grep '^project(doppler VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/'); \
	 SP=$$(grep '^version' src/specan/pyproject.toml | head -1 | sed 's/version = "\(.*\)"/\1/'); \
	 CL=$$(grep '^version' src/cli/pyproject.toml | head -1 | sed 's/version = "\(.*\)"/\1/'); \
	 RS=$$(grep '^version' $(RUST_DIR)/Cargo.toml | head -1 | sed 's/version = "\(.*\)"/\1/'); \
	 echo "pyproject.toml : $$PY"; \
	 echo "CMakeLists.txt : $$CM"; \
	 echo "specan         : $$SP"; \
	 echo "cli            : $$CL"; \
	 echo "Cargo.toml     : $$RS"; \
	 if [ "$$PY" = "$$CM" ] && [ "$$PY" = "$$SP" ] && [ "$$PY" = "$$CL" ] && [ "$$PY" = "$$RS" ]; then \
	     echo "OK — all versions match ($$PY)"; \
	 else \
	     echo "ERROR — version mismatch; run: make bump-version VERSION=<x.y.z>"; \
	     exit 1; \
	 fi

# ── tag-release ───────────────────────────────────────────────────────────────
# Commit the version bump and push the release tag.
#   make tag-release VERSION=0.2.0
tag-release:
ifndef VERSION
	@echo "usage: make tag-release VERSION=<x.y.z>"
	@exit 1
endif
	$(MAKE) check-version VERSION=$(VERSION)
	git add pyproject.toml src/specan/pyproject.toml src/cli/pyproject.toml \
	        $(RUST_DIR)/Cargo.toml CMakeLists.txt
	git commit -m "chore: release v$(VERSION)"
	git tag -a "v$(VERSION)" -m "Release v$(VERSION)"
	git push origin main "v$(VERSION)"
	@echo "Tagged and pushed v$(VERSION) — release workflow starting on GitHub"

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) $(PY_BUILD_DIR)
	rm -rf html/ xml/
	rm -rf site/
	find $(PYEXT_DIR) -name '*.cpython-*.so' -delete
	find $(PYEXT_DIR) -name '__pycache__' -type d -exec rm -rf {} + 2>/dev/null || true

# ── help ──────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "doppler build targets"
	@echo "====================="
	@echo ""
	@echo "  make               Configure + build ($(BUILD_TYPE))"
	@echo "  make build         Same as above"
	@echo "  make test          Run CTest suite (native/ unit tests; requires pyext)"
	@echo "  make pyext         Build Python C extensions"
	@echo "  make wheel         Build + auditwheel-repair doppler-dsp wheel (Linux)"
	@echo "  make test-all      Run all test suites (CTest + pytest)"
	@echo "  make python-test   Run pytest"
	@echo "  make bench         Run C + Python benchmarks; save JSON to benchmarks/history/"
	@echo "  make bench-python  Run Python benchmarks only"
	@echo "  make bench-c       Run C binary benchmarks only"
	@echo "  make specan              Launch live spectrum analyzer in browser"
	@echo "  make record-demo         Re-record specan demo frames (docs/specan/frames.json)"
	@echo "  make docs-build    Build Zensical site"
	@echo "  make docs-serve    Serve Zensical site locally"
	@echo "  make doxygen       Generate C API docs (XML + HTML via Doxygen)"
	@echo "  make debug         Clean + Debug build"
	@echo "  make release       Clean + Release build"
	@echo "  make blazing       Clean + Release + -march=native (max speed)"
	@echo "  make gen-pyext MOD=fir  Generate Python C extension for a module"
	@echo "  make bump-version VERSION=x.y.z  Update version everywhere"
	@echo "  make tag-release  VERSION=x.y.z  Commit + tag + push release"
	@echo "  make clean         Remove build/ and Python .so files"
	@echo "  make help          Show this message"
	@echo ""
	@echo "Overrides:"
	@echo "  BUILD_DIR=$(BUILD_DIR)  BUILD_TYPE=$(BUILD_TYPE)"
	@echo "  NPROC=$(NPROC)"
	@echo "  CMAKE_ARGS=        Extra cmake -D flags passed to all configure steps"
	@echo "  BLAZING_CFLAGS=    C flags for 'make blazing' (default: -march=native)"
	@echo ""
