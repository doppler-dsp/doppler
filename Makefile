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
#   make bench                    # run C + Python benchmarks, snapshot to benchmarks/history/
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
# Everyday `make build` / `make pyext` target a PORTABLE baseline
# (x86-64-v2) — the same flags every released wheel uses, so a local build
# can never behave differently from what ships.  For host-tuned speed, use
# `make blazing` (sets -DDOPPLER_NATIVE=ON -> -march=native) or pass
# CMAKE_ARGS=-DDOPPLER_NATIVE=ON.  Native builds must never be packaged.
#
# Extra C flags layered on top of -march=native by the `blazing` target.
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
        wheel just-build python-test rust-test test-all lint docs docs-serve gen-c-api doxygen \
        specan record-demo gallery \
        bench bench-report bench-publish bench-interleaved bench-docs \
        debug release blazing bump-version check-version tag-release \
        test-examples test-examples-python clean help

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
	uv sync

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
EXAMPLE_BIN_DIR := $(BUILD_DIR)/examples/c
STANDALONE_BUILD_DIR := examples/standalone/build
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
	@echo "Building standalone example..."
	@cmake -B $(STANDALONE_BUILD_DIR) examples/standalone \
	    -DDOPPLER_BUILD_DIR=$(abspath $(BUILD_DIR)) \
	    -DCMAKE_BUILD_TYPE=Release \
	    > /dev/null 2>&1
	@cmake --build $(STANDALONE_BUILD_DIR) > /dev/null 2>&1
	@printf "  %-20s" "awgn_example"; \
	if $(STANDALONE_BUILD_DIR)/awgn_example > /dev/null 2>&1; then \
	    echo "PASS"; \
	else \
	    echo "FAIL"; exit 1; \
	fi
	@echo "All C example smoke tests passed."

PYTHON_EXAMPLE_SCRIPTS := \
    examples/python/fir_demo.py \
    examples/python/lo_demo.py \
    examples/python/nco_demo.py \
    examples/python/fft_demo.py \
    examples/python/buffers_demo.py \
    examples/python/agc_demo.py \
    examples/python/corr_demo.py \
    examples/python/detection_curves.py \
    examples/python/detection_sim.py \
    examples/python/detection2d_demo.py \
    examples/python/rate_converter_demo.py \
    examples/python/awgn_demo.py \
    examples/python/wfmgen_demo.py \
    examples/python/wfm_composition_demo.py \
    examples/python/wfm_io_demo.py \
    examples/python/pn_codes.py \
    examples/python/wcdma_carriers_demo.py \
    examples/standalone/example.py

test-examples-python:
	@echo "Running Python example smoke tests..."
	@for ex in $(PYTHON_EXAMPLE_SCRIPTS); do \
	    printf "  %-45s" "$$ex"; \
	    if uv run python $$ex > /dev/null 2>&1; then \
	        echo "PASS"; \
	    else \
	        echo "FAIL"; exit 1; \
	    fi; \
	done
	@echo "All Python example smoke tests passed."

# ── test-all ──────────────────────────────────────────────────────────────────
test-all: test test-examples python-test test-examples-python

# ── python-test ───────────────────────────────────────────────────────────────
python-test:
	uv run pytest src/ -v

# ── lint ──────────────────────────────────────────────────────────────────────
lint:
	uv run pre-commit run --all-files

# ── bench ─────────────────────────────────────────────────────────────────────
# Run C + Python benchmarks on THIS machine, snapshot to benchmarks/history/
# (local scratch, gitignored). Use the CLI directly for options, e.g.
# `uvx just-makeit bench --c-only`.
bench: pyext
	uvx just-makeit bench

# ── bench-publish / bench-docs / bench-report ─────────────────────────────────
# Representative published numbers live under benchmarks/published/v<ver>/, two
# builds per release (portable = the wheel, native = -DDOPPLER_NATIVE=ON), each
# stamped with the compiler + flags. Measure on a REAL machine, not CI.
#
#   make bench-interleaved VERSION=X.Y.Z   # measure BOTH builds, denoised
#   make bench-docs                        # render docs/benchmarks.md
#   make bench-report                      # portable trend across releases
#
# bench-interleaved is the canonical path: it builds portable + native in two
# git worktrees and runs them alternately K times (K=5; override with K=N),
# keeping the per-benchmark best so the *from src* column isn't corrupted by
# cross-run drift. bench-publish stamps a single build by hand if you need it.
bench-interleaved:
ifndef VERSION
	@echo "usage: make bench-interleaved VERSION=X.Y.Z [K=5]"; exit 1
endif
	uv run python scripts/bench_interleaved.py $(VERSION) $(if $(K),-k $(K),)

bench-publish:
ifndef VERSION
	@echo "usage: make bench-publish VERSION=X.Y.Z BUILD=portable|native"; exit 1
endif
	uv run python scripts/bench_report.py --publish $(VERSION) \
		--build $(or $(BUILD),portable)

bench-docs:
	uv run python scripts/bench_report.py --page --out docs/benchmarks.md

bench-report:
	uv run python scripts/bench_report.py


# ── rust-test ─────────────────────────────────────────────────────────────────
rust-test: build
	cargo test --manifest-path $(RUST_DIR)/Cargo.toml

# ── docs ──────────────────────────────────────────────────────────────────────
# zensical reads mkdocs.yml natively — no config migration required.
docs: gen-c-api
	uv run zensical build --strict

docs-serve: gen-c-api
	uv run zensical serve

gen-c-api:
	rm -rf docs/c-api .mkdoxy .capi-site
	uv run zensical build -f mkdocs-capi.yml
	cp -r .mkdoxy/doppler/c-api docs/c-api
	# index.md is a hand-written landing page mkdoxy doesn't emit — restore it
	# after the regen wipes it (matches the CI docs.yml step).
	git checkout -- docs/c-api/index.md
	rm -rf .mkdoxy .capi-site

# ── doxygen ───────────────────────────────────────────────────────────────────
# Generates XML (consumed by mkdocstrings) and HTML in docs/doxygen/.
# HTML_OUTPUT and XML_OUTPUT are relative to the Doxyfile location.
doxygen:
	doxygen Doxyfile

# ── specan ────────────────────────────────────────────────────────────────────
specan:
	uv run doppler-specan

record-demo:
	uv run python -m doppler.specan.record_demo \
	    --frames 120 --fft-size 512 \
	    -o docs/specan/frames.json

# ── gallery ───────────────────────────────────────────────────────────────────
# Run all plot-generating examples and copy output PNGs to docs/assets/.
# Run before releasing whenever examples/python/ has changed.
GALLERY_SCRIPTS := \
    examples/python/agc_demo.py \
    examples/python/cic_demo.py \
    examples/python/corr_demo.py \
    examples/python/detection_curves.py \
    examples/python/detection_sim.py \
    examples/python/detection2d_demo.py \
    examples/python/rate_converter_demo.py \
    examples/python/ddc_fn_demo.py \
    examples/python/ddc_fn_scaling.py \
    examples/python/adc_demo.py \
    examples/python/hbdecim_q15_demo.py \
    examples/python/wfmgen_demo.py \
    examples/python/wfm_composition_demo.py \
    examples/python/wcdma_carriers_demo.py \
    examples/python/measure_demo.py \
    examples/python/measure_imd_npr_demo.py

gallery:
	@echo "Regenerating gallery plots..."
	@for script in $(GALLERY_SCRIPTS); do \
	    printf "  %-45s" "$$script"; \
	    uv run python $$script > /dev/null 2>&1 && echo "OK" || { echo "FAIL"; exit 1; }; \
	done
	@mv -f agc_convergence.png cic_demo_spectrum.png corr_demo.png detection_curves.png detection_sim.png detection2d_demo.png rate_converter_demo.png ddc_fn_demo.png ddc_fn_scaling.png adc_demo.png hbdecim_q15_demo.png wfmgen_demo.png wfm_composition_demo.png wcdma_carriers_demo.png measure_demo.png measure_imd_npr_demo.png docs/assets/
	@echo "Gallery plots written to docs/assets/."

# ── debug / release ───────────────────────────────────────────────────────────
debug: clean
	$(MAKE) build BUILD_TYPE=Debug

release: clean
	$(MAKE) build BUILD_TYPE=Release

blazing: clean
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DDOPPLER_NATIVE=ON \
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
	sed -i "s/^version = \"[0-9.]*/version = \"$$(echo $(VERSION) | sed 's/[^0-9.].*//g')/" $(RUST_DIR)/Cargo.toml
	sed -i "s/^project(doppler VERSION [0-9.]*/project(doppler VERSION $$(echo $(VERSION) | sed 's/[^0-9.].*//g')/" CMakeLists.txt
	uv lock      # sync uv.lock's editable doppler-dsp version (else it drifts)
	@echo "Bumped to $(VERSION) in pyproject.toml, Cargo.toml, CMakeLists.txt, uv.lock"
	@echo "Next: edit CHANGELOG.md, commit, push the branch, open a PR, get CI"
	@echo "      green, merge — then on main: make tag-release VERSION=$(VERSION)"

# ── check-version ─────────────────────────────────────────────────────────────
# Verify that all five version locations agree.  Run before tagging.
check-version:
	@PY=$$(grep '^version' pyproject.toml | head -1 | sed 's/version = "\(.*\)"/\1/'); \
	 CM=$$(grep '^project(doppler VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/'); \
	 RS=$$(grep '^version' $(RUST_DIR)/Cargo.toml | head -1 | sed 's/version = "\(.*\)"/\1/'); \
	 echo "pyproject.toml : $$PY"; \
	 echo "CMakeLists.txt : $$CM"; \
	 echo "Cargo.toml     : $$RS"; \
	 if [ "$$PY" = "$$CM" ] && [ "$$PY" = "$$RS" ]; then \
	     echo "OK — all versions match ($$PY)"; \
	 else \
	     echo "ERROR — version mismatch; run: make bump-version VERSION=<x.y.z>"; \
	     exit 1; \
	 fi

# ── release-branch ────────────────────────────────────────────────────────────
# Start a release: branch off main, bump the version. Then edit CHANGELOG.md,
# commit, push, open a PR, and let CI gate it — main is never pushed to directly.
#   make release-branch VERSION=0.2.0
release-branch:
ifndef VERSION
	@echo "usage: make release-branch VERSION=<x.y.z>"
	@exit 1
endif
	git checkout -b chore/release-$(VERSION)
	$(MAKE) bump-version VERSION=$(VERSION)
	@echo ""
	@echo "Now:"
	@echo "  - edit CHANGELOG.md ([Unreleased] -> [$(VERSION)] + compare links)"
	@echo "  - if perf-relevant code changed since the last release (release.md"
	@echo "    §2b): make bench-interleaved VERSION=$(VERSION) && make bench-docs"
	@echo "    (on a representative machine), then commit benchmarks/published +"
	@echo "    docs/benchmarks.md"
	@echo "  - git commit -am 'chore: release v$(VERSION)', git push -u origin HEAD"
	@echo "  - gh pr create --fill, merge once the required checks are green"
	@echo "  - then: git checkout main && git pull && make tag-release VERSION=$(VERSION)"

# ── tag-release ───────────────────────────────────────────────────────────────
# Tag an already-merged, CI-green main commit and push ONLY the tag (the bump +
# CHANGELOG land via a PR first — see release-branch). main is never pushed to
# directly, so the tag always points at code the required checks already passed.
# Triggers release.yml.
#   make tag-release VERSION=0.2.0
tag-release:
ifndef VERSION
	@echo "usage: make tag-release VERSION=<x.y.z>"
	@exit 1
endif
	@test "$$(git rev-parse --abbrev-ref HEAD)" = "main" || \
	  { echo "ERROR: not on main — release tags only point at merged main"; exit 1; }
	git fetch --quiet origin main
	@test "$$(git rev-parse HEAD)" = "$$(git rev-parse origin/main)" || \
	  { echo "ERROR: local main != origin/main — git pull first"; exit 1; }
	$(MAKE) check-version VERSION=$(VERSION)
	git tag -a "v$(VERSION)" -m "Release v$(VERSION)"
	git push origin "v$(VERSION)"
	@echo "Tagged v$(VERSION) on merged main — release workflow starting on GitHub"

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) $(PY_BUILD_DIR)
	rm -rf docs/doxygen/
	rm -rf site/
	find $(PYEXT_DIR) -name '*.cpython-*.so' -delete
	find $(PYEXT_DIR) -name '__pycache__' -type d -exec rm -rf {} + 2>/dev/null || true
	# stray artifacts demo/bench scripts and jm leave in the repo root
	rm -f *.png bench_*.json zensical.toml
	rm -rf __pycache__

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
	@echo "  make test-examples Run C example binaries (build first)"
	@echo "  make test-examples-python  Run Python example smoke tests (requires pyext)"
	@echo "  make python-test   Run pytest"
	@echo "  make bench         Run C + Python benchmarks; snapshot to benchmarks/history/"
	@echo "  make specan              Launch live spectrum analyzer in browser"
	@echo "  make record-demo         Re-record specan demo frames (docs/specan/frames.json)"
	@echo "  make gallery             Run plot examples and copy PNGs to docs/assets/"
	@echo "  make lint          Run pre-commit hooks on all files"
	@echo "  make docs          Build the docs site (zensical)"
	@echo "  make docs-serve    Serve the docs site locally (zensical)"
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
