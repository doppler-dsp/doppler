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
# ── coverage (clang source-based) ─────────────────────────────────────────────
# `make coverage` builds a dedicated instrumented tree and merges the harness
# .profraw into one llvm-cov report. clang + matching llvm tools required;
# override the tool names if they are version-suffixed (e.g. llvm-cov-22).
COV_DIR       ?= build-cov
LLVM_PROFDATA ?= llvm-profdata
LLVM_COV      ?= llvm-cov
# Excluded from the report: vendored code, jm-generated binding aggregators
# (`<mod>_ext.c`) and per-object fragments (`<mod>_ext_<obj>.c`), and the
# test/bench harnesses themselves — only first-party _core.c counts.
# `native/src/app/` (the wfmgen CLI) is excluded too: its body (wfmgen_core) is
# an OBJECT lib compiled into BOTH the `wfmgen` executable and a `.so`, but the
# report attributes only to the `.so` — whose copy is never executed (nothing
# calls `doppler_wfmgen()` through the `.so`), so the CLI's real coverage from
# the `wfmgen_cli` ctest can't be attributed. The CLI is instead byte-parity
# integration-tested (the `wfmgen_cli` ctest + test_compose byte-parity gate).
COV_IGNORE    ?= (^|/)(vendor|build|build-cov|native/src/app)/|_ext(_[a-z0-9_]+)?\.c$$|/(tests|benchmarks)/
# Python executable used when building extensions with `make pyext`.
# Defaults to the uv-managed venv Python so the extension suffix always
# matches the active interpreter.  Override on the command line:
#   make pyext PYTHON_EXECUTABLE=/usr/bin/python3.13
PYTHON_EXECUTABLE ?= $(shell uv run python -c \
    'import sys; print(sys.executable)' 2>/dev/null || which python3)
PYTHON_EXECUTABLE := $(or $(JUST_BUILDIT_PYTHON),$(PYTHON_EXECUTABLE))
# Extra cmake args passed through to every configure step.
# Example: make build CMAKE_ARGS="-DENABLE_SIMD=OFF"
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

.PHONY: all build test coverage coverage-gate pyext \
        wheel just-build python-test rust-test test-all lint docs docs-serve docs-relink gen-c-api doxygen \
        specan record-demo gallery \
        bench bench-report bench-publish bench-interleaved bench-docs \
        bench-stream \
        debug release blazing bump-version check-version tag-release \
        test-examples test-examples-python install-deps setup clean help

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

# ── coverage (clang source-based; phases 1+2 = C tests ∪ Python → merged) ─────
# Instruments every first-party C object once (DOPPLER_COVERAGE). The same
# OBJECT libs flow into the C-test exes AND the Python .so, so both harnesses
# emit .profraw that merge into one report attributed back to the hand-written
# _core.c. clang and matching llvm-profdata/llvm-cov are required. The
# Python .so is staged into a throwaway package ($(COV_DIR)/pkg) so the
# instrumented build never clobbers the dev's src/doppler/ .so. Phase 3
# (Rust) folds cargo .profraw into the same merge.
coverage:
	$(CMAKE) -B $(COV_DIR) -S . \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_COMPILER=clang \
		-DDOPPLER_COVERAGE=ON -DBUILD_PYTHON=ON \
		-DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) \
		-DPYTHON_PACKAGE_DIR=$(CURDIR)/$(COV_DIR)/pkg/doppler \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(COV_DIR) --parallel $(NPROC)
	# Stage an importable package: the instrumented .so are already in
	# pkg/doppler/<mod>/; layer the Python source (everything but .so) over them
	# so pytest imports the instrumented extension. GNU tar (Linux coverage job).
	mkdir -p $(COV_DIR)/pkg/doppler
	(cd src/doppler && tar cf - --exclude='*.so' .) \
		| (cd $(COV_DIR)/pkg/doppler && tar xf -)
	rm -rf $(COV_DIR)/prof && mkdir -p $(COV_DIR)/prof
	# C tests → per-process .profraw.
	cd $(COV_DIR) && LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/c-%p-%m.profraw" \
		$(CTEST) --output-on-failure
	# Python tests against the instrumented .so. Coverage is not a correctness
	# gate (the test jobs are), so a failing assert still counts the C lines it
	# executed — don't abort the merge on it (leading `-`).
	-LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/py-%p-%m.profraw" \
		PYTHONPATH="$(CURDIR)/$(COV_DIR)/pkg" \
		$(PYTHON_EXECUTABLE) -m pytest $(COV_DIR)/pkg/doppler \
		-q -p no:cacheprovider --ignore-glob='*/benchmarks/*'
	# Rust FFI tests against the instrumented libdoppler.so. DOPPLER_BUILD_DIR
	# points build.rs at build-cov; the .so self-writes its C .profraw via its
	# own profile runtime (no Rust instrumentation needed — and rustc's LLVM
	# matches clang's, so the .profraw merges). Skipped gracefully if no cargo.
	-DOPPLER_BUILD_DIR="$(CURDIR)/$(COV_DIR)" \
		LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/rs-%p-%m.profraw" \
		cargo test --manifest-path $(RUST_DIR)/Cargo.toml
	# Merge C ∪ Python ∪ Rust and report against libdoppler.so + every module
	# .so (the cores live in all; listing the .so captures lines only a wrapper
	# reaches).
	@objs="$(COV_DIR)/libdoppler.so $$(ls $(COV_DIR)/pkg/doppler/*/*.so \
		2>/dev/null | sed 's/^/-object /' | tr '\n' ' ')"; \
	$(LLVM_PROFDATA) merge -sparse $(COV_DIR)/prof/*.profraw \
		-o $(COV_DIR)/doppler.profdata; \
	$(LLVM_COV) report $$objs -instr-profile=$(COV_DIR)/doppler.profdata \
		-ignore-filename-regex='$(COV_IGNORE)'; \
	$(LLVM_COV) show $$objs -instr-profile=$(COV_DIR)/doppler.profdata \
		-ignore-filename-regex='$(COV_IGNORE)' \
		-format=html -output-dir=$(COV_DIR)/html; \
	$(LLVM_COV) export $$objs -instr-profile=$(COV_DIR)/doppler.profdata \
		-ignore-filename-regex='$(COV_IGNORE)' -format=lcov \
		> $(COV_DIR)/coverage.lcov
	# Relativize the SF: paths (llvm-cov emits absolute) so the patch gate's
	# git-relative diff paths match — see `make coverage-gate`.
	sed -i 's|SF:$(CURDIR)/|SF:|' $(COV_DIR)/coverage.lcov
	@echo "coverage: HTML -> $(COV_DIR)/html/index.html  lcov -> $(COV_DIR)/coverage.lcov"

# ── coverage-gate (phase 4: diff-cover patch gate) ────────────────────────────
# Fail if the C lines a PR adds/changes aren't covered by the merged report at
# >= COV_PATCH_MIN%. Compares the working tree to COV_BASE (origin/main in CI).
# Run after `make coverage`. .py/.rs changes carry no C coverage data and are
# simply not counted here (no false failures); C is where the algorithms live.
COV_BASE      ?= origin/main
COV_PATCH_MIN ?= 90
coverage-gate:
	uvx diff-cover $(COV_DIR)/coverage.lcov \
		--compare-branch=$(COV_BASE) \
		--fail-under=$(COV_PATCH_MIN) \
		--format html:$(COV_DIR)/patch.html \
		--format markdown:$(COV_DIR)/patch.md

# ── pyext ─────────────────────────────────────────────────────────────────────
# Build Python C extensions into src/doppler/**/.
# Re-configures with BUILD_PYTHON=ON (default is OFF for C-only builds).
#
# UV_SYNC_FLAGS can be overridden by dependents (e.g. just-build passes
# --no-group docs so the wheel build path never downloads the docs toolchain).
UV_SYNC_FLAGS ?=
pyext:
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) \
		-DBUILD_PYTHON=ON \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)
	uv sync $(UV_SYNC_FLAGS)

# ── just-build ────────────────────────────────────────────────────────────────
# PEP 517 build hook for just-buildit.
# just-buildit sets JUST_BUILDIT_OUTPUT_DIR and JUST_BUILDIT_PYTHON before
# calling this target. The package tree is copied there to be packaged.
#
# Exclude the docs group: a docs-dep network blip must not fail a wheel build.
just-build: UV_SYNC_FLAGS = --no-group docs
just-build: pyext
	mkdir -p $(JUST_BUILDIT_OUTPUT_DIR)
	cp -r $(PYEXT_DIR) $(JUST_BUILDIT_OUTPUT_DIR)/doppler

# ── test-examples ─────────────────────────────────────────────────────────────
# Smoke-test every standalone C example. Excluded (each needs a live
# NATS peer or terminal interaction, not just a broker): transmitter,
# receiver, pipeline_demo, spectrum_analyzer.
EXAMPLE_BIN_DIR := $(BUILD_DIR)/examples/c
STANDALONE_BUILD_DIR := examples/standalone/build
test-examples: build
	@echo "Running C example smoke tests..."
	@for ex in nco_demo fir_demo hbdecim_demo fft_demo \
	           agc_demo cic_demo corr_demo rate_converter_demo; do \
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


# Fail-closed: every src/doppler/examples/*.py (plus the standalone
# example) is discovered and run by the pytest gate -- no hand list to
# rot. Skips live in src/doppler/examples/.examples-skip (reasons
# mandatory). See src/doppler/tests/test_examples.py.
test-examples-python:
	uv run pytest -m examples -q src/doppler/tests/test_examples.py

# ── test-all ──────────────────────────────────────────────────────────────────
test-all: test test-examples python-test test-examples-python

# ── python-test ───────────────────────────────────────────────────────────────
python-test:
	uv run pytest src/ -v

# ── install-deps ─────────────────────────────────────────────────────────────
# Bootstraps jbx (to $HOME/.local/bin, same as CI) if it isn't already on
# PATH, then installs system build deps (detects OS/distro). The bootstrap
# runs `| bash`, not `. <(...)`, so the installed binary persists across
# make's per-recipe-line subshells; the PATH prefix on the second line
# covers a jbx that was *just* installed in the first line's own subshell.
install-deps:
	@command -v jbx >/dev/null 2>&1 || curl -sSL https://just-buildit.github.io/get-jb.sh | bash
	PATH="$$HOME/.local/bin:$$PATH" jbx install-deps

# ── setup ─────────────────────────────────────────────────────────────────────
setup:
	uv sync
	pre-commit install

# ── lint ──────────────────────────────────────────────────────────────────────
lint:
	@test -f .git/hooks/pre-commit || pre-commit install
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

# ── bench-stream ──────────────────────────────────────────────────────────────
# Transport (P0) bench: NATS firehose throughput + status-plane RTT via
# the bench_stream C harness. Self-contained — starts a JetStream broker on an
# isolated port (temp store) and tears it down. Prints a table; pass
# VERSION=X.Y.Z to stamp benchmarks/published/v<ver>/stream.json (rendered into
# the Transport section of docs/benchmarks.md by `make bench-docs`).
#
#   make bench-stream                    # scratch run, prints the table
#   make bench-stream VERSION=0.21.0     # publish stream.json for the release
bench-stream:
	uv run python scripts/bench_stream.py $(if $(VERSION),--publish $(VERSION),)

bench-report:
	uv run python scripts/bench_report.py


# ── rust-test ─────────────────────────────────────────────────────────────────
rust-test: build
	cargo test --manifest-path $(RUST_DIR)/Cargo.toml

# ── docs ──────────────────────────────────────────────────────────────────────
# zensical reads mkdocs.yml natively — no config migration required.
# The docs toolchain (zensical + its bundled material theme, mkdocstrings,
# mkdoxy) lives in the `docs` dependency group, which is NOT auto-synced — pass
# `--group docs` on every invocation so a clean checkout renders identically to
# CI (`uv sync --group dev --group docs`). Without it, `uv run zensical` relies
# on incidental venv state and can fall back to a themeless (no-left-nav) build.
# A stale local `zensical.toml` (gitignored, absent in CI) shadows mkdocs.yml
# and silently truncates the nav, so remove it first.
# gen-c-api is a separate step; docs/c-api/ is committed and only needs
# regeneration when native/inc/ headers change.
docs:
	rm -f zensical.toml
	uv run --group docs zensical build --clean --strict

docs-serve:
	rm -f zensical.toml
	uv run --group docs zensical serve

# Regenerates the "## Related pages" block on every docs/api/*.md page from
# gallery/guide/design/dev cross-links (scripts/gen_related_pages.py),
# README.md's synced body from docs/index.md (scripts/gen_readme.py),
# and tests/install/build-*-deps.sh from jb.toml (scripts/gen_install_scripts.py).
docs-relink:
	uv run python scripts/gen_related_pages.py --write
	uv run python scripts/gen_readme.py --write
	uv run python scripts/gen_install_scripts.py --write

gen-c-api:
	rm -rf docs/c-api .mkdoxy .capi-site
	uv run --group docs mkdocs build -f mkdocs-capi.yml
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
# Run before releasing whenever src/doppler/examples/ has changed.
GALLERY_SCRIPTS := \
    src/doppler/examples/agc_demo.py \
    src/doppler/examples/cic_demo.py \
    src/doppler/examples/corr_demo.py \
    src/doppler/examples/detection_curves.py \
    src/doppler/examples/detection_sim.py \
    src/doppler/examples/detection2d_demo.py \
    src/doppler/examples/lockdet_demo.py \
    src/doppler/examples/telemetry_fanin_demo.py \
    src/doppler/examples/rate_converter_demo.py \
    src/doppler/examples/ddc_fn_demo.py \
    src/doppler/examples/ddc_fn_scaling.py \
    src/doppler/examples/adc_demo.py \
    src/doppler/examples/hbdecim_q15_demo.py \
    src/doppler/examples/wfmgen_demo.py \
    src/doppler/examples/symbols_demo.py \
    src/doppler/examples/wfm_composition_demo.py \
    src/doppler/examples/wcdma_carriers_demo.py \
    src/doppler/examples/plan_demo.py \
    src/doppler/examples/measure_demo.py \
    src/doppler/examples/measure_imd_npr_demo.py \
    src/doppler/examples/wfm_write_demo.py \
    src/doppler/examples/dsss_despread_demo.py \
    src/doppler/examples/awgn_demo.py \
    src/doppler/examples/wfm_io_demo.py \
    src/doppler/examples/dsss_burst_pipeline_demo.py \
    src/doppler/examples/dsss_acq_async_data_demo.py \
    src/doppler/examples/dsss_despread_async_data_demo.py

gallery:
	@echo "Regenerating gallery plots..."
	@for script in $(GALLERY_SCRIPTS); do \
	    printf "  %-45s" "$$script"; \
	    uv run python $$script > /dev/null 2>&1 && echo "OK" || { echo "FAIL"; exit 1; }; \
	done
	@mv -f agc_convergence.png cic_demo_spectrum.png corr_demo.png detection_curves.png detection_sim.png detection2d_demo.png lockdet_demo.png telemetry_fanin_demo.png rate_converter_demo.png ddc_fn_demo.png ddc_fn_scaling.png adc_demo.png hbdecim_q15_demo.png wfmgen_demo.png symbols_demo.png wfm_composition_demo.png wcdma_carriers_demo.png plan_demo.png measure_demo.png measure_imd_npr_demo.png wfm_write_demo.png dsss_despread_demo.png wfm_io_demo.png dsss_burst_pipeline_demo.png dsss_acq_async_data_demo.png dsss_acq_async_data_demo_diversity.png dsss_despread_async_data_demo.png docs/assets/
	@rm -f burst.blue
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
# Start a release: branch off origin/main, bump the version. Then edit
# CHANGELOG.md, commit, push, open a PR, and let CI gate it — main is never
# pushed to directly.  The explicit origin/main start point matters: a bare
# `checkout -b` forks from whatever HEAD the invoker happens to be on (a
# feature branch, a stale main), silently building the release on the wrong
# base — the bump then misses anything merged since.
#   make release-branch VERSION=0.2.0
release-branch:
ifndef VERSION
	@echo "usage: make release-branch VERSION=<x.y.z>"
	@exit 1
endif
	git fetch origin main
	git checkout -b chore/release-$(VERSION) origin/main
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
	@echo "  make install-deps  Bootstrap jbx (if missing) + install system build deps"
	@echo "  make setup         One-time per clone: uv sync + pre-commit install"
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
	@echo "  make docs-relink   Regenerate docs/api/*.md's Related pages blocks"
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
