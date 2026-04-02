# doppler — project wrapper Makefile
#
# All build artifacts go to BUILD_DIR/ (never to the repo root).
#
# Overrides (pass on the command line or export from the environment):
#   BUILD_DIR    Build directory            (default: build)
#   BUILD_TYPE   CMake build type           (default: RelWithDebInfo)
#   PREFIX       Install prefix             (default: /usr/local)
#   DOCKER_IMAGE Docker image name          (default: doppler)
#   NPROC        Parallel build jobs        (default: nproc || 4)
#
# Examples:
#   make                          # configure + build
#   make test                     # run CTest suite
#   make pyext                    # build Python C extensions
#   make install PREFIX=$HOME/.local
#   make install-test PREFIX=$HOME/.local
#   make python-test              # pytest
#   make test-all                 # all test suites (C + Python + Rust)
#   make docker                   # build Docker image
#   make docker-test              # build + run container tests
#   make debug                    # clean + Debug build
#   make release                  # clean + Release build
#   make clean                    # remove build/ and Python .so files
#   make help                     # show this message

SHELL        = /bin/sh
BUILD_DIR   ?= build
BUILD_TYPE  ?= Release
PREFIX      ?= /usr/local
PYEXT_DIR   ?= python/dsp/doppler
RUST_DIR    ?= ffi/rust
C_DIR       ?= c
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

.PHONY: all build test rust-test rust-examples install install-test pyext \
        wheel python-test test-all docs-build docs-serve \
        specan record-demo \
        docker docker-test \
        debug release blazing gen-pyext bump-version tag-release clean help \
        install-cli

# ── default ──────────────────────────────────────────────────────────────────
all: build

# ── configure (internal) ─────────────────────────────────────────────────────
$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		$(CMAKE_ARGS)

# ── build ─────────────────────────────────────────────────────────────────────
build: $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)

# ── test (CTest) ──────────────────────────────────────────────────────────────
test:
	$(CTEST) --test-dir $(BUILD_DIR)/$(C_DIR) --output-on-failure

# ── rust-test ─────────────────────────────────────────────────────────────────
# FFT tests share global plan state — run single-threaded.
# build.rs bakes rpath on Linux/macOS; PATH is used on Windows.
rust-test: build
	cd $(RUST_DIR) && \
		LD_LIBRARY_PATH=$(CURDIR)/$(BUILD_DIR)/$(C_DIR) \
		PATH="$(CURDIR)/$(BUILD_DIR)/$(C_DIR):$(PATH)" \
		cargo test -- --test-threads=1

# ── rust-examples ─────────────────────────────────────────────────────────────
# Build all Rust examples and print their locations.
rust-examples: build
	cd $(RUST_DIR) && cargo build --examples
	@echo ""
	@echo "Rust examples (run directly):"
	@ls $(RUST_DIR)/target/debug/examples/ \
		| grep -E '^[a-z_]+(\.exe)?$$' \
		| sed "s|^|    $(RUST_DIR)/target/debug/examples/|"
	@echo ""

# ── install ───────────────────────────────────────────────────────────────────
install: build
	$(CMAKE) --install $(BUILD_DIR)/$(C_DIR) --prefix $(PREFIX)

# ── install-test ──────────────────────────────────────────────────────────────
install-test:
	bash $(C_DIR)/tests/test_install.sh $(PREFIX)

# ── install-cli ───────────────────────────────────────────────────────────────
install-cli:
	uv pip install python/cli/

# ── pyext ─────────────────────────────────────────────────────────────────────
pyext: build
	$(CMAKE) -DBUILD_PYTHON=ON -B $(BUILD_DIR) -S . \
		-DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) $(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --target pyext

# ── wheel ─────────────────────────────────────────────────────────────────────
# Build the doppler-dsp wheel and repair it with auditwheel (Linux only).
# Output: dist/doppler_dsp-*-cp3XX-cp3XX-manylinux_*.whl
wheel: build
	$(CMAKE) -DBUILD_PYTHON=ON -B $(BUILD_DIR) -S . $(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --target wheel

# ── test-all ──────────────────────────────────────────────────────────────────
test-all: test python-test rust-test

# ── python-test ───────────────────────────────────────────────────────────────
python-test:
	uv run pytest $(PYEXT_DIR)/tests/ -v \
		--cov=$(PYEXT_DIR) --cov-report=term-missing

# ── docs ──────────────────────────────────────────────────────────────────────
docs-build:
	uv run zensical build --clean

docs-serve:
	uv run zensical serve

# ── specan ────────────────────────────────────────────────────────────────────
specan:
	uv run doppler-specan

record-demo:
	uv run python -m doppler_specan.record_demo \
	    --frames 120 --fft-size 512 \
	    -o docs/specan/frames.json

# ── docker ────────────────────────────────────────────────────────────────────
docker:
	docker build -t $(DOCKER_IMAGE) .

docker-test:
	docker run --rm $(DOCKER_IMAGE) /app/test_stream
	docker run --rm $(DOCKER_IMAGE) /app/fft_testbench

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

# ── gen-pyext ─────────────────────────────────────────────────────────────────
# Generate a thin Python C extension from a C module.
#   make gen-pyext MOD=fir            # writes python/ext/dp_fir.c
#   make gen-pyext MOD=fir DRY_RUN=1  # preview only
gen-pyext:
ifndef MOD
	@echo "usage: make gen-pyext MOD=<module>  (e.g. MOD=fir)"
	@exit 1
endif
	python3 tools/gen_pyext.py $(if $(DRY_RUN),--dry-run) $(C_DIR)/src/$(MOD).c

# ── bump-version ──────────────────────────────────────────────────────────────
# Update version in all five places atomically.
#   make bump-version VERSION=0.2.1
bump-version:
ifndef VERSION
	@echo "usage: make bump-version VERSION=<x.y.z>"
	@exit 1
endif
	sed -i 's/^version = "[0-9.]*"/version = "$(VERSION)"/' pyproject.toml
	sed -i 's/^version = "[0-9.]*"/version = "$(VERSION)"/' python/specan/pyproject.toml
	sed -i 's/^version = "[0-9.]*"/version = "$(VERSION)"/' python/cli/pyproject.toml
	sed -i 's/^version = "[0-9.]*"/version = "$(VERSION)"/' $(RUST_DIR)/Cargo.toml
	sed -i 's/^project(doppler VERSION [0-9.]*/project(doppler VERSION $(VERSION)/' CMakeLists.txt
	@echo "Bumped to $(VERSION) in pyproject.toml, specan, cli, Cargo.toml, CMakeLists.txt"
	@echo "Next: review CHANGELOG.md, commit, then: make tag-release VERSION=$(VERSION)"

# ── tag-release ───────────────────────────────────────────────────────────────
# Commit the version bump and push the release tag.
#   make tag-release VERSION=0.2.0
tag-release:
ifndef VERSION
	@echo "usage: make tag-release VERSION=<x.y.z>"
	@exit 1
endif
	git add pyproject.toml $(RUST_DIR)/Cargo.toml CMakeLists.txt
	git commit -m "chore: release v$(VERSION)"
	git tag -a "v$(VERSION)" -m "Release v$(VERSION)"
	git push origin main "v$(VERSION)"
	@echo "Tagged and pushed v$(VERSION) — release workflow starting on GitHub"

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(PYEXT_DIR)/*.cpython-*.so
	rm -rf $(PYEXT_DIR)/__pycache__ $(PYEXT_DIR)/*/__pycache__

# ── help ──────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "doppler build targets"
	@echo "====================="
	@echo ""
	@echo "  make               Configure + build ($(BUILD_TYPE))"
	@echo "  make build         Same as above"
	@echo "  make test          Run CTest suite"
	@echo "  make rust-test     Run Rust FFI tests (single-threaded)"
	@echo "  make rust-examples Build Rust examples and list paths"
	@echo "  make install       Install to PREFIX (default: $(PREFIX))"
	@echo "  make install-test  Verify installed pkg-config + headers"
	@echo "  make install-cli   Install doppler-cli (adds doppler command)"
	@echo "  make pyext         Build Python C extensions"
	@echo "  make wheel         Build + auditwheel-repair doppler-dsp wheel (Linux)"
	@echo "  make test-all      Run all test suites (C + Python + Rust)"
	@echo "  make python-test   Run pytest"
	@echo "  make specan              Launch live spectrum analyzer in browser"
	@echo "  make docs-build    Build Zensical site"
	@echo "  make docs-serve    Serve Zensical site locally"
	@echo "  make docker        Build Docker image"
	@echo "  make docker-test   Build + run container tests"
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
	@echo "  PREFIX=$(PREFIX)  NPROC=$(NPROC)"
	@echo "  CMAKE_ARGS=        Extra cmake -D flags passed to all configure steps"
	@echo "  BLAZING_CFLAGS=    C flags for 'make blazing' (default: -march=native)"
	@echo ""
