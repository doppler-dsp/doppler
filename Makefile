# doppler — development control centre
#
# CONFIGURATION ONLY. Every shared target lives in `standard.mk`, vendored from
# https://just-buildit.github.io/standard.mk and never edited in place — per-repo
# variation is the variables below, because a local edit is a fork. Design RFC:
# doppler-dsp/doppler#555. Plan + criteria: the just-buildit/.github README.
#
# `make help` is GENERATED from the `##` comment on each target's rule line, so
# there is no hand-written target list here. There used to be, and it advertised
# `make wheel` (which had no rule and exited 0 having done nothing) and
# `make gen-pyext` (which did not exist), while omitting 20 targets that did.
#
# Overrides (pass on the command line or export from the environment):
#   BUILD_DIR    Build directory            (default: build)
#   BUILD_TYPE   CMake build type           (default: Release)
#   NPROC        Parallel build jobs        (default: nproc || 4)
#   CMAKE_ARGS   Extra -D flags for every configure step

# ── Feature flags ────────────────────────────────────────────────────────────
# doppler is the demanding consumer: every group is on.
HAS_C        = 1
HAS_PYTHON   = 1
HAS_RUST     = 1
HAS_DOCS     = 1
HAS_DOXYGEN  = 1
HAS_BENCH    = 1
HAS_COVERAGE = 1
HAS_RELEASE  = 1
HAS_EXAMPLES = 1

# ── Layout ───────────────────────────────────────────────────────────────────
BUILD_DIR   ?= build
BUILD_TYPE  ?= Release
PREFIX      ?= /usr/local
PYEXT_DIR   ?= src/doppler
PY_BUILD_DIR ?= $(BUILD_DIR)
RUST_DIR    ?= ffi/rust
DOCKER_IMAGE ?= doppler
# Container images — the Makefile is the single driver for every docker build;
# CI and release call the docker-* targets, never a raw `docker build` (a stray
# context or a missing --target silently builds the wrong image). Tag + build
# args are overridable; JM_VERSION is read from the manifest so the SDK image's
# jm stays in lock-step with the repo pin.
DOCKER_TAG          ?= dev
DOCKER_VERSION      ?= $(shell grep -m1 '^version' pyproject.toml | cut -d'"' -f2)
JM_VERSION          ?= $(shell grep -m1 '^jm_version' just-makeit.toml | cut -d'"' -f2)
EXAMPLES_DOCKERFILE := deploy/docker/Dockerfile.examples
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

# ── Tooling ──────────────────────────────────────────────────────────────────
# The ONLY place a tool binary is named or given flags. Humans, the pre-commit
# hooks and CI all reach the tools through standard.mk's targets, so a flag
# change is a one-line edit here. Versions live in pyproject.toml's `dev` group
# and are pinned by uv.lock — which is what makes local and CI resolve
# identically, and why the hook config resolves no lock-managed tool itself.
UV         = uv
DEV_RUN    = $(UV) run --group dev
RUFF       = $(DEV_RUN) ruff
MDFORMAT   = $(DEV_RUN) mdformat
PRE_COMMIT = $(DEV_RUN) pre-commit
SYNC_CMD   = $(UV) sync

# ── lint-<tool> dispatch ─────────────────────────────────────────────────────
# LINT_TOOLS stamps out one `lint-<tool>` target each; .pre-commit-config.yaml
# calls `make -s lint-<tool>`, so a hook cannot run a tool differently from the
# way `make format` runs it. That is what closes the "same command, different
# environment" class of drift: the tool comes from `--group dev` and uv.lock
# owns the version, so there is no `additional_dependencies` left to drift.
LINT_TOOLS   = ruff ruff-format mdformat
FORMAT_TOOLS = ruff-format ruff mdformat

# ruff reads its own excludes from pyproject's [tool.ruff] extend-exclude
# (*.pyi, vendor, the re-export __init__.py shims), so the path list is just
# the tree.
RUFF_PATHS = .

# mdformat's own --exclude needs Python >=3.13 (it uses glob.translate), so the
# file list is built from `git ls-files` minus this regex instead — which also
# keeps the exclusions in ONE place rather than mirrored into the hook config.
# Generated, not hand-written: docs/c-api/ is mkdoxy output from the C headers;
# docs/benchmarks.md is `make bench-docs` output (mdformat would realign its
# tables on every regen -> spurious diffs); docs/**/archive/ are frozen
# snapshots (and one trips a plugin render-consistency bug across mdformat-gfm
# versions); examples/*/docs/ is the docs stub `jm new` scaffolds for a nested
# jm project, which `jm apply` recreates. vendor/ is not ours.
MD_EXCLUDE_RE = ^(vendor/|docs/c-api/|docs/benchmarks\.md$$|docs/.*/archive/|examples/[^/]+/docs/)

LINT_ruff        = $(RUFF) check --fix --unsafe-fixes $(RUFF_PATHS)
LINT_ruff-format = $(RUFF) format $(RUFF_PATHS)

# mdformat needs Python >=3.10 (see pyproject's dev group, where it carries a
# marker). On a 3.9 dev env it is simply absent, so skip with a notice rather
# than failing — the CI lint job runs a modern Python and enforces it there.
define LINT_mdformat
@if $(MDFORMAT) --version > /dev/null 2>&1; then \
    git ls-files '*.md' \
        | grep -Ev '$(MD_EXCLUDE_RE)' \
        | xargs -r $(MDFORMAT); \
else \
    echo "mdformat unavailable (needs Python >=3.10) — skipping"; \
fi
endef

# ── Test ─────────────────────────────────────────────────────────────────────
# `test` is CTest (native/ unit tests); the Python suite is `test-python`.
TEST_CMD      = $(CTEST) --test-dir $(BUILD_DIR) --output-on-failure
TEST_FAST_CMD = $(CTEST) --test-dir $(BUILD_DIR) --output-on-failure \
                    --stop-on-failure
# The SELECTION must match CI's exactly, or `make test-python` means one thing
# locally and another in CI — which it did: local ran everything under src/,
# CI excluded the `docs_snippets` and `examples` markers. Those two markers have
# their own targets (`test-snippets`, `test-examples-python`), so excluding them
# here is what makes the suites add up instead of overlapping. PYTEST_ARGS is
# how CI adds its coverage reporting without changing what runs.
#
# The suite is embarrassingly parallel across module directories, and
# pytest-xdist is in the dev group: `make test PYTEST_ARGS="-n auto"` runs it
# on every core. Measured on 8 cores: 278s -> 81s (3.4x), identical results
# (2554 passed, 6 skipped both ways).
#
# NOT the default, because it is not a free win: pytest-benchmark disables
# itself under xdist ("Benchmarks cannot be performed reliably in a
# parallelized environment"), so `-n auto` silently turns the benchmark tests
# under src/doppler/*/benchmarks/ into correctness-only runs. Real benchmark
# numbers come from `make bench-interleaved` regardless — but a gate must not
# quietly measure less than it appears to, so the flag stays opt-in.
#
# Do NOT parallelize `test-stubs`: measured 2.35s -> 2.24s (startup dominates),
# and a text-mode .pyi shares ONE doctest namespace across the whole file, so
# any future finer-than-file split would break name bindings that earlier
# examples in the same file establish.
PYTEST_ARGS     ?=
TEST_PYTHON_CMD = uv run pytest src/ -v \
                      -m "not docs_snippets and not examples" $(PYTEST_ARGS)
TEST_RUST_CMD   = cargo test --manifest-path $(RUST_DIR)/Cargo.toml

# Fail-closed: every src/doppler/examples/*.py (plus the standalone example) is
# discovered and run by the pytest gate -- no hand list to rot. Skips live in
# src/doppler/examples/.examples-skip (reasons mandatory).
TEST_EXAMPLES_CMD = $(MAKE) --no-print-directory test-examples-c

TEST_ALL_DEPS = test test-examples test-python test-examples-python

# `gates` must run every gate CI does — enforced by `gates-check` (standard.mk),
# which scans ci.yml and fails on any `make <target>` CI runs that `gates`
# cannot reach. Every such target is here or in GATES_PROVISION; nothing is
# silently omitted. GATES_PROVISION is the setup/build steps a dev runs BEFORE
# gating (install the deps, build the tree), not gates themselves.
GATES_PROVISION = install-deps install-docs-deps build pyext
GATES_DEPS    = lint changelog-check drift-check doxygen-check docs-check \
                test-all test-stubs test-api-docs test-snippets test-rust \
                abi-check link-check consumer-faces-check glibc-check \
                specan-check check-isotime-parity coverage coverage-gate \
                docker-examples

# ── Build ────────────────────────────────────────────────────────────────────
CMAKE_FLAGS = -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              -DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) $(CMAKE_ARGS)

# `CMAKE_FLAGS` reaches the CONFIGURE step only, and the standard's `build`
# ends with a bare `cmake --build $(BUILD_DIR)` — so there is no flag surface
# for `--parallel`, and doppler's build would drop from $(NPROC) jobs to one.
# CMake's own environment knob covers it, and covers it better: it applies to
# every `cmake --build` in the tree, including `pyext` and both downstream
# example builds, instead of only the one recipe a flag would reach.
export CMAKE_BUILD_PARALLEL_LEVEL = $(NPROC)

# Re-configures with BUILD_PYTHON=ON (default is OFF for C-only builds), then
# syncs so the venv sees the freshly built extensions.
#
# UV_SYNC_FLAGS can be overridden by dependents (e.g. just-build passes
# --no-group docs so the wheel build path never downloads the docs toolchain).
UV_SYNC_FLAGS ?=
define PYEXT_CMD
$(CMAKE) -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
    $(CMAKE_FLAGS) -DBUILD_PYTHON=ON
$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)
uv sync $(UV_SYNC_FLAGS)
endef

# `wheel` used to be a .PHONY with no rule: advertised in help, exiting 0 with
# no wheel produced. This is what it should always have been.
WHEEL_CMD = $(UV) build --wheel

# ── Docs ─────────────────────────────────────────────────────────────────────
# zensical reads mkdocs.yml natively. The docs toolchain lives in the `docs`
# dependency group, which is NOT auto-synced — hence `--group docs` on every
# invocation, so a clean checkout renders identically to CI. A stale local
# zensical.toml (gitignored, absent in CI) shadows mkdocs.yml and silently
# truncates the nav, so it is removed before every build.
ZENSICAL     = $(UV) run --group docs zensical
DOCS_PREPARE = @rm -f zensical.toml

# The docs gate: every check runs, every failure is reported in one pass. The
# ordering is the point — cheap script gates first, so a drifted generated file
# is reported without paying for a site build; then the strict build; then
# check_site_links, which walks the freshly built site/ and cannot run before
# it exists.
#
# These were one hand-rolled accumulator loop, with DOCS_CHECK_BUILD_CMD
# neutered to `:` so the recipe could own the build's position. The standard
# now expresses the shape directly, so the build line goes back to being the
# standard's — one implementation again, which is what criterion 5 asked for.
define DOCS_CHECK_PRE_CMDS
uv run python scripts/check_api_docs.py
uv run python scripts/check_docstring_coverage.py --check
uv run python scripts/check_nav_index.py
uv run python scripts/gen_related_pages.py --check
uv run python scripts/gen_readme.py --check
uv run python scripts/gen_install_scripts.py --check
uv run python scripts/gen_doc_versions.py --check
uv run python scripts/check_version_strings.py
uv run python scripts/check_doc_targets.py
uv run python scripts/check_serializable.py
uv run python scripts/check_doc_face_parity.py
uv run python scripts/check_init_param_optionality.py
endef

define DOCS_CHECK_POST_CMDS
uv run python scripts/check_site_links.py
endef

# ── Doxygen ──────────────────────────────────────────────────────────────────
# Version matters more than it looks: doxygen releases disagree about what
# counts as a warning, so checking with a different version than CI's gives a
# different answer. Runs natively when the local doxygen matches CI's, and
# otherwise inside DOXYGEN_IMAGE, which pins it.
DOXYGEN_VERSION ?= 1.9.8
DOXYGEN_IMAGE   ?= ubuntu:24.04
DOXYGEN_CMD      = doxygen Doxyfile

# The same pin, applied to GENERATION rather than checking (gen-c-api).
#
# doxygen versions disagree about output, not just about warnings, so
# regenerating docs/c-api with a local doxygen writes a diff CI would never
# produce. Measured: local 1.15.0 against CI's 1.9.8 turned a nine-line content
# change into 164 modified / 88 added / 6 deleted files, with nothing to
# distinguish the real change from the version churn.
#
# Only the doxygen BINARY needs pinning — mkdoxy and mkdocs come from uv.lock,
# so they are already identical here and in CI. That is why this shims one
# executable onto PATH instead of re-entering the whole target in a container:
# no uv, no network sync, no root-owned output tree.
#
# The image is built once and cached (a 3-line Dockerfile over DOXYGEN_IMAGE);
# its doxygen is verified to BE the pinned version at build time, so an
# upstream apt bump fails loudly here rather than silently changing output.
# The mount uses the same absolute path inside and out, so any absolute path
# mkdoxy hands to doxygen resolves identically on both sides. `docker run -i`
# is load-bearing, not hygiene: mkdoxy invokes `doxygen -` and pipes the whole
# generated config on STDIN (mkdoxy/doxyrun.py `run`), so without it doxygen
# reads an empty config, emits no XML, and the failure surfaces later as a
# missing index.xml — after the recipe has already wiped docs/c-api.
DOXYGEN_PIN_IMAGE ?= doppler-doxygen:$(DOXYGEN_VERSION)

define DOXYGEN_CHECK_CMD
@have=$$(doxygen --version 2>/dev/null | cut -d' ' -f1); \
if [ "$$have" = "$(DOXYGEN_VERSION)" ]; then \
  $(MAKE) -s doxygen-warn-gate; \
else \
  echo "doxygen-check: local $${have:-none} != CI's $(DOXYGEN_VERSION), using $(DOXYGEN_IMAGE)"; \
  docker run --rm -v "$(CURDIR)":/w:ro -w /w $(DOXYGEN_IMAGE) sh -c \
    'apt-get update -qq >/dev/null 2>&1 && \
     apt-get install -y -qq --no-install-recommends doxygen make >/dev/null 2>&1 && \
     make -s doxygen-warn-gate'; \
fi
endef

# ── Bench ────────────────────────────────────────────────────────────────────
BENCH_THRESHOLD ?= 0.30

# Benchmarks reported by the advisory perf gate but never failed by it
# (jm bench --check --allow). Keep this list SHORT and justified — an
# exemption is a blind spot, so each entry names why the measurement, not the
# code, is untrustworthy.
#
# test_bench_execute_decim_64k: bimodal on GitHub runners. It lands in one of
#   two internally-tight states ~1.66x apart (min 1.10 ms vs 1.84 ms, each
#   with ~1.2% stddev) that track per-process memory layout rather than any
#   code change. It has flagged PRs containing no DSP code at all (#519
#   docs-only +59.7%, #524 tooling-only +66.5%). Not reproducible locally:
#   448 us +/- 0.2% across 8 separate processes. A gate that fires on
#   documentation changes stops being read.
BENCH_ALLOW ?= test_bench_execute_decim_64k

BENCH_CMD         = uv run just-makeit bench
BENCH_SAVE_CMD    = uv run just-makeit bench --python-only --tag base
BENCH_COMPARE_CMD = uv run just-makeit bench --check \
                        --python-only --baseline base \
                        --threshold $(BENCH_THRESHOLD) \
                        $(foreach b,$(BENCH_ALLOW),--allow $(b))

# ── Coverage ─────────────────────────────────────────────────────────────────
# `coverage` builds a dedicated clang-instrumented tree and merges the C,
# Python and Rust .profraw into one llvm-cov report; `coverage-gate` fails when
# the C lines a PR adds aren't covered. clang + matching llvm tools required;
# override the tool names if they are version-suffixed (e.g. llvm-cov-22).
COV_DIR       ?= build-cov
LLVM_PROFDATA ?= llvm-profdata
LLVM_COV      ?= llvm-cov
COV_BASE      ?= origin/main
COV_PATCH_MIN ?= 90
# Excluded from the report: vendored code, jm-generated binding aggregators
# (`<mod>_ext.c`) and per-object fragments, and the test/bench harnesses — only
# first-party _core.c counts. `native/src/app/` (the wfmgen CLI) is excluded
# too: its body is an OBJECT lib compiled into BOTH the executable and a `.so`,
# but the report attributes only to the `.so`, whose copy is never executed.
COV_IGNORE    ?= (^|/)(vendor|build|build-cov|native/src/app)/|_ext(_[a-z0-9_]+)?\.c$$|/(tests|benchmarks)/

define COVERAGE_CMD
$(CMAKE) -B $(COV_DIR) -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DDOPPLER_COVERAGE=ON -DBUILD_PYTHON=ON \
    -DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) \
    -DPYTHON_PACKAGE_DIR=$(CURDIR)/$(COV_DIR)/pkg/doppler \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    $(CMAKE_ARGS)
$(CMAKE) --build $(COV_DIR) --parallel $(NPROC)
mkdir -p $(COV_DIR)/pkg/doppler
(cd src/doppler && tar cf - --exclude='*.so' .) \
    | (cd $(COV_DIR)/pkg/doppler && tar xf -)
rm -rf $(COV_DIR)/prof && mkdir -p $(COV_DIR)/prof
cd $(COV_DIR) && LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/c-%p-%m.profraw" \
    $(CTEST) --output-on-failure
-LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/py-%p-%m.profraw" \
    PYTHONPATH="$(CURDIR)/$(COV_DIR)/pkg" \
    $(PYTHON_EXECUTABLE) -m pytest $(COV_DIR)/pkg/doppler \
    -q -p no:cacheprovider --ignore-glob='*/benchmarks/*'
-DOPPLER_BUILD_DIR="$(CURDIR)/$(COV_DIR)" \
    LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/rs-%p-%m.profraw" \
    cargo test --manifest-path $(RUST_DIR)/Cargo.toml
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
sed -i 's|SF:$(CURDIR)/|SF:|' $(COV_DIR)/coverage.lcov
@echo "coverage: HTML -> $(COV_DIR)/html/index.html  lcov -> $(COV_DIR)/coverage.lcov"
endef

define COVERAGE_GATE_CMD
uvx diff-cover $(COV_DIR)/coverage.lcov \
    --compare-branch=$(COV_BASE) \
    --fail-under=$(COV_PATCH_MIN) \
    --format html:$(COV_DIR)/patch.html \
    --format markdown:$(COV_DIR)/patch.md
endef

# ── Release ──────────────────────────────────────────────────────────────────
# Five places carry the version; `version-check` probes three of them (uv.lock's
# copy is re-synced by `uv lock` in the bump, and CHANGELOG is prose).
define VERSION_PROBES
pyproject.toml|grep '^version' pyproject.toml | head -1 | sed 's/version = "\(.*\)"/\1/'
CMakeLists.txt|grep '^project(doppler VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/'
Cargo.toml|grep '^version' $(RUST_DIR)/Cargo.toml | head -1 | sed 's/version = "\(.*\)"/\1/'
endef

# CMake and Cargo reject a Python pre-release suffix (1.2.3rc1), so those two
# get the numeric prefix while pyproject keeps the full string.
define BUMP_VERSION_CMD
sed -i 's/^version = "[^"]*"/version = "$(VERSION)"/' pyproject.toml
sed -i "s/^version = \"[0-9.]*/version = \"$$(echo $(VERSION) | sed 's/[^0-9.].*//g')/" $(RUST_DIR)/Cargo.toml
sed -i "s/^project(doppler VERSION [0-9.]*/project(doppler VERSION $$(echo $(VERSION) | sed 's/[^0-9.].*//g')/" CMakeLists.txt
uv lock
@$(MAKE) --no-print-directory docs-relink
endef

define RELEASE_BRANCH_NOTES
@echo "  - if perf-relevant code changed since the last release (release.md"
@echo "    §2b): make bench-interleaved VERSION=$(VERSION) && make bench-docs"
@echo "    (on a representative machine), then commit benchmarks/published +"
@echo "    docs/benchmarks.md"
endef

RELEASE_WATCH_CMD = @REPO=doppler-dsp/doppler scripts/release-watch.sh \
                        "$(VERSION)"

# ── Clean ────────────────────────────────────────────────────────────────────
CLEAN_PATHS = $(BUILD_DIR) $(PY_BUILD_DIR) docs/doxygen/ site/ \
              *.png bench_*.json zensical.toml __pycache__

define CLEAN_CMD
@find $(PYEXT_DIR) -name '*.cpython-*.so' -delete
@find $(PYEXT_DIR) -name '__pycache__' -type d -exec rm -rf {} + 2>/dev/null || true
endef

# ── Repo-local targets ───────────────────────────────────────────────────────
# Named here so they land in `.PHONY` and in generated `help`, and so the same
# gates cover them: criterion 2 is "help lists EVERY target", not "every
# standard target" — a local target help omits is exactly as invisible.
LOCAL_TARGETS = specan record-demo gallery blazing gen-c-api just-build \
                gen-c-api-run doxygen-pin-image \
                package-c sdist \
                docs-relink drift-check changelog-check doxygen-warn-gate \
                test-examples-c test-examples-python test-example-downstream \
                test-example-downstream-python \
                test-stubs test-api-docs test-snippets lint-stubs \
                check-docstring-coverage \
                abi-check link-check consumer-faces-check \
                glibc-check specan-check check-isotime-parity \
                install-docs-deps \
                wheel-check wheel-smoke release-smoke \
                bench-interleaved bench-publish bench-docs bench-stream \
                bench-report \
                docker-runtime docker-sdk docker-downstream docker-stream \
                docker-examples

include standard.mk

# ── Everything below is genuinely doppler's own ──────────────────────────────

# The docs system deps (doxygen, graphviz, LaTeX) — the `docs` group in
# jb.toml. standard.mk's `install-deps` installs only the default groups
# (runtime, dev) and is vendored, so it cannot take a group; this local target
# is the one place CI's doxygen job and the Pages docs workflow both reach for
# the docs group, keeping jb.toml the single dependency list. Bootstraps jbx
# the same way standard.mk's install-deps does.
install-docs-deps: ## Install the docs system deps (jb.toml `docs` group)
	@command -v jbx >/dev/null 2>&1 \
	    || curl -sSL https://just-buildit.github.io/get-jb.sh | bash
	PATH="$$HOME/.local/bin:$$PATH" jbx install-deps -g docs

specan: ## Launch the live spectrum analyzer in a browser
	uv run doppler-specan

record-demo: ## Re-record the specan demo frames (docs/specan/frames.json)
	uv run python -m doppler.specan.record_demo \
	    --frames 120 --fft-size 512 \
	    -o docs/specan/frames.json

# Run all plot-generating examples and copy output PNGs to docs/assets/.
# Run before releasing whenever src/doppler/examples/ has changed.
GALLERY_SCRIPTS := \
    src/doppler/examples/agc_demo.py \
    src/doppler/examples/ber_awgn_demo.py \
    src/doppler/examples/cic_demo.py \
    src/doppler/examples/corr_demo.py \
    src/doppler/examples/detection_curves.py \
    src/doppler/examples/detection_sim.py \
    src/doppler/examples/detection2d_demo.py \
    src/doppler/examples/lockdet_demo.py \
    src/doppler/examples/telemetry_fanin_demo.py \
    src/doppler/examples/mpsk_telemetry_capture_demo.py \
    src/doppler/examples/rate_converter_demo.py \
    src/doppler/examples/ratesync_demo.py \
    src/doppler/examples/ddc_fn_demo.py \
    src/doppler/examples/ddc_fn_scaling.py \
    src/doppler/examples/adc_demo.py \
    src/doppler/examples/hbdecim_q15_demo.py \
    src/doppler/examples/wfmgen_demo.py \
    src/doppler/examples/symbols_demo.py \
    src/doppler/examples/wfm_composition_demo.py \
    src/doppler/examples/wcdma_carriers_demo.py \
    src/doppler/examples/plan_demo.py \
    src/doppler/examples/crowded_band_demo.py \
    src/doppler/examples/measure_demo.py \
    src/doppler/examples/measure_imd_npr_demo.py \
    src/doppler/examples/wfm_write_demo.py \
    src/doppler/examples/awgn_demo.py \
    src/doppler/examples/doppler_channel_demo.py \
    src/doppler/examples/wfm_io_demo.py \
    src/doppler/examples/dsss_burst_pipeline_demo.py \
    src/doppler/examples/async_dsss_receiver_spec_demo.py \
    src/doppler/examples/dsss_receiver_demo.py \
    src/doppler/examples/carrier_acq_rrc_demo.py \
    src/doppler/examples/mpsk_receiver_demo.py \
    src/doppler/examples/mpsk_receiver_performance_demo.py

gallery: ## Run the plot examples and copy their PNGs to docs/assets/
	@echo "Regenerating gallery plots..."
	@for script in $(GALLERY_SCRIPTS); do \
	    printf "  %-45s" "$$script"; \
	    uv run python $$script > /dev/null 2>&1 && echo "OK" || { echo "FAIL"; exit 1; }; \
	done
	@mv -f agc_convergence.png ber_awgn_demo.png cic_demo_spectrum.png corr_demo.png detection_curves.png detection_sim.png detection2d_demo.png lockdet_demo.png telemetry_fanin_demo.png mpsk_telemetry_capture_demo.png rate_converter_demo.png ratesync_demo.png ddc_fn_demo.png ddc_fn_scaling.png adc_demo.png hbdecim_q15_demo.png wfmgen_demo.png symbols_demo.png wfm_composition_demo.png wcdma_carriers_demo.png plan_demo.png plan_background_demo.png crowded_band_demo.png measure_demo.png measure_imd_npr_demo.png wfm_write_demo.png doppler_channel_demo.png wfm_io_demo.png dsss_burst_pipeline_demo.png async_dsss_receiver_spec_demo.png dsss_receiver_demo.png carrier_acq_rrc_demo.png mpsk_receiver_demo.png mpsk_receiver_performance_demo.png docs/assets/
	@rm -f burst.blue
	@echo "Gallery plots written to docs/assets/."

blazing: ## Clean + Release + -march=native (max speed; never packaged)
	@$(MAKE) --no-print-directory clean
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DDOPPLER_NATIVE=ON \
		"-DCMAKE_C_FLAGS=$(BLAZING_CFLAGS)" \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)

doxygen-pin-image: ## Build/cache the pinned-doxygen image gen-c-api shims in
	@docker image inspect $(DOXYGEN_PIN_IMAGE) >/dev/null 2>&1 && exit 0; \
	 echo "doxygen-pin-image: building $(DOXYGEN_PIN_IMAGE) from $(DOXYGEN_IMAGE)"; \
	 printf 'FROM %s\nRUN apt-get update -qq \
	   && apt-get install -y -qq --no-install-recommends doxygen graphviz \
	   && rm -rf /var/lib/apt/lists/*\n' '$(DOXYGEN_IMAGE)' \
	   | docker build -q -t $(DOXYGEN_PIN_IMAGE) - >/dev/null; \
	 got=$$(docker run --rm $(DOXYGEN_PIN_IMAGE) doxygen --version | cut -d' ' -f1); \
	 if [ "$$got" != "$(DOXYGEN_VERSION)" ]; then \
	   echo "doxygen-pin-image: $(DOXYGEN_IMAGE) now ships doxygen $$got, not $(DOXYGEN_VERSION)."; \
	   echo "  CI installs doxygen from the same base, so CI has moved too — update"; \
	   echo "  DOXYGEN_VERSION (and re-check the c-api diff) rather than pinning around it."; \
	   docker image rm -f $(DOXYGEN_PIN_IMAGE) >/dev/null 2>&1; exit 1; \
	 fi; \
	 echo "doxygen-pin-image: $(DOXYGEN_PIN_IMAGE) ready (doxygen $$got)"

gen-c-api: ## Regenerate docs/c-api/ from the headers (mkdoxy, CI's doxygen)
	@have=$$(doxygen --version 2>/dev/null | cut -d' ' -f1); \
	 if [ "$$have" = "$(DOXYGEN_VERSION)" ]; then \
	   echo "gen-c-api: local doxygen $$have matches CI's — running natively"; \
	   $(MAKE) -s gen-c-api-run; \
	 else \
	   echo "gen-c-api: local $${have:-none} != CI's $(DOXYGEN_VERSION) — shimming $(DOXYGEN_PIN_IMAGE)"; \
	   $(MAKE) -s doxygen-pin-image; \
	   shim=$$(mktemp -d); \
	   printf '#!/bin/sh\nexec docker run --rm -i -u %s:%s -v "%s":"%s" -w "$$PWD" %s doxygen "$$@"\n' \
	     "$$(id -u)" "$$(id -g)" "$(CURDIR)" "$(CURDIR)" "$(DOXYGEN_PIN_IMAGE)" > $$shim/doxygen; \
	   chmod +x $$shim/doxygen; \
	   PATH="$$shim:$$PATH" $(MAKE) -s gen-c-api-run; rc=$$?; \
	   rm -rf $$shim; exit $$rc; \
	 fi

gen-c-api-run: ## gen-c-api proper (re-entered with CI's doxygen on PATH)
	rm -rf docs/c-api .mkdoxy .capi-site
	uv run --group docs mkdocs build -f mkdocs-capi.yml
	cp -r .mkdoxy/doppler/c-api docs/c-api
	# index.md is a hand-written landing page mkdoxy doesn't emit — restore it
	# after the regen wipes it (matches the CI docs.yml step).
	git checkout -- docs/c-api/index.md
	# `latex/` too: the repo's Doxyfile sets GENERATE_LATEX = NO, but mkdoxy
	# builds its OWN config dict and does not, so every run drops an untracked
	# latex/ in the repo root. Cleaned here rather than gitignored — it is
	# output of this target, and nothing else in the tree wants that name.
	rm -rf .mkdoxy .capi-site latex

# PEP 517 build hook for just-buildit. It sets JUST_BUILDIT_OUTPUT_DIR and
# JUST_BUILDIT_PYTHON before calling this target; the package tree is copied
# there to be packaged. The docs group is excluded so a docs-dep network blip
# cannot fail a wheel build.
just-build: UV_SYNC_FLAGS = --no-group docs
just-build: pyext ## PEP 517 build hook for just-buildit
	mkdir -p $(JUST_BUILDIT_OUTPUT_DIR)
	cp -r $(PYEXT_DIR) $(JUST_BUILDIT_OUTPUT_DIR)/doppler

# The relocatable C-library package: Release, no Python extension, headers +
# static/shared libs + the cmake/pkg-config config, installed to PREFIX. This is
# the ONE definition of the C tarball build — release.yml's three
# Package-C-library jobs (linux x86_64/aarch64, macOS) each call it, so the
# install layout lives in one place. LIBDIR=lib (not the RHEL/manylinux lib64
# default) so a consumer's find_package/CMAKE_PREFIX_PATH resolves the config on
# every distro (CMake searches lib/ universally; lib64/ only where the platform
# opts in). Tarball naming stays in the caller — it is per-platform and trivial.
package-c: ## PREFIX=<dir> — build+install the relocatable C library (no Python)
ifndef PREFIX
	@echo "usage: make package-c PREFIX=<dir>"; exit 1
endif
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release \
	    -DBUILD_PYTHON=OFF -DCMAKE_INSTALL_LIBDIR=lib $(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)
	$(CMAKE) --install $(BUILD_DIR) --prefix $(PREFIX)

# Source distribution, sibling of standard.mk's `wheel` (= uv build --wheel).
# release.yml's build-sdist job calls this instead of hand-rolling a
# uv-venv/pip/`python -m build --sdist` bootstrap.
sdist: ## Build a source distribution into dist/
	$(UV) build --sdist
	@echo ""
	@ls -lh dist/*.tar.gz

# Regenerates every generated doc region: the "## Related pages" blocks on
# docs/api/*.md, README.md's synced body from docs/index.md, the per-distro
# install scripts from jb.toml, and the release version stamped into
# doc-version regions.
docs-relink: ## Regenerate every generated doc region
	uv run python scripts/gen_related_pages.py --write
	uv run python scripts/gen_readme.py --write
	uv run python scripts/gen_install_scripts.py --write
	uv run python scripts/gen_doc_versions.py --write

# The jm manifest drift gate. --no-install-project because the gate only reads
# the manifest, so there is no reason to build the C extension for it.
drift-check: ## jm manifest drift gate (CI's 'jm manifest drift')
	uv sync --group dev --no-install-project
	uv run just-makeit status --check
	@echo "── downstream jm example ──"
# The example is a just-makeit project in its own right, so its generated
# bindings/stubs/CMake can drift from its manifest exactly like doppler's can.
# Checked with the SAME pinned jm, so the example cannot silently document a jm
# version doppler is not on.
	cd $(DOWNSTREAM_DIR) && uv run --project $(CURDIR) just-makeit status --check

# Fails when code has shipped since the last tag but [Unreleased] is empty.
# keep-a-changelog says the entry lands with the change, not at release time.
# The RELEASE PR is the one legitimate empty state: promotion moves every entry
# into `## [X.Y.Z]`, so the notes exist, just not under [Unreleased]. That
# exemption is narrow on purpose — the version must HAVE a section AND its tag
# must NOT exist yet, which is what stops it becoming permanent.
changelog-check: ## [Unreleased] must not be empty if code shipped
	@t=$$(git describe --tags --abbrev=0 2>/dev/null || true); \
	n=$$(git log --oneline $${t:+$$t..}HEAD -- src native objects ffi 2>/dev/null | wc -l); \
	e=$$(awk '/^## \[Unreleased\]/{f=1;next} f&&/^## /{exit} f' CHANGELOG.md \
	     | grep -c '^- ' || true); \
	v=$$(awk -F'"' '/^version = /{print $$2; exit}' pyproject.toml); \
	if [ "$$n" -gt 0 ] && [ "$$e" -eq 0 ] \
	   && grep -q "^## \[$$v\]" CHANGELOG.md \
	   && ! git rev-parse -q --verify "refs/tags/v$$v" >/dev/null 2>&1; then \
	  echo "changelog-check: release PR for $$v — notes are promoted into [$$v], v$$v not yet tagged — OK"; \
	  exit 0; \
	fi; \
	if [ "$$n" -gt 0 ] && [ "$$e" -eq 0 ]; then \
	  echo "changelog-check: $$n code commit(s) since $${t:-repo start}, [Unreleased] is empty — FAIL"; \
	  exit 1; \
	fi; \
	echo "changelog-check: $$e entry/entries for $$n code commit(s) since $${t:-repo start}"

# The doxygen gate proper. Split out so doxygen-check's Docker branch can
# re-enter it with the pinned doxygen on PATH. Writes NOTHING into the tree:
# the container mounts the repo read-only, so the generated XML/HTML and the
# log go to a scratch dir. A gate that mutates the tree it checks is a bug.
doxygen-warn-gate: ## The doxygen warning gate proper (re-entered in Docker)
	@out=$$(mktemp -d); \
	{ cat Doxyfile; \
	  echo "OUTPUT_DIRECTORY=$$out"; \
	  echo "WARN_LOGFILE="; } | doxygen - > $$out/stdout.log 2>&1; \
	n=$$(grep -c 'warning:' $$out/stdout.log || true); \
	if [ "$$n" != "0" ]; then \
	  grep 'warning:' $$out/stdout.log; \
	  echo "doxygen-check: $$n warning(s) — FAIL"; rm -rf $$out; exit 1; \
	fi; \
	rm -rf $$out; \
	echo "doxygen-check: 0 warnings"

# ── Examples ─────────────────────────────────────────────────────────────────
# Smoke-test every standalone C example. Excluded (each needs a live NATS peer
# or terminal interaction, not just a broker): transmitter, receiver,
# pipeline_demo, spectrum_analyzer.
EXAMPLE_BIN_DIR := $(BUILD_DIR)/examples/c
STANDALONE_BUILD_DIR := examples/standalone/build
DOWNSTREAM_DIR := examples/downstream-jm
# Build trees live under doppler's BUILD_DIR, not inside the example: the
# example is a jm project and `jm status` walks its tree, so a build dir there
# would be scanned on every drift check.
DOWNSTREAM_BUILD_DIR := $(BUILD_DIR)/downstream-jm

test-examples-c: build ## Smoke-test every standalone C example
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
	@$(MAKE) --no-print-directory test-example-downstream
	@echo "All C example smoke tests passed."

# The downstream just-makeit project built the way a real consumer builds it:
# `find_package(doppler)` against this build tree, linking libdoppler.a. Its
# own CTest suite writes captures with doppler's writer and reads them back
# through the façade, so it exercises the whole consume-doppler path.
#
# BUILD_PYTHON=OFF deliberately: this runs inside test-examples-c, which CI
# also runs in the debian:buster-slim glibc container where there is no Python
# and no NumPy. Keeping the C half Python-free means "can a downstream link
# libdoppler.a" gets answered on ubuntu, macOS AND glibc 2.28.
test-example-downstream: build ## Build the downstream jm example (C only)
	@echo "Building downstream jm example (C only, links libdoppler.a)..."
# Output is captured rather than discarded: a gate that hides why it failed
# costs a whole CI round-trip to diagnose, which this one did on its first
# macOS run. Quiet on success, the real compiler/linker error on failure.
	@cmake -B $(DOWNSTREAM_BUILD_DIR) $(DOWNSTREAM_DIR) \
	    -Ddoppler_DIR=$(abspath $(BUILD_DIR)) \
	    -DBUILD_PYTHON=OFF \
	    -DCMAKE_BUILD_TYPE=Release \
	    > $(DOWNSTREAM_BUILD_DIR).log 2>&1 \
	    || { echo "  configure FAILED"; cat $(DOWNSTREAM_BUILD_DIR).log; exit 1; }
	@cmake --build $(DOWNSTREAM_BUILD_DIR) --parallel $(NPROC) \
	    > $(DOWNSTREAM_BUILD_DIR).log 2>&1 \
	    || { echo "  build FAILED"; cat $(DOWNSTREAM_BUILD_DIR).log; exit 1; }
	@printf "  %-20s" "downstream-jm"; \
	if $(CTEST) --test-dir $(DOWNSTREAM_BUILD_DIR) > /dev/null 2>&1; then \
	    echo "PASS"; \
	else \
	    echo "FAIL"; \
	    $(CTEST) --test-dir $(DOWNSTREAM_BUILD_DIR) --output-on-failure; \
	    exit 1; \
	fi

# ── The release-path gates ───────────────────────────────────────────────────
# These guard what users actually download. They ran only inside release.yml,
# so they could not be rehearsed: the first time you learn a release gate is
# wrong is while a release is failing. Each is now a target, and each was run
# for real against v0.40.0 before being wired in.

WHEEL_DIR ?= dist

# A distributed wheel must run on any CPU of its arch. `-march=native` on the
# build host sprays AVX2/AVX-512 across every function and SIGILLs on older
# hardware -- doppler 0.5.1 shipped exactly that bug. Wide SIMD is legitimate
# ONLY inside runtime-dispatched functions, which carry the ISA in their name
# and sit behind a __builtin_cpu_supports() check.
#
# One target, not two: the x86 and aarch64 scans were separate CI steps with
# separate `if:` conditions, but they differ only in the register pattern and
# the allowed function-name substring. Detecting the arch here deletes the
# duplication and the condition with it.
wheel-check: ## Verify built wheels carry no -march/-mcpu=native SIMD leak
	@arch="$$(uname -m)"; \
	 case "$$arch" in \
	   x86_64)  pat='%zmm[0-9]|%ymm[0-9]';        ok='avx512'; isa='AVX2/AVX-512';; \
	   aarch64|arm64) pat='[ ,](z[0-9]+\.[bhsdq]|p[0-9]+/[mz])'; ok='sve'; isa='SVE';; \
	   *) echo "wheel-check: no SIMD-leak pattern for $$arch — skipping"; exit 0;; \
	 esac; \
	 ls $(WHEEL_DIR)/*.whl > /dev/null 2>&1 \
	   || { echo "wheel-check: no wheel in $(WHEEL_DIR)/ — run 'make wheel'"; exit 1; }; \
	 fail=0; tmp="$$(mktemp -d)"; \
	 for whl in $(WHEEL_DIR)/*.whl; do \
	     unzip -q -o "$$whl" -d "$$tmp/x"; \
	     find "$$tmp/x" \( -name '*.cpython-*.so' \
	           -o -path '*/wfm/_bin/wfmgen' \) > "$$tmp/solist"; \
	     while IFS= read -r so; do \
	         bad=$$(objdump -d "$$so" 2>/dev/null | awk -v pat="$$pat" -v ok="$$ok" ' \
	           /^[0-9a-f]+ <.*>:/ { fn=$$2; gsub(/[<>:]/, "", fn) } \
	           $$0 ~ pat { if (fn !~ ok) { print fn; exit } }'); \
	         if [ -n "$$bad" ]; then \
	             echo "  FAIL $$(basename $$whl): $$(basename $$so) has $$isa in"; \
	             echo "       non-dispatched function '$$bad' (built with -native?)"; \
	             fail=1; \
	         fi; \
	     done < "$$tmp/solist"; \
	     rm -rf "$$tmp/x"; \
	 done; \
	 rm -rf "$$tmp"; \
	 if [ "$$fail" -ne 0 ]; then \
	     echo "wheel-check: refusing a non-portable wheel — build without -DDOPPLER_NATIVE"; \
	     exit 1; \
	 fi; \
	 echo "wheel-check: portable — $$isa only in runtime-dispatched *$$ok* functions"

# Installs the built wheel into a THROWAWAY venv and runs the end-to-end from a
# clean cwd, so `import doppler` can only resolve to the wheel and never to
# ./src. CI installed into the job environment instead; a temp venv is what
# makes the same check safe to run on a dev machine.
#
# The venv's bin/ must be on PATH, not just its python: two of the e2e's ten
# checks shell out to the `wfmgen` CONSOLE SCRIPT, and calling
# venv/bin/python directly leaves that off PATH. Running the target before
# wiring it is what caught that -- it read as "the wheel is broken" (8/10)
# when it was the harness.
wheel-smoke: ## Install the built wheel in a temp venv and run the e2e
	@ls $(WHEEL_DIR)/*.whl > /dev/null 2>&1 \
	   || { echo "wheel-smoke: no wheel in $(WHEEL_DIR)/ — run 'make wheel'"; exit 1; }; \
	 tmp="$$(mktemp -d)"; \
	 $(UV) venv --quiet "$$tmp/venv"; \
	 VIRTUAL_ENV="$$tmp/venv" $(UV) pip install --quiet $(WHEEL_DIR)/*.whl; \
	 ( cd "$$tmp" && PATH="$$tmp/venv/bin:$$PATH" \
	     "$$tmp/venv/bin/python" $(CURDIR)/deploy/validation/wfm_e2e.py ) \
	   && "$$tmp/venv/bin/python" -c 'import doppler; print("doppler", doppler.__version__)' \
	   && { rm -rf "$$tmp"; echo "wheel-smoke: OK"; } \
	   || { rm -rf "$$tmp"; echo "wheel-smoke: FAILED"; exit 1; }

# The published C tarball, fetched and exercised the way a consumer would.
# Runnable against any released version, which is what makes it rehearsable:
# `make release-smoke VERSION=0.40.0` after a release, or before touching the
# script.
release-smoke: ## Smoke-test a PUBLISHED C tarball (VERSION=x.y.z)
ifndef VERSION
	@echo "usage: make release-smoke VERSION=<x.y.z>"; exit 1
endif
	bash tests/install/release-smoke.sh "$(VERSION)"

# ── The binary-hygiene gates ─────────────────────────────────────────────────
# These inspect what the build actually produced, and each one exists because
# the property it checks is invisible in source and expensive to discover late:
# a wheel that SIGILLs on a user's CPU, a C library that drags in libstdc++, an
# archive a downstream cannot link. They were inline CI steps, so none could be
# run before pushing.

# Linux-only: both inspect ELF with objdump/nm/ldd. The link smoke below is
# the cross-platform one.
abi-check: ## Verify the built libraries are portable and C++-free (Linux)
	@fail=0; \
	 echo "=== portable SIMD (no leaked AVX2/AVX-512) ==="; \
	 bad=$$(objdump -d $(BUILD_DIR)/libdoppler.so 2>/dev/null | awk ' \
	   /^[0-9a-f]+ <.*>:/ { fn=$$2; gsub(/[<>:]/, "", fn) } \
	   /%zmm[0-9]|%ymm[0-9]/ { if (fn !~ /avx512/) { print fn; exit } }'); \
	 if [ -n "$$bad" ]; then \
	     echo "  FAIL: wide SIMD in non-dispatched function '$$bad' —"; \
	     echo "        a wide-SIMD flag leaked into the default build?"; \
	     fail=1; \
	 else echo "  OK — wide SIMD only in runtime-dispatched *avx512* functions."; fi; \
	 echo "=== C++-free (no libstdc++ / CXXABI anywhere) ==="; \
	 pat='GLIBCXX_|CXXABI_|std::|operator new|operator delete'; \
	 bad=$$(nm -CD $(BUILD_DIR)/libdoppler.so 2>/dev/null | grep -E "$$pat" || true); \
	 bad="$$bad$$(nm -C $(BUILD_DIR)/libdoppler.a 2>/dev/null | grep -E "$$pat" || true)"; \
	 bad="$$bad$$(nm -CD $(BUILD_DIR)/libdoppler_stream.so 2>/dev/null | grep -E "$$pat" || true)"; \
	 bad="$$bad$$(nm -C $(BUILD_DIR)/libdoppler_stream.a 2>/dev/null | grep -E "$$pat" || true)"; \
	 if [ -n "$$bad" ]; then \
	     echo "  FAIL: doppler references the C++ runtime — the build must be pure C:"; \
	     echo "$$bad" | head -20; fail=1; \
	 elif ldd $(BUILD_DIR)/libdoppler.so 2>/dev/null | grep -qi 'libstdc++' \
	   || ldd $(BUILD_DIR)/libdoppler_stream.so 2>/dev/null | grep -qi 'libstdc++'; then \
	     echo "  FAIL: a dynamic libstdc++ dependency"; fail=1; \
	 else echo "  OK — C++-free everywhere; links -lm only."; fi; \
	 if [ "$$fail" = 0 ]; then echo "abi-check: ALL PASS"; \
	 else echo "abi-check: FAILURES above"; exit 1; fi

# Cross-platform, unlike abi-check: this is the question a downstream actually
# asks — does libdoppler.a link with nothing but -lm? — and it is answered on
# Linux and macOS both.
link-check: ## Smoke-test that a downstream links libdoppler.a with only -lm
	@t=$$(mktemp -d); \
	 if cc examples/consumer/main.c -Inative/inc -I$(BUILD_DIR)/native/inc \
	       $(BUILD_DIR)/libdoppler.a -lm -o "$$t/consumer_smoke" \
	    && ( cd "$$t" && ./consumer_smoke > /dev/null ); then \
	     echo "link-check: OK — libdoppler.a links with only -lm."; \
	     rm -rf "$$t"; \
	 else \
	     echo "link-check: FAIL — a downstream cannot link libdoppler.a with -lm"; \
	     rm -rf "$$t"; exit 1; \
	 fi

# The three documented consumer faces (bare cc / CMake find_package /
# pkg-config), built against a fresh install of THIS build tree and asserted
# to produce identical output. build-three-ways.sh is the SSOT — the docs
# --8<-- include its command regions, and release-smoke.sh runs the same
# script against the PUBLISHED tarball. This target gives CI's per-commit path
# and a local dev the exact same check without waiting for a release.
consumer-faces-check: build ## Build a consumer via cc/CMake/pkg-config, assert identical output
	@t=$$(mktemp -d); \
	 $(CMAKE) --install $(BUILD_DIR) --prefix "$$t/pfx" > /dev/null \
	 && bash tests/install/stream-consumer/build-three-ways.sh "$$t/pfx"; \
	 rc=$$?; rm -rf "$$t"; exit $$rc

# The oldest glibc a released .so may reference. Only meaningful against a
# build made on that glibc — CI runs this in a Debian 10 container, and running
# it on a modern distro will fail on the local build's newer symbols, which is
# the check working, not a bug.
GLIBC_MAX ?= 2.28
glibc-check: ## Verify no glibc symbol newer than $(GLIBC_MAX) (needs an old-glibc build)
	@BAD=$$(objdump -T $(BUILD_DIR)/libdoppler.so \
	        | grep -oP 'GLIBC_\K[0-9.]+' | sort -Vu \
	        | awk -F. -v mx="$(GLIBC_MAX)" \
	            'BEGIN{split(mx,m,".")} $$1 > m[1] || ($$1 == m[1] && $$2 > m[2])'); \
	 if [ -n "$$BAD" ]; then \
	     echo "glibc-check: libdoppler.so references glibc > $(GLIBC_MAX): $$BAD"; \
	     exit 1; \
	 fi; \
	 echo "glibc-check: all glibc symbols <= $(GLIBC_MAX)"

# The recorded specan demo frames are a projection of the specan source, so a
# change to one without the other ships a demo that no longer matches the code.
SPECAN_BASE ?= HEAD^
specan-check: ## Fail if specan changed without re-recording its demo frames
	@s=$$(git diff --name-only "$(SPECAN_BASE)"...HEAD -- src/specan/doppler_specan/ | wc -l); \
	 f=$$(git diff --name-only "$(SPECAN_BASE)"...HEAD -- docs/specan/frames.json | wc -l); \
	 if [ "$$s" -gt 0 ] && [ "$$f" -eq 0 ]; then \
	     echo "specan-check: src/specan/doppler_specan/ changed but"; \
	     echo "  docs/specan/frames.json was not — run 'make record-demo'"; \
	     exit 1; \
	 fi; \
	 echo "specan-check: OK (specan=$$s frames=$$f)"

# dp_isotime.h follows just-bashit's `iso-8601-basic`; it does not define it.
# test_dp_isotime.c pins a snapshot of that helper's output, and a snapshot
# goes stale silently — the committed vectors keep passing while the helper
# moves on, and doppler keeps emitting a spelling the comment above them
# says matches. This runs the real helper against the real C, so the
# agreement is checked rather than claimed.
#
# ISOTIME_REQUIRE=1 makes a missing just-bashit an error instead of a skip.
# CI sets it, because CI clones the reference and an absent one there means
# the gate silently stopped gating; a developer without the checkout gets the
# skip and still has the golden vectors.
ISOTIME_REQUIRE ?= 0
check-isotime-parity: build ## Check dp_isotime.h against just-bashit
	@BUILD_DIR=$(BUILD_DIR) bash scripts/check_isotime_parity.sh \
	    $(if $(filter 1,$(ISOTIME_REQUIRE)),--require,)

# ── The doc gates ────────────────────────────────────────────────────────────
# These four existed ONLY as inline CI steps, so none could be run locally by
# name — you found out a docstring example was broken by pushing. The RFC
# counted them as five gates with no target (#555); they are three targets
# because the snippet trio always runs together, under one CI condition.

test-stubs: ## Doctest every generated .pyi stub
	uv run python -m pytest --doctest-glob='*.pyi' -q \
	    $$(find $(PYEXT_DIR) -name '*.pyi')

# 79-col gate for the generated .pyi stubs. They are jm-owned and excluded from
# the main ruff run (its autofixing rules would rewrite them and drift the
# manifest gate), so this is a SEPARATE check-only pass: E501 + W505 have no
# autofix, so they verify the width WITHOUT ever touching the bytes -- the one
# way to enforce a rule on generated files without drifting them. E501 catches
# every line (signatures + docstring prose); W505 (doc-line-too-long) is the
# intent-explicit doc twin, redundant at max-doc-length == line-length == 79 but
# selected to document purpose. --isolated bypasses pyproject's .pyi exclusion
# (which force-excludes them). NOT yet wired into `lint`/CI: it is red today
# because jm emits unwrapped docstring summaries + wide signatures. It goes
# green once just-makeit wraps generated stubs to 79 (just-makeit#744); wire it
# into the `lint` prereqs at that point.
lint-stubs: ## Check generated .pyi stays within 79 cols (check-only; see jm#744)
	$(RUFF) check --isolated --select E501,W505 --line-length 79 \
	    --config 'lint.pycodestyle.max-doc-length = 79' \
	    $$(find $(PYEXT_DIR) -name '*.pyi')

# Docstring-coverage burn-down meter. Scores BOTH doc faces: the .pyi stubs
# (static, always) and the runtime __doc__ (needs the built extension, so this
# reads both only after `make build`/`make pyext`; without a build it reports
# the stub face and notes the runtime face was skipped). This target is the
# human-facing both-face REPORT; the no-regression ratchet
# (`--check` against docs/.docstring-coverage-baseline) runs inside the
# docs-check gate (DOCS_CHECK_PRE_CMDS above), green from baseline and failing
# only on backsliding. Refresh the baseline after improving coverage with
# `python scripts/check_docstring_coverage.py --update-baseline`.
check-docstring-coverage: ## Report docstring coverage across both Python faces
	uv run python scripts/check_docstring_coverage.py

test-api-docs: ## Doctest the docs/api/*.md reference pages
	uv run python -m pytest --doctest-glob='*.md' -q docs/api/

# python/C/shell fences under docs/. Run as one target and reported together:
# a page usually breaks all three the same way, and stopping at the first costs
# a whole round trip to see the rest. The C gate compiles every fence against
# build/libdoppler.a, so `make build` first; the python gate's `broker=` fences
# need a NATS broker on :4222 and skip without one.
test-snippets: ## Run the python/C/shell doc-fence gates
	@fail=0; \
	 for t in test_doc_snippets test_c_doc_snippets test_sh_doc_snippets; do \
	     echo "=== $$t ==="; \
	     uv run python -m pytest -m docs_snippets -q \
	         src/doppler/tests/$$t.py || fail=1; \
	 done; \
	 if [ "$$fail" = 0 ]; then echo "test-snippets: ALL FENCE GATES PASS"; \
	 else echo "test-snippets: FAILURES above"; exit 1; fi

test-examples-python: ## Run the Python example gate (requires pyext)
	uv run pytest -m examples -q src/doppler/tests/test_examples.py
	@$(MAKE) --no-print-directory test-example-downstream-python

# The Python half of the downstream example: build its extension against the
# same interpreter pytest will use, then run its own suite. Separate from the C
# half because test-examples-c also runs in a Python-free CI container.
#
# The suite is the point of the example, not a smoke test -- it pins that the
# jm `view` really is a second constructor over one core, so a regression in
# jm's view codegen fails here rather than in someone's project.
test-example-downstream-python: ## Build + test the downstream example (Python)
	@echo "Building downstream jm example (Python extension)..."
	@cmake -B $(DOWNSTREAM_BUILD_DIR)-py $(DOWNSTREAM_DIR) \
	    -Ddoppler_DIR=$(abspath $(BUILD_DIR)) \
	    -DBUILD_PYTHON=ON \
	    -DPython3_EXECUTABLE=$$(uv run python -c 'import sys; print(sys.executable)') \
	    -DCMAKE_BUILD_TYPE=Release \
	    > $(DOWNSTREAM_BUILD_DIR)-py.log 2>&1 \
	    || { echo "  configure FAILED"; cat $(DOWNSTREAM_BUILD_DIR)-py.log; exit 1; }
	@cmake --build $(DOWNSTREAM_BUILD_DIR)-py --parallel $(NPROC) \
	    > $(DOWNSTREAM_BUILD_DIR)-py.log 2>&1 \
	    || { echo "  build FAILED"; cat $(DOWNSTREAM_BUILD_DIR)-py.log; exit 1; }
	PYTHONPATH=$(abspath $(DOWNSTREAM_DIR))/src \
	    uv run pytest -q $(DOWNSTREAM_DIR)/src/iqtools/capture/tests/

# ── Bench scripts that are not save/compare ──────────────────────────────────
# Representative published numbers live under benchmarks/published/v<ver>/, two
# builds per release (portable = the wheel, native = -DDOPPLER_NATIVE=ON), each
# stamped with the compiler + flags. Measure on a REAL machine, not CI.
#
# bench-interleaved is the canonical path: it builds portable + native in two
# git worktrees and runs them alternately K times (K=5; override with K=N),
# keeping the per-benchmark best so the *from src* column isn't corrupted by
# cross-run drift. bench-publish stamps a single build by hand if you need it.
bench-interleaved: ## Measure portable + native alternately, denoised (VERSION=)
ifndef VERSION
	@echo "usage: make bench-interleaved VERSION=X.Y.Z [K=5]"; exit 1
endif
	uv run python scripts/bench_interleaved.py $(VERSION) $(if $(K),-k $(K),)

bench-publish: ## Stamp one build's numbers for a release (VERSION= BUILD=)
ifndef VERSION
	@echo "usage: make bench-publish VERSION=X.Y.Z BUILD=portable|native"; exit 1
endif
	uv run python scripts/bench_report.py --publish $(VERSION) \
		--build $(or $(BUILD),portable)

bench-docs: ## Render docs/benchmarks.md from the published numbers
	uv run python scripts/bench_report.py --page --out docs/benchmarks.md

# Transport (P0) bench: NATS firehose throughput + status-plane RTT via the
# bench_stream C harness. Self-contained — starts a JetStream broker on an
# isolated port (temp store) and tears it down.
bench-stream: ## Transport bench: NATS firehose + status-plane RTT
	uv run python scripts/bench_stream.py $(if $(VERSION),--publish $(VERSION),)

bench-report: ## Portable-build trend across releases
	uv run python scripts/bench_report.py

# ── Container images ─────────────────────────────────────────────────────────
# Three shapes, one job each (see deploy/docker/README.md). Each target builds
# AND smoke-runs its image, so the target is the whole gate — CI just calls it.
# Raw `docker build` is never invoked outside these rules; the flags live here.

# Each target builds its image, then smoke-tests it via scripts/smoke-image.sh
# — the SAME script release.yml runs against the pushed image, so the local
# gate and the published-image gate cannot drift (see the script header).

docker-runtime: ## Build+smoke the runtime "try it" image (needs the wheel on PyPI)
	docker build -f deploy/docker/Dockerfile.cli \
	    --build-arg DOPPLER_VERSION=$(DOCKER_VERSION) \
	    -t $(DOCKER_IMAGE):$(DOCKER_TAG) .
	bash scripts/smoke-image.sh runtime $(DOCKER_IMAGE):$(DOCKER_TAG)

docker-sdk: ## Build+smoke the SDK / develop image (doppler-sdk)
	docker build -f $(EXAMPLES_DOCKERFILE) --target sdk \
	    --build-arg JM_VERSION=$(JM_VERSION) \
	    -t $(DOCKER_IMAGE)-sdk:$(DOCKER_TAG) .
	bash scripts/smoke-image.sh sdk $(DOCKER_IMAGE)-sdk:$(DOCKER_TAG)

docker-downstream: ## Build+smoke the iqtools showcase image (doppler-downstream-jm)
# The build itself runs `make test` inside the image, so a green build IS the
# smoke; the run only confirms the shipped, pre-built package imports.
	docker build -f $(EXAMPLES_DOCKERFILE) --target downstream-jm \
	    --build-arg JM_VERSION=$(JM_VERSION) \
	    -t $(DOCKER_IMAGE)-downstream-jm:$(DOCKER_TAG) .
	bash scripts/smoke-image.sh downstream \
	    $(DOCKER_IMAGE)-downstream-jm:$(DOCKER_TAG)

docker-stream: ## Build+smoke the lean compose streaming-services image
	docker build -f $(EXAMPLES_DOCKERFILE) --target stream-services \
	    -t $(DOCKER_IMAGE)-stream-services:$(DOCKER_TAG) .
	bash scripts/smoke-image.sh stream \
	    $(DOCKER_IMAGE)-stream-services:$(DOCKER_TAG)

docker-examples: docker-sdk docker-downstream docker-stream ## Build+smoke all build-on-doppler images
	@echo "All build-on-doppler images built and smoked."
