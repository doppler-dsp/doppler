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
# Routed through the dev group, not PATH: uv.lock is the one pin, shared by
# make, the pre-commit hooks and jm's own `c_format_command`.
CLANG_FORMAT = $(DEV_RUN) clang-format
CLANG_TIDY   = $(DEV_RUN) clang-tidy
MDFORMAT   = $(DEV_RUN) mdformat
PRE_COMMIT = $(DEV_RUN) pre-commit
SYNC_CMD   = $(UV) sync

# ── lint-<tool> dispatch ─────────────────────────────────────────────────────
# LINT_TOOLS stamps out one `lint-<tool>` target each; .pre-commit-config.yaml
# calls `make -s lint-<tool>`, so a hook cannot run a tool differently from the
# way `make format` runs it. That is what closes the "same command, different
# environment" class of drift: the tool comes from `--group dev` and uv.lock
# owns the version, so there is no `additional_dependencies` left to drift.
LINT_TOOLS   = conflict ruff ruff-format mdformat clang-format clang-tidy \
               phase-conversion stimulus-sources retired-names ci-pipefail \
               rust-abi
FORMAT_TOOLS = ruff-format ruff mdformat clang-format

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
# jm project, which `jm apply` recreates; src/doppler/.*/tests/validation/ holds
# each object's results.md, written by that folder's own validate.py, so
# reformatting it would drift against the next regeneration. vendor/ is not
# ours.
MD_EXCLUDE_RE = ^(vendor/|docs/c-api/|docs/benchmarks\.md$$|docs/.*/archive/|examples/[^/]+/docs/|src/doppler/(.*/)?tests/validation/)

LINT_ruff        = $(RUFF) check --fix --unsafe-fixes $(RUFF_PATHS)
LINT_ruff-format = $(RUFF) format $(RUFF_PATHS)

# ── C formatting ─────────────────────────────────────────────────────────────
# The exclusions live HERE, not in the hook config, for the same reason
# MD_EXCLUDE_RE does: one place, and `make format` cannot disagree with the
# hook about which files are ours.
#
# native/inc/** carries jm-injected public declarations, and *_ext.c are jm
# aggregators — both regenerated by `jm apply`, so formatting them drifts the
# manifest gate. `(^|/)` rather than `^`: examples/downstream-jm is a separate
# jm project living in-tree with its own native/inc, and an anchored pattern
# misses it — clang-format then ping-pongs with jm over its generated headers
# forever. vendor/ is the pristine upstream FFT cores (pocketfft / PFFFT).
#
# jm_bench.h and jm_test.h joined that list once jm 0.59.0 could SAY they had
# diverged. They are jm's create-only files, exactly like the jm_*.h under
# native/inc/ — the only reason they were formatted is that they live under
# native/benchmarks/ and native/tests/, which the path pattern above never
# reached. The cost was invisible until gh-949: `jm status` reported
# jm_bench.h as OUTDATED on a tree whose content is byte-identical to jm's,
# because doppler's own formatter had rewritten it from K&R to GNU. Measured
# — strip all whitespace from both and the hashes match. Its siblings under
# native/inc/ were absent from that same report, which is the control.
#
# So the exclusion is what makes the new signal mean something: with these
# formatted, OUTDATED fires forever on a file nobody needs to act on, and a
# real upstream change would arrive into a line already being ignored.
C_EXCLUDE_RE = (^|/)(native/inc/|vendor/)|(^|/)jm_(bench|test)\.h$$|_ext\.c$$

C_FILES = git ls-files '*.c' '*.h' | grep -Ev '$(C_EXCLUDE_RE)'

LINT_clang-format = @$(C_FILES) | xargs -r $(CLANG_FORMAT) -i --style=file

# clang-tidy does NOT take clang-format's file list, and sharing C_FILES was
# the reason its first real run emitted 6916 lines of which ~1700 were noise.
# clang-format formats any file it is handed; clang-tidy COMPILES one, so it
# needs a translation unit that compile_commands.json actually has an entry
# for. C_FILES has three kinds of file that are not that: the per-object
# `*_ext_<obj>.c` fragments (105 of them) are #included into the generated
# aggregator and never compiled alone, so each one fails on PyObject_HEAD;
# examples/downstream-jm is a separate jm project with its own build tree, so
# every file there fails on a header it cannot find; and a bare .h is not a TU.
# A parse failure is not a finding — it is the gate not running on that file,
# which is the failure mode this whole exercise exists to remove.
#
# Scope is the LIBRARY, not the harnesses. native/tests and native/benchmarks
# carry ~4900 diagnostics, 4587 of them cert-err33-c for an unchecked printf
# return in a test — a real rule, aimed at code where nobody is reading the
# screen. Holding shipped code to it while a test prints freely is the honest
# split, and it is what keeps the gate at zero rather than behind a ratchet.
#
# native/inc is covered WITHOUT being listed: .clang-tidy's HeaderFilterRegex
# surfaces header diagnostics through whichever TU includes them. That is also
# why it must not be listed — clang-tidy would then own a file jm generates.
# The object half of `_ext_<obj>.c` is the CLASS name, so it is CamelCase for
# Resampler/RateConverter/HalfbandDecimator — a lowercase-only character class
# misses exactly those three and lets 53 parse failures back in.
TIDY_FILES = git ls-files 'native/src/*.c' | grep -Ev '_ext(_[A-Za-z0-9_]+)?\.c$$'

LINT_clang-tidy   = @$(TIDY_FILES) | xargs -r $(CLANG_TIDY) -p $(BUILD_DIR) --quiet

# FIRST in LINT_TOOLS, and that ordering is the point rather than a
# preference: mdformat NORMALISES a conflict marker instead of refusing it, so
# a check that runs after it is looking for something the formatter has already
# rewritten. Check-only, so not in FORMAT_TOOLS. Logic lives in the script
# rather than inline here so a test can run it over seeded files — a lint
# target whose only exercise is corrupting the repo is a target nobody proves.
LINT_conflict = ./scripts/conflict-check.sh

# Check-only, so it is in LINT_TOOLS but deliberately NOT in FORMAT_TOOLS.
# nco_core.h calls confining the double->phase-word conversion "a STRUCTURAL
# rule rather than a stylistic one", and records that duplicated copies have
# already drifted once (one truncated while a sibling rounded). A rule with
# no gate behind it is how that happened; this is the gate.
LINT_phase-conversion = $(UV) run python scripts/check_phase_conversion_sites.py

# A rename is finished when the old name appears NOWHERE, and that is a
# different event from the build going green. fec_ -> ccsds_tm_ (#828) went
# green with eleven occurrences of the retired prefix still in the tree --
# five of them the NAME a C test prints, which no compiler can notice. The
# list is data, added by the commit that retires a name.
LINT_retired-names = $(UV) run python scripts/check_retired_names.py

# A CI step may not throw away the exit code it just produced. The default
# Actions shell is `bash -e`, where a PIPELINE reports the last command's
# status -- so `make coverage | tee coverage.txt` was green over a recipe that
# had failed, and the missing report only surfaced one step later as the patch
# gate opening an lcov that was never written. Same shape as the leading `-`
# this repo removed from the coverage pytest: the code is computed and then
# discarded. Registration-free -- it walks every workflow and composite action,
# so a new file is covered the moment it exists.
LINT_ci-pipefail = $(UV) run python scripts/check_workflow_pipelines.py

# ffi/rust/ is the one binding jm does not generate, so `jm status --check`
# has nothing to say about it and an `extern "C"` block is a promise no
# compiler, linker or runtime ever checks. doppler#911 is what that cost: a
# control port declared `*const f32` against a C `const double *`, so C read
# twice the buffer it was handed, from a safe method, and the crate's own test
# passed because it used an all-zero control — the one input whose bit pattern
# is identical at both widths. Check-only, so not in FORMAT_TOOLS.
LINT_rust-abi = $(UV) run python scripts/check_rust_abi.py

# Stimulus and its measurement have ONE home, and it is the library: wfmgen
# (wfm_synth_*/Synth) generates signal, ber_* measures it. A private copy in a
# test, harness or example does not merely duplicate code -- it invents a
# CONVENTION, and the convention is what goes wrong silently. Measured: a
# 0.25-of-peak backoff in ratesync_demo under-drove RateSync ~40x in loop gain
# and failed its own lock assertion with nothing pointing at the level.
LINT_stimulus-sources = $(UV) run python scripts/check_stimulus_sources.py

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
# pytest-xdist is in the dev group, so `make test-python` runs it on every
# core. Override with `PYTEST_ARGS="-n 0"` to force serial.
#
# It is ON by default, in a way that costs nothing: pytest-benchmark disables
# itself under xdist ("Benchmarks cannot be performed reliably in a
# parallelized environment"), so a blanket `-n auto` would silently turn the
# benchmark tests under src/doppler/*/benchmarks/ into correctness-only runs —
# and a gate must not quietly measure less than it appears to. So the run is
# SPLIT: everything except those directories goes through xdist, and they get
# a second, serial pass. Nothing is downgraded and nothing is skipped.
#
# Measured on 8 cores: 274s all-serial -> 178s, as 60s for the parallel bulk
# (2493 tests) plus 117s for the serial benchmark pass (133 tests). Same
# 2626 passed / 6 skipped either way. The benchmark pass now dominates and is
# irreducible by parallelism BY DEFINITION -- pytest-benchmark needs a quiet
# machine, and min_rounds x max_time puts a floor of ~1s on each of its tests.
#
# Do NOT parallelize `test-stubs`: measured 2.35s -> 2.24s (startup dominates),
# and a text-mode .pyi shares ONE doctest namespace across the whole file, so
# any future finer-than-file split would break name bindings that earlier
# examples in the same file establish.
PYTEST_ARGS     ?=
PYTEST_SELECT   = -m "not docs_snippets and not examples"
# Every src/doppler/<mod>/benchmarks directory, derived not listed.
PYTEST_BENCH_DIRS = $(wildcard src/doppler/*/benchmarks)
# ONE pass, and the benchmark directories are IN it -- as tests, not as
# measurements. `--benchmark-disable` says so EXPLICITLY: the fixture stays
# (so the 138 benchmark tests still run, and a broken benchmark script fails
# here, where it should) and nothing is timed.
#
# Explicit, because the alternative is implicit and conditional: pytest-
# benchmark also disables itself whenever xdist is active, which would leave
# this depending on `-n auto`. `PYTEST_ARGS="-n 0"` is a DOCUMENTED override
# (see the note above), and under it the timing would quietly switch back on
# inside the step everyone runs -- slower, and taking measurements nobody
# reads. `-p no:benchmark` is the wrong knob: it removes the fixture, and all
# 138 tests error with "fixture 'benchmark' not found". Both measured.
#
# MEASURING is `make bench-python`, and the separation is the point: tests are
# run constantly, benchmarks occasionally, and they answer different
# questions. This target used to do both -- a second, SERIAL pass over the
# benchmark dirs so pytest-benchmark would measure -- which put a measurement
# nobody was reading inside the step everyone runs. Measured in CI: 139s of a
# 268s step, on all SIX Python versions, ~14 minutes a run spent timing code
# on a shared runner and discarding the numbers. Timing on a shared runner is
# also the thing doppler#543 already deleted perf-regression.yml over.
TEST_PYTHON_CMD = uv run pytest src/ -v $(PYTEST_SELECT) \
                      --benchmark-disable -n auto $(PYTEST_ARGS)
TEST_RUST_CMD   = cargo test --manifest-path $(RUST_DIR)/Cargo.toml

# Fail-closed: every src/doppler/examples/*.py (plus the standalone example) is
# discovered and run by the pytest gate -- no hand list to rot. Skips live in
# src/doppler/examples/.examples-skip (reasons mandatory).
TEST_EXAMPLES_CMD = $(MAKE) --no-print-directory test-examples-c

TEST_ALL_DEPS = test test-examples test-python test-examples-python

# The stream suite's nats:// tests self-skip when 127.0.0.1:4222 is
# unreachable, so whether they RUN is a property of the environment, not of the
# test selection. CI started the broker with a bare `bash scripts/start-nats.sh`
# and tore it down with an inline `docker rm -f nats` written two different
# ways -- and `gates-check` scans for `make <target>`, so the one step that
# decides whether a whole transport is exercised was invisible to the gate that
# exists to catch CI-only steps. As targets they are visible, listed in
# GATES_PROVISION, and runnable before `make test-python` or `make coverage`.
#
# `docker rm -f` first because start-nats.sh runs `docker run --name nats`,
# which fails outright if a container by that name is already there -- fine for
# a fresh runner, wrong for a dev box running it twice.
nats-up: ## Start the NATS JetStream broker the nats:// stream tests need
	@docker rm -f nats >/dev/null 2>&1 || true
	@bash scripts/start-nats.sh

# Symmetric with start-nats.sh's two paths: kill the binary by its pidfile,
# remove the container if there is one. Both unconditionally and both quietly,
# because `nats-down` runs with `if: always()` after a failed job and must not
# turn "the tests failed" into "the cleanup failed too".
#
# EVERY WRITER IS STOPPED AND WAITED FOR BEFORE THE STORE IS REMOVED, and that
# ordering is the whole point. `kill` returns when the signal is QUEUED, not
# when the process is gone -- measured here, nats-server stayed alive for ~900
# further `kill -0` polls after `kill` returned, and a JetStream shutdown
# spends that window flushing stream state to disk. An `rm -rf` racing it
# empties a stream's msgs/ directory and then fails its rmdir on a file the
# server re-created in between:
#
#   rm: cannot remove '.../streams/DP_WORK_ep752264292/msgs': Directory not empty
#
# which is a cleanup step failing a job whose tests had passed. Reproduced
# deterministically against a writer that keeps writing through SIGTERM: with
# the wait it removes clean, without it, that exact message.
#
# The container comes down before the store too. It publishes no bind mount
# today, so it cannot be the writer -- but "stop every writer, then remove" is
# the invariant, and ordering it the other way would let a future bind mount
# reintroduce this silently.
#
# The removal itself cannot fail the target. That is the `if: always()` rule
# above: the store is a temp directory, and a surviving one is worth a warning
# and not worth converting a red test run into a red cleanup.
nats-down: ## Stop and remove the NATS JetStream broker
	@pidfile="$${NATS_PIDFILE:-$${TMPDIR:-/tmp}/doppler-nats.pid}"; \
	 if [ -f "$$pidfile" ]; then \
	     pid="$$(cat "$$pidfile")"; \
	     kill "$$pid" 2>/dev/null || true; \
	     n=0; \
	     while kill -0 "$$pid" 2>/dev/null; do \
	         n=$$((n + 1)); \
	         if [ $$n -eq 100 ]; then kill -9 "$$pid" 2>/dev/null || true; fi; \
	         if [ $$n -ge 150 ]; then \
	             echo "nats-down: pid $$pid outlived SIGTERM+SIGKILL"; \
	             break; \
	         fi; \
	         sleep 0.1; \
	     done; \
	     rm -f "$$pidfile"; \
	 fi
	@docker rm -f nats >/dev/null 2>&1 || true
	@store="$${TMPDIR:-/tmp}/doppler-nats-store"; \
	 rm -rf "$$store" 2>/dev/null || rm -rf "$$store" 2>/dev/null || true; \
	 if [ -e "$$store" ]; then \
	     echo "nats-down: WARNING — $$store survived removal, so something is"; \
	     echo "  still writing to it. Not failing the target: this runs after a"; \
	     echo "  job that may already have failed, and the store is a temp dir."; \
	 fi
	@echo "nats-down: broker stopped"

# `gates` must run every gate CI does — enforced by `gates-check` (standard.mk),
# which scans ci.yml and fails on any `make <target>` CI runs that `gates`
# cannot reach. Every such target is here or in GATES_PROVISION; nothing is
# silently omitted. GATES_PROVISION is the setup/build steps a dev runs BEFORE
# gating (install the deps, build the tree), not gates themselves.
# nats-up/nats-down are provisioning, not gates: they decide whether the
# nats:// stream tests RUN at all (the suite self-skips on an unreachable
# broker), so they belong to the "bring the box up" half, the same as build.
# doppler's root compile_commands.json is a relative SYMLINK into the build
# tree, not a copy: it resolves to whatever the last configure wrote, so there
# is nothing to refresh and no staleness gate to add, and being relative it
# survives a worktree or a fresh clone. That is not a preference — the root
# entry had silently been a stale in-source COPY (247 entries against 425, cc
# off PATH with no -O3 -march), and scripts/bench_report.py reads it, so
# published benchmark records carried an unoptimized toolchain line.
#
# canonical takes `copy` or `symlink` (just-buildit.github.io#23, filed from
# here after its cp refused outright against the symlink doppler already had).
COMPILE_DB = symlink

# ccache-stats prints a hit rate; it asserts nothing and cannot fail the
# build, so it is provisioning/reporting rather than a gate. gates-check is
# what noticed -- CI called a target `gates` could not reach, and said so.
GATES_PROVISION = install-deps install-docs-deps build pyext nats-up \
                  nats-down install-deps-ci install-docs-deps-ci \
                  ccache-stats \
                  apt-stall-config
GATES_DEPS    = lint changelog-check release-notes-size-check \
                drift-check doxygen-check docs-check \
                gen-c-api-check \
                validate-check \
                test-all test-stubs test-api-docs test-snippets test-rust \
                abi-check link-check installed-headers-check \
                consumer-faces-check glibc-gate \
                specan-check check-isotime-parity coverage coverage-gate \
                docker-examples

# ── Build ────────────────────────────────────────────────────────────────────
# Compile through ccache when it is installed, and silently not when it is
# not: bootstrap.toml lists it, but a checkout predating that, or a box
# provisioned another way, must still build. DERIVED rather than declared for
# the same reason the coverage target derives its profile runtime -- asking
# whether the tool is there beats asserting that it is.
#
# CMake caches the launcher in CMakeCache.txt, so an existing build tree keeps
# whatever it was configured with until it is reconfigured. That is why this
# reaches CONFIGURE, not the build step.
CCACHE_BIN   := $(shell command -v ccache 2>/dev/null)
CCACHE_FLAGS := $(if $(CCACHE_BIN),-DCMAKE_C_COMPILER_LAUNCHER=ccache,)

CMAKE_FLAGS = -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              -DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) $(CCACHE_FLAGS) \
              $(CMAKE_ARGS)

# `CMAKE_FLAGS` reaches the CONFIGURE step only, and the standard's `build`
# ends with a bare `cmake --build $(BUILD_DIR)` — so there is no flag surface
# for `--parallel`, and doppler's build would drop from $(NPROC) jobs to one.
# CMake's own environment knob covers it, and covers it better: it applies to
# every `cmake --build` in the tree, including `pyext` and both downstream
# example builds, instead of only the one recipe a flag would reach.
export CMAKE_BUILD_PARALLEL_LEVEL = $(NPROC)

# clang-tidy and `scripts/bench_report.py` both look for the compilation
# database at the REPO ROOT, while cmake writes it into $(BUILD_DIR) — so the
# root entry is a second copy of a generated file, and a second copy goes
# stale silently. It did: the root database sat at a July in-source configure
# for weeks (247 entries against the build tree's 425, `cc` off PATH instead
# of the real `-march`/`-O3` line), so every consumer was reading flags no
# translation unit was actually compiled with, and nothing said so.
#
# A SYMLINK cannot drift — it resolves to whatever the last configure wrote,
# so there is nothing to refresh and no staleness gate to add. It is relative,
# so it carries no $(CURDIR) and works in a worktree or a fresh clone; the
# file is gitignored, hence the rule rather than a checked-in link. Hung off
# `build` (standard.mk's, extended here by prerequisite) so a clone gets it
# from the first build. `ln -sfn` is idempotent, and re-running while the
# link dangles — before the first configure — is harmless.
#
# It is a real file target rather than a .PHONY, so make skips it once the
# link is there. That also means `help-check` sees it: the target is listed in
# LOCAL_TARGETS and carries a `##` description like any other. Worth knowing
# why it has to be — help-check skips a file target that EXISTS, and this one
# is gitignored, so it exists on every developer's box and on no fresh
# checkout. Undocumented, it passes locally and fails only in CI.
# Canonical's `compile-commands` produces the root entry the same way, because
# COMPILE_DB is set to `symlink` above — so the explicit target and this
# build-time one agree by construction instead of racing to write the same
# path two different ways. This rule stays because it is hung off `build`: a
# fresh clone gets the database from its first build, without having to know
# to ask for it.
build: compile_commands.json
compile_commands.json: ## Link the compilation database to the build tree
	@ln -sfn $(BUILD_DIR)/compile_commands.json $@

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
uv run python scripts/gen_validation_log.py --check
uv run python scripts/gen_doc_versions.py --check
uv run python scripts/check_version_strings.py
uv run python scripts/check_doc_targets.py
uv run python scripts/check_doc_paths.py
uv run python scripts/check_serializable.py
uv run python scripts/check_doc_face_parity.py
uv run python scripts/check_init_param_optionality.py
endef

# docs-check's invariants WITHOUT the site build, so pre-commit can run them.
#
# The whole gate is ~11 s and most of that is zensical building the site; the
# 13 checks above are 1.6 s together. That is cheap enough to run on every
# commit, which matters because two of them -- check_api_docs and
# check_docstring_coverage --check -- are what caught a new class arriving
# undocumented in doppler#858, after a local run came back clean.
#
# Iterates DOCS_CHECK_PRE_CMDS itself rather than restating any of it, so this
# target and docs-check cannot disagree about what an invariant is: adding a
# line above puts it in both. Reports every failure rather than stopping at the
# first, the same as docs-check.
docs-invariants: ## docs-check's fast invariants, no site build (pre-commit)
	@tmp=$$(mktemp); trap 'rm -f "$$tmp"' EXIT; fail=0; \
	 printf '%s\n' "$$DOCS_CHECK_PRE_CMDS" >"$$tmp"; \
	 while IFS= read -r c; do \
	     [ -n "$$c" ] || continue; \
	     echo "=== $$c ==="; \
	     sh -c "$$c" || { echo "docs-invariants: FAILED: $$c"; fail=1; }; \
	 done <"$$tmp"; \
	 if [ "$$fail" != 0 ]; then \
	     echo "docs-invariants: FAILURES above — every check ran, all reported"; \
	     exit 1; \
	 fi; \
	 echo "docs-invariants: OK — $$(printf '%s\n' "$$DOCS_CHECK_PRE_CMDS" \
	     | grep -c .) invariant(s)"

define DOCS_CHECK_POST_CMDS
uv run python scripts/check_site_links.py
endef

# ── Doxygen ──────────────────────────────────────────────────────────────────
# Version matters more than it looks: doxygen releases disagree about what
# counts as a warning, so checking with a different version than CI's gives a
# different answer. Runs natively when the local doxygen matches CI's, and
# otherwise inside DOXYGEN_IMAGE, which pins it.
DOXYGEN_VERSION ?= 1.9.8
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
# The pinned doxygen comes from $(CI_IMAGE) — the image CI itself runs in —
# rather than from a bespoke doppler-doxygen image built here. Both existed to
# answer the same question ("give me CI's doxygen"), and the CI image is the
# one that can answer it by construction: the doxygen job runs INSIDE it, so
# what it ships is what CI used, not a second thing built from the same base
# and hoped to match. Retiring the private image also retires 556 MB and a
# `docker build` from every `gen-c-api` on a box whose doxygen differs.
#
# The mount uses the same absolute path inside and out, so any absolute path
# mkdoxy hands to doxygen resolves identically on both sides. `docker run -i`
# is load-bearing, not hygiene: mkdoxy invokes `doxygen -` and pipes the whole
# generated config on STDIN (mkdoxy/doxyrun.py `run`), so without it doxygen
# reads an empty config, emits no XML, and the failure surfaces later as a
# missing index.xml — after the recipe has already wiped docs/c-api.
#
# The version assertion moved with it: an upstream bump in the image must fail
# LOUDLY here rather than silently changing generated output, so both fallback
# paths below check the image's doxygen against DOXYGEN_VERSION first.
DOXYGEN_IMAGE_CHECK = \
  got=$$(docker run --rm $(CI_IMAGE) doxygen --version 2>/dev/null \
         | cut -d' ' -f1); \
  if [ "$$got" != "$(DOXYGEN_VERSION)" ]; then \
      echo "doxygen: the CI image ships doxygen $$got, not $(DOXYGEN_VERSION)."; \
      echo "  CI runs the doxygen job INSIDE that image, so CI has moved too."; \
      echo "  Update DOXYGEN_VERSION and re-check the docs/c-api diff rather"; \
      echo "  than pinning around it."; \
      exit 1; \
  fi

define DOXYGEN_CHECK_CMD
@have=$$(doxygen --version 2>/dev/null | cut -d' ' -f1); \
if [ "$$have" = "$(DOXYGEN_VERSION)" ]; then \
  $(MAKE) -s doxygen-warn-gate; \
else \
  echo "doxygen-check: local $${have:-none} != CI's $(DOXYGEN_VERSION), using the CI image"; \
  $(DOXYGEN_IMAGE_CHECK); \
  docker run --rm -v "$(CURDIR)":/w:ro -w /w $(CI_IMAGE) \
    make -s doxygen-warn-gate; \
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

# `jm bench` covers jm's COMPONENTS — the objects in objects/*.toml — and
# structurally cannot see anything else. A `c_deps` entry (conv, rs, ccsds_tm)
# or a function-only module (mpsk, ber, snr, util, detection) is not a
# component, so its bench binary is built by CMake and run by NOTHING:
# `just-makeit bench util` answers `unknown component(s): util`.
#
# So ten benchmarks in this tree do not run here, and that is a known,
# tracked gap rather than an oversight — just-makeit#1023 asks jm to run
# them, and doppler deliberately does not carry a local runner that the fix
# would immediately retire. Until it ships they are run by hand:
#
#     cmake --build build --target bench_conv_core && ./build/.../bench_conv_core
#
# scripts/check_bench_coverage.py holds what CAN be held meanwhile — every
# one of them has a CMake target, records a measurement, and writes its JSON
# under the name a collector opens. See docs/dev/contributing/benchmarking.md.
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

# `llvm-cov` consults debuginfod for every object it opens, and Ubuntu turns
# that on FOR EVERY SHELL in /etc/profile.d/debuginfod.sh
# (DEBUGINFOD_URLS=https://debuginfod.ubuntu.com). Nothing it could fetch
# exists — these objects were built here, minutes ago, and carry their own
# coverage mapping — so each lookup is a network round trip that ends in a
# timeout.
#
# Measured on one extension `.so`: **over 120 s producing no output at all,
# against 0.105 s with the variable cleared.** This target opens 33 objects
# and passes over them three times (report, show, export), so the effect is
# not a slow build, it is a target that never finishes. It presents as a
# process asleep at 0 % CPU with an open socket, which reads like a hang in
# llvm-cov and sent one investigation to `-num-threads=1` (no effect —
# threading was never involved) before an fd listing showed the socket.
#
# Cleared for the coverage tools alone, via the recipe rather than a note in
# a doc: a developer's own debuginfod setup is untouched everywhere else, and
# a machine that has the variable set — which on Ubuntu is every machine —
# cannot be the difference between this target working and not.
# Applied at the CALL SITES, not folded into the tool names below: those are
# documented as overridable for a version-suffixed toolchain
# (`LLVM_COV=llvm-cov-22`), and an override must not be able to drop this.
COV_ENV       ?= env DEBUGINFOD_URLS=
LLVM_PROFDATA ?= llvm-profdata
LLVM_COV      ?= llvm-cov
COV_BASE      ?= origin/main
COV_PATCH_MIN ?= 90
# Excluded from the report: vendored code, jm-generated binding aggregators
# (`<mod>_ext.c`) and per-object fragments, and the test/bench harnesses — only
# first-party _core.c counts. `native/src/app/` (the wfmgen CLI) is excluded
# too: its body is an OBJECT lib compiled into BOTH the executable and a `.so`,
# but the report attributes only to the `.so`, whose copy is never executed.
#
# `jm_*.h` is vendored code that simply was not listed. It stayed invisible
# because the patch gate only sees lines a branch CHANGES, and nothing had
# changed one until the 0.58.0 re-vendor — which added jm's `jm_dot_f32` /
# `jm_dot_f64` inline helpers, 22 uncovered lines doppler does not call, and
# failed `coverage-gate` at 18.5% on a header doppler does not write. The
# `jm_` prefix is deliberately narrow: `native/inc/` is otherwise doppler's own
# (`dp_simd.h`, `dp_state.h`, every `*_core.h`) and those inline bodies must
# stay measured. Only jm's own inline functions lose attribution, and the SIMD
# MACROS doppler actually uses are unaffected — a macro expands at its call
# site and is attributed to the `.c` that used it.
COV_IGNORE    ?= (^|/)(vendor|build|build-cov|native/src/app)/|(^|/)jm_[a-z]+\.h$$|_ext(_[a-z0-9_]+)?\.c$$|/(tests|benchmarks)/

# Preflight: can this toolchain link an instrumented binary at all? Without
# it the build compiles every object and dies at the FIRST LINK with "cannot
# find .../libclang_rt.profile.a", which reads like a broken toolchain rather
# than a missing package -- it cost a full gates run to diagnose.
#
# DERIVED rather than listed in bootstrap.toml's dev group, because whether `clang`
# already carries its profile runtime is a property of the distro RELEASE and
# one package list cannot express it: Ubuntu 22.04 ships it inside clang's own
# packages and has no libclang-rt package at all (naming one fails apt
# outright), while 26.04 splits it into libclang-rt-21-dev that `clang` does
# not depend on. See the note in bootstrap.toml.
define COVERAGE_CMD
@printf 'int main(void){return 0;}\n' > $(COV_DIR)-probe.c 2>/dev/null \
    || { mkdir -p $(dir $(COV_DIR)) ; \
         printf 'int main(void){return 0;}\n' > $(COV_DIR)-probe.c ; }
@clang -fprofile-instr-generate -fcoverage-mapping \
     $(COV_DIR)-probe.c -o $(COV_DIR)-probe 2>/dev/null \
  || { v=$$(clang --version | head -1 | tr -dc '0-9. ' \
              | tr ' ' '\n' | grep -m1 '[0-9]' | cut -d. -f1); \
       echo ""; \
       echo "coverage: clang cannot link -fprofile-instr-generate here."; \
       echo "  The profile runtime (libclang_rt.profile.a) is missing, and"; \
       echo "  clang does not depend on it where it ships separately."; \
       echo ""; \
       echo "  Ubuntu/Debian 24.04+ :  sudo apt-get install libclang-rt-$$v-dev"; \
       echo "  Arch                 :  pacman -S compiler-rt"; \
       echo "  Fedora               :  dnf install compiler-rt"; \
       echo "  openSUSE             :  zypper install llvm-compiler-rt"; \
       echo ""; \
       echo "  Not in bootstrap.toml's dev group on purpose: 22.04 has no such"; \
       echo "  package at all and naming one fails apt outright. See bootstrap.toml."; \
       echo ""; \
       rm -f $(COV_DIR)-probe.c $(COV_DIR)-probe; exit 1; }
@rm -f $(COV_DIR)-probe.c $(COV_DIR)-probe
$(CMAKE) -B $(COV_DIR) -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    $(CCACHE_FLAGS) \
    -DDOPPLER_COVERAGE=ON -DBUILD_PYTHON=ON \
    -DPython3_EXECUTABLE=$(PYTHON_EXECUTABLE) \
    -DPYTHON_PACKAGE_DIR=$(CURDIR)/$(COV_DIR)/pkg/doppler \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    $(CMAKE_ARGS)
$(CMAKE) --build $(COV_DIR) --parallel $(NPROC)
mkdir -p $(COV_DIR)/pkg/doppler
# The extract below is ADDITIVE -- it overwrites what it carries and leaves
# everything else -- so a test deleted or renamed in src/ lingered here and
# kept running, forever. Found by sabotage: a temporary failing test removed
# from src/ still failed the next run from this copy.
#
# A deleted test that keeps passing is worse than one that keeps failing: it
# reports coverage for source that no longer exists. So the python half is
# cleared first, by name rather than by wiping the directory -- the freshly
# built *.so live here too (PYTHON_PACKAGE_DIR points at it) and removing
# those would force a relink on every run.
find $(COV_DIR)/pkg/doppler -type f ! -name '*.so' -delete 2>/dev/null || true
find $(COV_DIR)/pkg/doppler -type d -empty -delete 2>/dev/null || true
(cd src/doppler && tar cf - --exclude='*.so' .) \
    | (cd $(COV_DIR)/pkg/doppler && tar xf -)
# The tar above excludes *.so but NOT executables, so `_bin/wfmgen` rode
# along from the NORMAL build -- gcc, optimised -- while `import doppler`
# resolved to the instrumented clang/Debug library beside it. Every test
# asserting byte parity between the CLI and the library then compared two
# different builds of the same source and failed on floating-point output:
# 8 of them, invisible under the leading `-` on pytest below.
#
# Installing the instrumented binary makes them RUN and contribute coverage,
# which is the same reasoning as resolving the repo root by walking up
# rather than skipping the tests that could not find it.
#
# `mkdir -p` FIRST, and it is load-bearing rather than defensive. The build
# itself already bundles wfmgen here (wfmcompose/CMakeLists.txt's POST_BUILD
# copy into PYTHON_PACKAGE_DIR), and the purge above then deletes it -- it is
# not a `*.so` -- and takes the emptied `wfm/_bin/` with it. The directory
# came back only from the tar, i.e. only on a box whose `src/doppler/wfm/_bin/`
# already held a binary from a NORMAL build; that path is .gitignored, so it
# exists on a developer's tree and never in CI. This target therefore passed
# locally and died in CI on `install: cannot create regular file`, which is
# the whole "works here" class -- a recipe reaching for an artifact no clean
# checkout has.
mkdir -p $(COV_DIR)/pkg/doppler/wfm/_bin
install -m 755 $(COV_DIR)/native/src/wfmcompose/wfmgen \
    $(COV_DIR)/pkg/doppler/wfm/_bin/wfmgen
rm -rf $(COV_DIR)/prof && mkdir -p $(COV_DIR)/prof
# -j, because this suite is the single biggest block of the coverage job:
# 682s serial in CI against 135 tests. Source-based coverage is per-PROCESS
# (LLVM_PROFILE_FILE's %p), so concurrent tests each write their own .profraw
# and the merge below is unaffected -- the profile format is what makes this
# safe, not luck.
cd $(COV_DIR) && LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/c-%p-%m.profraw" \
    $(CTEST) --output-on-failure -j $(NPROC)
# -n auto for the same reason as ctest above: 486s serial in CI, 102s here.
# The per-process profile argument is identical -- xdist workers are separate
# processes and %p gives each its own .profraw.
#
# NO LEADING `-`. This ran with make ignoring pytest's exit code, hiding 89
# results (27 failed, 62 errors) in the one job that produces the coverage
# number -- the "a gate that cannot fail" shape where it can least afford to
# be. All 89 are now fixed at their cause rather than tolerated: 81 by
# resolving the repo root by walking up, 7 by installing the instrumented
# wfmgen above, and 1 by withholding a scaling assertion that profiling
# invalidates. A failure here is now a failure.
#
# DOPPLER_BUILD_DIR and PATH are what make this run self-sufficient, and both
# were supplied by the DEVELOPER'S MACHINE until the exit code started being
# read. This job builds only $(COV_DIR), yet three gates asked for `build/`
# (the C doc-snippet compiler, the conv/rs certify harnesses) and four asked
# for console scripts on PATH (`wfmgen` in the sh doc fences, `doppler-source`
# / `doppler-fir` / `doppler-specan` in the cli block tests). A dev box has an
# ordinary build tree and an activated venv, so all of it passed here and
# failed on a runner -- 44 failures and 6 errors, none of them about the code
# under test.
#
# The instrumented wfmgen goes FIRST, ahead of the venv's console script, so
# the fences drive the binary this run built and their work lands in the
# report. The variable is jm's own name for this, already used by the cargo
# leg below and by ffi/rust/build.rs; `build_dir()` in doppler.tests._repo is
# the single reader on the Python side.
LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/py-%p-%m.profraw" \
    PYTHONPATH="$(CURDIR)/$(COV_DIR)/pkg" \
    DOPPLER_BUILD_DIR="$(CURDIR)/$(COV_DIR)" \
    PATH="$(CURDIR)/$(COV_DIR)/pkg/doppler/wfm/_bin:$(dir $(PYTHON_EXECUTABLE)):$$PATH" \
    $(PYTHON_EXECUTABLE) -m pytest $(COV_DIR)/pkg/doppler \
    -q -p no:cacheprovider --ignore-glob='*/benchmarks/*' -n auto
-DOPPLER_BUILD_DIR="$(CURDIR)/$(COV_DIR)" \
    LLVM_PROFILE_FILE="$(CURDIR)/$(COV_DIR)/prof/rs-%p-%m.profraw" \
    cargo test --manifest-path $(RUST_DIR)/Cargo.toml
# -failure-mode=all, because a partial .profraw here is EXPECTED, not damage.
# Two tests spawn a child that imports the instrumented extension and is then
# terminated rather than allowed to exit -- test_server_ws.py kills the server
# it started, test_missing_extras.py runs a module that is supposed to fail.
# A killed process never runs the profile runtime's flush, so it leaves its
# mmap'd counter pages behind as 4096-byte files with an unfinished header.
# That is not fixable at the test end: you cannot ask a process you had to
# kill to write a clean profile first.
#
# llvm-profdata's DEFAULT failure mode is `any`, which discards the entire
# report over any single unreadable input -- measured here as 12 such files
# against 166 good ones, and the whole of `coverage` failing with "error: no
# profile can be merged". `all` merges everything readable and only fails when
# nothing is, which is the honest reading of a directory that is expected to
# contain both.
#
# This does not hide a real loss of data: if profiles genuinely stopped being
# written, the numbers collapse and `coverage-gate`'s threshold fails, which
# is the gate that exists for exactly that.
@objs="$(COV_DIR)/libdoppler.so $$(ls $(COV_DIR)/pkg/doppler/*/*.so \
    2>/dev/null | sed 's/^/-object /' | tr '\n' ' ')"; \
$(COV_ENV) $(LLVM_PROFDATA) merge -sparse -failure-mode=all $(COV_DIR)/prof/*.profraw \
    -o $(COV_DIR)/doppler.profdata; \
$(COV_ENV) $(LLVM_COV) report $$objs -instr-profile=$(COV_DIR)/doppler.profdata \
    -ignore-filename-regex='$(COV_IGNORE)'; \
$(COV_ENV) $(LLVM_COV) show $$objs -instr-profile=$(COV_DIR)/doppler.profdata \
    -ignore-filename-regex='$(COV_IGNORE)' \
    -format=html -output-dir=$(COV_DIR)/html; \
$(COV_ENV) $(LLVM_COV) export $$objs -instr-profile=$(COV_DIR)/doppler.profdata \
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
# ONE declaration of the version sites, in scripts/version_sites.py, with one
# reader and one writer over it. Both directions used to be spelled out here,
# per site, and they differed by more than a filename: five bespoke
# `grep | sed` reads and five bespoke `sed -i` writes, so the two spellings of
# one site could disagree about which line they meant, and a site in one list
# and not the other was invisible.
#
# They DID disagree. The writer stripped a pre-release suffix on the way into
# CMakeLists.txt and Cargo.toml (CMake and Cargo reject `1.2.3rc1`) while the
# reader read back whatever was there and `version-check` demanded every site
# match. Measured 2026-08-24:
#
#     $ make bump-version VERSION=0.44.0rc1
#     $ make version-check
#     ERROR: CMakeLists.txt has 0.44.0, but pyproject.toml has 0.44.0rc1
#
# A bump that cannot pass its own check, in the exact step release.yml runs
# against the tag. Each rule was locally reasonable; jointly they were
# impossible. Pre-releases are now REFUSED with that reason rather than
# half-supported -- doppler has cut zero of them -- and supporting them needs a
# change in the vendored standard.mk: just-buildit/just-buildit.github.io#30.
#
# uv.lock's copy is re-synced by the `uv lock` below; CHANGELOG is prose.
VERSION_SITES_CMD = python3 scripts/version_sites.py

# The labels, once. The probe COMMAND is identical for every site, so this
# derives the whole VERSION_PROBES block standard.mk wants instead of carrying
# five hand-written pipelines. `$(shell)` is deliberately absent: standard.mk
# EXPORTS VERSION_PROBES, so a shell in it would run once per recipe, on every
# target -- this is pure make text and costs nothing.
VERSION_SITE_LABELS = pyproject.toml CMakeLists.txt Cargo.toml \
                      bootstrap.toml just-makeit.toml

define VERSION_PROBE_NEWLINE


endef
# The newline leads each entry rather than trailing it: `foreach` joins with a
# SPACE, and a space after the newline lands inside the next entry's label
# (standard.mk splits on `|` only, so it would print as indentation). Leading
# it puts that space after the command, where the shell ignores it, and the
# empty first line is dropped by standard.mk's own blank-line filter.
VERSION_PROBES := $(foreach L,$(VERSION_SITE_LABELS),$(VERSION_PROBE_NEWLINE)$(L)|$(VERSION_SITES_CMD) --read $(L))

# `--write` sets every DECLARED site and reads them all back, so a site missing
# from VERSION_SITE_LABELS above is still written and still verified -- the
# silent direction of the old two-list arrangement cannot recur.
define BUMP_VERSION_CMD
$(VERSION_SITES_CMD) --write $(VERSION)
uv lock
@$(MAKE) --no-print-directory docs-relink
endef

# PROJECT names the namespaced env var standard.mk accepts a version from:
# JUST_BUILDIT_DOPPLER_VERSION. A BARE exported VERSION is refused for the
# release targets, because that name carries no evidence anyone meant it --
# measured before the guard existed, `VERSION=9.9.9 make -n bump-version`
# rewrote every manifest from a variable nobody typed, and `release-branch`
# would have branched on it. Set here rather than guarded here: the guard is
# generic to every repo on standard.mk and lives there, not in a private copy
# (just-buildit/just-buildit.github.io#29). Inert until that lands.
PROJECT = DOPPLER

define RELEASE_BRANCH_NOTES
@echo "  - if perf-relevant code changed since the last release (release.md"
@echo "    §2b): make bench-interleaved VERSION=$(VERSION) && make bench-docs"
@echo "    (on a representative machine), then commit benchmarks/published +"
@echo "    docs/benchmarks.md"
endef

# The script is VENDORED from canonical (just-buildit.github.io) and held to it
# by standard-check; everything doppler-specific is here. It used to be a local
# 194-line copy that had drifted from just-makeit's — same filename, same
# target, different capabilities.
#
# RW_PUBLISH_JOB must stay precise: a matcher that also caught
# "Publish container images" would read PyPI as live before it is, and one that
# matches nothing at all would let a rerun fire after a successful publish.
RELEASE_WATCH_CMD = @REPO=doppler-dsp/doppler RW_PKG=doppler-dsp \
                        RW_PUBLISH_JOB="publish to pypi" \
                        HANG_MIN=30 RW_MIN_ASSETS=3 \
                        scripts/release-watch.sh "$(VERSION)"

# ── Clean ────────────────────────────────────────────────────────────────────
CLEAN_PATHS = $(BUILD_DIR) $(PY_BUILD_DIR) $(UBSAN_DIR) $(TSAN_DIR) \
              $(GLIBC_BUILD_DIR) \
              docs/doxygen/ site/ \
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
                docs-invariants \
                jm-apply changelog-assemble changelog-assembled-check \
                plot-rx-dynamics \
                gen-c-api-check \
                gen-c-api-run \
                package-c package-c-tarball sdist release-notes \
                print-jm-version nats-up nats-down \
                docs-relink docs-drift-check drift-check changelog-check \
                release-notes-size-check workflow-syntax-check \
                jm-pin \
                issue-link-check \
                validate validate-c validate-check \
                characterize characterization-check \
                doxygen-warn-gate \
                test-examples-c test-examples-python test-example-downstream \
                test-example-downstream-python \
                package-starter-tarball test-starter-tarball \
                test-stubs test-api-docs test-snippets lint-stubs \
                test-ubsan test-tsan \
                check-docstring-coverage \
                abi-check link-check consumer-faces-check \
                glibc-check glibc-gate glibc-image specan-check \
                check-isotime-parity \
                tests-ssot validation-report-check \
                compile_commands.json \
                install-docs-deps install-deps-ci install-docs-deps-ci \
                apt-stall-config deps-budget-check cargo-floor-check \
                bench-coverage-check kwarg-parity-check issues \
                doc-sections-check \
                installed-headers-check \
                ci-image ci-image-check ci-image-shell ci-image-source-hash \
                ci-shell ci-run ci-gates ccache-stats \
                wheel-check wheel-smoke release-smoke \
                bench-python \
                bench-interleaved bench-publish bench-docs bench-stream \
                bench-report \
                docker-runtime docker-sdk docker-downstream docker-stream \
                docker-examples smoke-image

# ── Vendored from canonical ──────────────────────────────────────────────────
# Verbatim copies the drift gate holds to canonical, alongside standard.mk
# itself. Edit canonical and re-vendor; never edit these in place.
VENDORED_FILES = scripts/release-watch.sh

include standard.mk

# ── Everything below is genuinely doppler's own ──────────────────────────────

# Hung off `lint` as an extra prerequisite rather than added to standard.mk,
# which is vendored and must not be edited. CI runs `make lint` and nothing
# else, so this is what makes the rule enforced instead of merely written in
# native/tests/README.md — which is the distinction that matters, since the
# convention WAS written down while 90 copies of CHECK accumulated under it.
#
# `changelog-check` joined this list for the second half of that same reason.
# It was in GATES_DEPS and nowhere else, and NO CI job runs `make gates` — so
# a gate this repo believed it had was executed by nothing on every PR for as
# long as it has existed. Being inert (#705) was only half the problem; not
# running was the other half, and `gates` is a local convenience, not CI.
#
# `release-notes-size-check` joined for the SAME reason, one release later. It
# was still GATES_DEPS-only, so the two-sentences-above paragraph described it
# exactly: a gate the repo believed it had, run by nothing on any PR. It now
# also enforces the per-entry length that changelog.d/README.md asks for, and
# guidance without a gate is the thing this repo keeps re-learning -- v0.44.0
# assembled to 72,636 characters at a median of 24 lines per entry while a
# README asked politely for less.
lint: tests-ssot characterization-check validation-report-check changelog-check \
      workflow-syntax-check release-notes-size-check \
      issue-link-check deps-budget-check ci-image-check cargo-floor-check \
      bench-coverage-check kwarg-parity-check doc-sections-check

# The base the assertion ratchet compares against, same shape as COV_BASE:
# no test file may end up with FEWER assertions than the base ref has. A
# migration or a badly resolved rebase can drop assertions while the suite
# stays green -- this branch dropped 43 across three files before it was
# caught, and only one of the three was visible to review.
ASSERT_BASE ?= origin/main
tests-ssot: ## Verify no C test re-defines dp_test.h's macros or loses assertions
	@uv run python scripts/check_tests_ssot.py --base $(ASSERT_BASE)

# On `lint` rather than `bench` deliberately: `bench` is the occasional
# activity (see bench-python), so a gate hung off it reports a missing
# benchmark weeks after the component landed, if ever. Both directions are
# checked -- a tested component with no benchmark, and a benchmark no runner
# can reach -- because this repo has now had both, and the second one looks
# fixed from every angle except the snapshot it never appears in.
bench-coverage-check: ## Verify every tested component has a benchmark that runs
	@uv run python scripts/check_bench_coverage.py

# On `lint` rather than `drift-check`, because drift-check structurally
# CANNOT see this: a `_kwlist` lives in the wrapper body of a sacred
# `_ext_<obj>.c` fragment, and jm's own output says that part is "yours …
# not counted as drift". Measured before this target existed -- renaming
# DelayCf64.ptr's kwarg back to `n` left drift-check reporting exactly what
# it reported before, and passing, while a caller following the stub got a
# TypeError (#619). Reads both faces instead, discovering every fragment.
# GitHub does not parse a `run:` block, so a shell syntax error there does not
# announce itself -- the shell runs what it managed to parse and the step fails
# somewhere else. v0.43.0's release died on
# `mkdir: build/starter-pkg: Permission denied` because an unbalanced `"` in a
# COMMENT closed the container's script early and the rest ran on the runner.
workflow-syntax-check: ## Verify every workflow `run:` block is valid shell
	@uv run python scripts/check_workflow_syntax.py

kwarg-parity-check: ## Verify each binding accepts the keywords its stub publishes
	@uv run python scripts/check_kwarg_parity.py

# check_doc_paths.py holds the FILE half of a prose citation; this holds the
# SECTION half. Neither is covered by check_site_links, which reads links in
# the built site and cannot see `docs/design/mpsk.md` §3.2 written as prose
# inside a C comment. Both halves matter for the same reason: a citation
# reads as authority, so one pointing at the wrong argument is worse than
# none -- the reader concludes the claim is unsupported (#795).
doc-sections-check: ## Verify every `docs/x.md` section citation resolves
	@uv run python scripts/check_doc_sections.py

# `build` as a prerequisite, not a note: this reads `nm` output, so without a
# built archive it has nothing to check -- and a gate that skips when its
# input is missing is the failure it exists to catch, one level up. It says
# so and fails rather than passing on an empty set.
#
# Absolute, with no allowlist. The two headers #801 named are deleted, so the
# right number is zero; an allowlist would only be somewhere for a third to
# hide -- and there WAS a third, which this found on arrival.
#
# Plain `python3`, not `uv run`: this runs in the C-only build jobs, which
# never set uv up -- `make: uv: No such file or directory`, exit 127, which
# is a gate that cannot run rather than one that fails. The script imports
# nothing outside the standard library precisely so it does not need one.
installed-headers-check: build ## Verify installed headers declare only what the libraries define, and define no feature-test macro
	@command -v python3 >/dev/null 2>&1 || { \
	    echo "installed-headers-check: no python3 — this gate has not run,"; \
	    echo "  so it has not passed."; exit 1; }
	@python3 scripts/check_installed_headers.py

# Hung off `lint` rather than `validate-check` deliberately. `validate-check`
# re-runs each validator and compares -- a STALENESS gate, and staleness is
# not correctness: a generator that emits a broken report agrees with itself
# perfectly. Under that green gate, five of six reports carried a malformed
# table and ALL SIX rendered none of the findings they counted. This reads
# the RENDERED file instead, so it also catches a hand edit and a future
# validator that grows its own emitter. On `lint` it runs in the fast CI
# job, seconds after the mistake, instead of behind the validators.
validation-report-check: ## Verify every generated validation report is structurally sound
	@uv run python scripts/check_validation_reports.py

# The docs system deps (doxygen, graphviz, LaTeX) — the `docs` group in
# bootstrap.toml. standard.mk's `install-deps` installs only the default groups
# (runtime, dev) and is vendored, so it cannot take a group; this local target
# is the one place CI's doxygen job and the Pages docs workflow both reach for
# the docs group, keeping bootstrap.toml the single dependency list. Bootstraps jbx
# the same way standard.mk's install-deps does.
install-docs-deps: ## Install the docs system deps (bootstrap.toml `docs` group)
	@command -v jbx >/dev/null 2>&1 \
	    || curl -sSL https://just-buildit.github.io/get-jb.sh | bash
	PATH="$$HOME/.local/bin:$$PATH" jbx install-deps -g docs

# ── Provisioning with a deadline (doppler#879) ───────────────────────────────
# Both targets above reach the network three times — the jbx bootstrap curl,
# then jbx's own apt/brew — and none of those calls carries a timeout. So a
# stalled mirror does not fail provisioning, it HANGS it, until GitHub's
# 360-minute job limit, reporting `pending` the whole way where a reader
# cannot tell it from slow CI. Measured 2026-08-19: one commit hung 3h45m and
# then 2h42m at `Install system dependencies` while a sibling branch passed
# the identical step ninety seconds later.
#
# A RETRY ALONE WOULD HAVE BEEN DECORATION. The retry-on-failure pattern this
# repo already runs (.github/actions/setup-uv) keys on a step EXITING
# non-zero — correct for the 2026-08-07 fetch that timed out and *returned* an
# error, and inert against a hang, which never exits at all. The deadline is
# what converts the hang into a failure; the retry is what then recovers
# instead of merely failing faster. scripts/with-deadline.sh carries both, in
# that order, and says so.
#
# These wrap rather than replace `install-deps` so there is still ONE spelling
# of how the tool runs: a developer keeps the plain target, CI reaches for the
# bounded one, and neither is a second copy of the invocation.
#
# Sized from measurement rather than feel: healthy runs took 16–179s across 9
# samples (median 18), so 300s is ~1.7x the worst observed. The workflow steps
# carry `timeout-minutes` too — the backstop for a shell with no `timeout(1)`,
# and the half that cannot be bypassed.
# 600, not 300. The first live run caught a REAL stall at 300s — apt sat 229
# seconds with zero output midway through a 114 MB download from
# azure.archive.ubuntu.com — so the deadline works. But the 9 healthy samples
# it was sized from (16-179s) were all fast ones, and a wall-clock deadline
# CANNOT tell a stalled mirror from a slow one. 600s is ~3.4x the worst
# observed healthy run and still bounds a true hang at ten minutes against the
# 360 it used to get. The apt-level timeout below is what actually makes that
# distinction; this is the backstop for everything it does not cover.
#
# THE BUDGET MUST FIT THE STEP CEILING, and the first version of this did not:
# 600s x 3 tries is 30 minutes against a `timeout-minutes: 15`, so a retry
# after a full-deadline attempt was killed mid-download every time. Measured
# exactly that way -- the reclaim worked, apt restarted cleanly with no lock
# error, and the step ceiling ended it four minutes later. Keep
#
#     DEPS_DEADLINE x DEPS_TRIES + backoff  <  the workflow's timeout-minutes
#
# true whenever either number moves: 420 x 3 + 30s is ~21.5 min, under the 25
# the steps now allow.
#
# THREE SHORTER ATTEMPTS, NOT TWO LONG ONES, and the split is measured rather
# than guessed. On 2026-08-19 the runners' azure mirror was answering `Ign` and
# apt was falling back to archive.ubuntu.com, where the same 112 MB sometimes
# trickled. What the timings show is that the RETRY IS THE THING THAT WORKS:
#
#   Build on ubuntu-24.04   831s = 600 (attempt 1 killed) + 10 + 221 -> PASSED
#   Python 3.13 (re-run)    119s                                     -> PASSED
#   coverage, Python 3.11  1210s = 600 + 10 + 600, both exhausted    -> FAILED
#
# A healthy attempt is 120-220s, so 600s was buying nothing after the first
# few minutes -- a stalled attempt spends the rest of its deadline stalled.
# Three 420s attempts give the mirror lottery three rolls with ~2x headroom
# over a healthy install, inside the SAME step ceiling that two 600s attempts
# used. deps-budget-check enforces the inequality above either way.
DEPS_DEADLINE ?= 420
DEPS_TRIES    ?= 3

# Teach apt to fail a STALLED connection fast and retry it itself.
#
# This is the layer the problem actually lives at. A wall-clock deadline can
# only ask "has the whole step taken too long", which conflates a hung mirror
# with a large download over a slow link. apt can ask the question that
# matters — "has this connection produced no bytes for 30 seconds" — and its
# own retry RESUMES rather than restarting, with no dpkg lock left behind,
# which is exactly what the outer retry cannot do (see with-deadline.sh).
#
# Written as a config drop-in because `jbx install-deps` invokes apt-get
# itself and takes no pass-through options. Linux only: `sudo`/apt may not
# exist (macOS runners use brew), so the whole thing is best-effort and never
# fails provisioning by itself.
APT_STALL_CONF = /etc/apt/apt.conf.d/99-doppler-stall
define APT_STALL_CONFIG
Acquire::http::Timeout "30";
Acquire::https::Timeout "30";
Acquire::ftp::Timeout "30";
Acquire::Retries "3";
endef
export APT_STALL_CONFIG

apt-stall-config: ## Make apt fail a stalled mirror fast (CI, Linux, no-op elsewhere)
	@if command -v apt-get >/dev/null 2>&1 && command -v sudo >/dev/null 2>&1; \
	 then \
	    printf '%s\n' "$$APT_STALL_CONFIG" \
	        | sudo tee $(APT_STALL_CONF) >/dev/null \
	        && echo "apt-stall-config: 30s connection timeout, 3 retries" \
	        || echo "apt-stall-config: could not write $(APT_STALL_CONF) — skipped"; \
	 else \
	    echo "apt-stall-config: no apt-get/sudo — skipped (not an error)"; \
	 fi

# The Rust floor, as a gate rather than a comment. Cargo writes lockfile
# format v4 from 1.78 onward and will rewrite this file the first time it
# resolves anything -- silently, since a lockfile is not something anyone
# reads. The distro cargo on both Ubuntu LTSes is 1.75, which REFUSES v4
# ("lock file version 4 requires -Znext-lockfile-bump"), so a bump strands
# every developer who provisioned from bootstrap.toml.
#
# That is not hypothetical: it is gh-887, and it went unnoticed because the
# hosted runner's rustup cargo shadowed apt's, so CI passed while the
# documented dev path could not run `make test-rust` at all. The bump is
# invisible, the failure is far from the cause, and both halves are why this
# is a gate.
cargo-floor-check: ## Fail if the Rust lockfile or MSRV leaves the distro floor
	@lock=$(RUST_DIR)/Cargo.lock; toml=$(RUST_DIR)/Cargo.toml; rc=0; \
	 v=$$(grep -m1 '^version = ' "$$lock" | tr -dc '0-9'); \
	 if [ "$$v" != "3" ]; then \
	     echo "cargo-floor-check: $$lock is format v$$v, not v3."; \
	     echo "  cargo >= 1.78 rewrote it. The distro cargo on both Ubuntu"; \
	     echo "  LTSes is 1.75 and cannot read v4, so this strands anyone"; \
	     echo "  provisioned from bootstrap.toml (gh-887)."; \
	     echo "  Fix: sed -i 's/^version = 4$$/version = 3/' $$lock"; \
	     echo "  and re-run the Rust tests to confirm nothing needed v4."; \
	     rc=1; \
	 fi; \
	 if ! grep -q '^rust-version = ' "$$toml"; then \
	     echo "cargo-floor-check: $$toml declares no rust-version."; \
	     echo "  The MSRV is the floor this gate defends; undeclared, cargo"; \
	     echo "  cannot enforce it and the next contributor cannot see it."; \
	     rc=1; \
	 fi; \
	 [ $$rc -eq 0 ] || exit 1; \
	 echo "cargo-floor-check: OK — lockfile v3," \
	      "MSRV $$(grep -m1 '^rust-version = ' "$$toml" | cut -d'"' -f2)"

# The budget rule above, as a gate rather than a comment, because it already
# drifted once: 600x3 shipped against a 15-minute ceiling and every retry died
# mid-download. Derived from the real numbers on both sides -- DEPS_* here, the
# `timeout-minutes:` the workflows actually carry -- so moving either one
# without the other fails here instead of four minutes into a CI retry.
deps-budget-check: ## Verify DEPS_DEADLINE x DEPS_TRIES fits the workflow ceiling
	@budget=$$(( $(DEPS_DEADLINE) * $(DEPS_TRIES) )); \
	 backoff=0; i=1; \
	 while [ $$i -lt $(DEPS_TRIES) ]; do \
	     backoff=$$(( backoff + i * 10 )); i=$$(( i + 1 )); \
	 done; \
	 need=$$(( budget + backoff )); \
	 ceilings=$$(grep -rhoE '^[[:space:]]*timeout-minutes: [0-9]+' \
	     .github/workflows/*.yml | grep -oE '[0-9]+' | LC_ALL=C sort -un); \
	 if [ -z "$$ceilings" ]; then \
	     echo "ERROR: deps-budget-check found no timeout-minutes in"; \
	     echo "  .github/workflows — the scan found nothing, so it did not"; \
	     echo "  run, so it has not passed."; \
	     exit 1; \
	 fi; \
	 rc=0; \
	 for c in $$ceilings; do \
	     have=$$(( c * 60 )); \
	     if [ $$need -ge $$have ]; then \
	         echo "ERROR: provisioning can need $${need}s"; \
	         echo "  ($(DEPS_DEADLINE)s x $(DEPS_TRIES) tries + $${backoff}s"; \
	         echo "  backoff) against a $${c}-minute step ceiling ($${have}s)."; \
	         echo "  A retry after a full-deadline attempt would be killed"; \
	         echo "  mid-download, which is the bug this pairing exists to"; \
	         echo "  avoid. Lower DEPS_DEADLINE/DEPS_TRIES or raise"; \
	         echo "  timeout-minutes."; \
	         rc=1; \
	     fi; \
	 done; \
	 [ $$rc -eq 0 ] || exit 1; \
	 echo "deps-budget-check: $${need}s budget fits every step ceiling" \
	      "($$(echo $$ceilings | tr '\n' ' ')min)"

install-deps-ci: apt-stall-config ## install-deps under a per-attempt deadline + retries (CI)
	@./scripts/with-deadline.sh $(DEPS_DEADLINE) $(DEPS_TRIES) \
	    $(MAKE) --no-print-directory install-deps

install-docs-deps-ci: apt-stall-config ## install-docs-deps under a deadline + retries (CI)
	@./scripts/with-deadline.sh $(DEPS_DEADLINE) $(DEPS_TRIES) \
	    $(MAKE) --no-print-directory install-docs-deps

specan: ## Launch the live spectrum analyzer in a browser
	uv run doppler-specan

record-demo: ## Re-record the specan demo frames (docs/specan/frames.json)
	uv run python -m doppler.specan.record_demo \
	    --frames 120 --fft-size 512 \
	    -o docs/specan/frames.json

# Run all plot-generating examples and copy output PNGs to docs/assets/.
# Run before releasing whenever src/doppler/examples/ has changed.
#
# A characterization subject with a published page belongs here too, and is
# invoked SEPARATELY below rather than joined to the two lists that follow.
# Those lists are hand-maintained and must agree with each other — 35 script
# paths, then 35 PNG names in one `mv` — so a subject added to them would be
# two more edits that can silently disagree. It takes an explicit destination
# instead (the argv its `__main__` accepts), which needs neither list and
# writes straight to docs/assets/.
#
# Why it is regenerated here at all: `docs/assets/dsss_acq_characterization.png`
# was a hand-committed file from 2026-07-11 that NOTHING refreshed — not
# `make gallery` (the script was never in GALLERY_SCRIPTS) and not
# `make characterize`. Its page therefore showed an image that could not be
# reproduced by any target, which is the same "no gate runs it" failure the
# validation tree had.
# ONE entry only: the recipe below passes a hard-coded destination, so a second
# entry would become the first script's argv[1]. Loop + derive the destination
# before adding one — gh-694.
GALLERY_CHARACTERIZATIONS := \
    src/doppler/dsss/tests/characterization/burst_acquisition/characterize.py

GALLERY_SCRIPTS := \
    src/doppler/examples/agc_demo.py \
    src/doppler/examples/ccsds_link_demo.py \
    src/doppler/examples/agc_settling_design_demo.py \
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

# The receiver-dynamics figure. NOT a gallery script: the measurement is C
# (validate_rx_dynamics) and this only renders the telemetry that harness
# captured, so it depends on `build` rather than on the Python examples and it
# writes straight to docs/assets/ instead of the gallery's mv dance.
plot-rx-dynamics: build ## Render docs/assets/rx-dynamics.png from the C harness's telemetry
	uv run python scripts/plot_rx_dynamics.py

# EXAMPLES_SKIP is the same list src/doppler/tests/test_examples.py reads, and
# it is read here rather than restated: a script the smoke gate deliberately
# does not run is a script this target cannot run either, and two copies of
# that judgement is how they came to disagree. They did --
# mpsk_receiver_performance_demo.py has been skipped by the gate since its
# Monte Carlo was found to be draw-dependent, while `gallery` ran it anyway and
# exited 1 on the failure, so step 2 of the release checklist could not pass
# (gh-857). A skip is announced with its reason rather than being silent,
# because a quietly-absent panel is how gh-780's stale assets accumulated.
EXAMPLES_SKIP := src/doppler/examples/.examples-skip

gallery: ## Run the plot examples and copy their PNGs to docs/assets/
	@echo "Regenerating gallery plots..."
	@for script in $(GALLERY_SCRIPTS); do \
	    base=$$(basename $$script); \
	    if grep -q "^$$base:" $(EXAMPLES_SKIP) 2>/dev/null; then \
	        printf "  %-45s%s\n" "$$script" "SKIP ($(EXAMPLES_SKIP))"; \
	        continue; \
	    fi; \
	    printf "  %-45s" "$$script"; \
	    uv run python $$script > /dev/null 2>&1 && echo "OK" || { echo "FAIL"; exit 1; }; \
	done
	@for png in agc_convergence.png ccsds_link_demo.png agc_settling_design.png ber_awgn_demo.png cic_demo_spectrum.png corr_demo.png detection_curves.png detection_sim.png detection2d_demo.png lockdet_demo.png telemetry_fanin_demo.png mpsk_telemetry_capture_demo.png rate_converter_demo.png ratesync_demo.png ddc_fn_demo.png ddc_fn_scaling.png adc_demo.png hbdecim_q15_demo.png wfmgen_demo.png symbols_demo.png wfm_composition_demo.png wcdma_carriers_demo.png plan_demo.png plan_background_demo.png crowded_band_demo.png measure_demo.png measure_imd_npr_demo.png wfm_write_demo.png awgn_demo.png doppler_channel_demo.png wfm_io_demo.png dsss_burst_pipeline_demo.png async_dsss_receiver_spec_demo.png dsss_receiver_demo.png carrier_acq_rrc_demo.png mpsk_receiver_demo.png mpsk_receiver_performance_demo.png; do \
	    [ -e "$$png" ] && mv -f "$$png" docs/assets/ || true; \
	done
	# The demos that WRITE a capture leave it in the repo root. burst.blue
	# was cleaned; the SigMF and BLUE pairs the wfm_io/wfm_write demos
	# emit were not, so `make gallery` left four untracked files behind --
	# which is how an artifact gets committed by accident.
	@rm -f burst.blue probe.ci16 probe.ci16.sigmf-meta \
	       scene.cf32 scene.cf32.sigmf-meta
	@printf "  %-45s" "$(GALLERY_CHARACTERIZATIONS)"
	@uv run python $(GALLERY_CHARACTERIZATIONS) \
	     docs/assets/dsss_acq_characterization.png > /dev/null 2>&1 \
	     && echo "OK" || { echo "FAIL"; exit 1; }
	@echo "Gallery plots written to docs/assets/."

# ── Undefined behaviour ──────────────────────────────────────────────────────
# The C suite, rebuilt under UBSan, with `halt_on_error=1` so a report is a
# FAILURE and not a line in a log nobody reads.
#
# This exists because the behavioural suite structurally cannot cover this
# class. An out-of-range float->integer conversion is undefined, and on
# x86-64 it happens to yield 0 -- which is frequently the right answer, so
# every test stays green while the program is wrong by the standard and would
# behave differently on another target. Three instances of exactly that have
# already landed in this tree (symsync's nominal_inc, resamp's phase_inc at
# rate 1, get_branch's `ph >> 32`), and two of them CANCELLED, which is how
# `Synth(sps=1)` became a silently dead waveform instead of a crash. A gate
# that only runs the program cannot see any of it; this one can.
#
# `alignment` is excluded, and that is a RATCHET, not an exemption: 821
# reports today, every one `member access within misaligned address`, from
# casting a byte cursor to a struct pointer in dp_tlm, buffer and dp_state.
# Fixing them is its own change. The exclusion may only ever shrink -- if you
# find yourself adding a second `-fno-sanitize=`, fix the code instead.
UBSAN_DIR    ?= build-ubsan
UBSAN_OFF    ?= alignment
UBSAN_FLAGS   = -fsanitize=undefined,float-cast-overflow \
                -fno-sanitize=$(UBSAN_OFF) -fno-omit-frame-pointer -g
# halt_on_error is the whole point: without it UBSan prints and carries on,
# the suite passes, and the gate is decorative.
UBSAN_OPTS    = halt_on_error=1:print_stacktrace=1:abort_on_error=1

test-ubsan: ## Run the C suite under UBSan; any undefined behaviour fails
	$(CMAKE) -B $(UBSAN_DIR) -S . \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		"-DCMAKE_C_FLAGS=$(UBSAN_FLAGS)" \
		"-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined" \
		"-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=undefined" \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(UBSAN_DIR) --parallel $(NPROC)
	UBSAN_OPTIONS=$(UBSAN_OPTS) \
		$(CTEST) --test-dir $(UBSAN_DIR) --output-on-failure

# ── ThreadSanitizer ──────────────────────────────────────────────────────────
# Scoped to the tests that actually run threads, because that is what makes
# the result readable: a whole-suite TSan run is dominated by single-threaded
# targets that cannot report anything, and the signal is one line in it.
#
# TSAN_TESTS is a ctest -R pattern, not a hand list of binaries -- a new
# threaded test named for what it is gets picked up with no edit here. That
# is the same reasoning `check_bench_coverage` applies to benchmarks: a list
# maintained by hand is a list that goes stale silently.
#
# halt_on_error, for exactly the reason UBSAN_OPTS gives above: without it
# TSan prints a race and the suite still passes, and the gate is decorative.
TSAN_DIR   ?= build-tsan
TSAN_TESTS ?= race|parallel|thread
TSAN_FLAGS  = -fsanitize=thread -fno-omit-frame-pointer -g
TSAN_OPTS   = halt_on_error=1:second_deadlock_stack=1

test-tsan: ## Run the threaded C tests under TSan; any data race fails
	$(CMAKE) -B $(TSAN_DIR) -S . \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		"-DCMAKE_C_FLAGS=$(TSAN_FLAGS)" \
		"-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread" \
		"-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=thread" \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(TSAN_DIR) --parallel $(NPROC)
# An empty result set is not a pass. If the pattern matches nothing the gate
# has not run, so it has not passed -- the same trap the glibc and tarball
# gates were both caught by.
	@n=$$($(CTEST) --test-dir $(TSAN_DIR) -R '$(TSAN_TESTS)' -N \
	      | sed -n 's/^Total Tests: //p'); \
	 if [ "$$n" = "0" ] || [ -z "$$n" ]; then \
	   echo "test-tsan: no test matched '$(TSAN_TESTS)' — nothing ran,"; \
	   echo "  so this gate has not passed."; exit 1; \
	 fi; \
	 echo "test-tsan: $$n threaded test(s) under ThreadSanitizer"
	TSAN_OPTIONS=$(TSAN_OPTS) \
		$(CTEST) --test-dir $(TSAN_DIR) -R '$(TSAN_TESTS)' \
		--output-on-failure

blazing: ## Clean + Release + -march=native (max speed; never packaged)
	@$(MAKE) --no-print-directory clean
	$(CMAKE) -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DDOPPLER_NATIVE=ON \
		"-DCMAKE_C_FLAGS=$(BLAZING_CFLAGS)" \
		$(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)

gen-c-api: ## Regenerate docs/c-api/ from the headers (mkdoxy, CI's doxygen)
	@have=$$(doxygen --version 2>/dev/null | cut -d' ' -f1); \
	 if [ "$$have" = "$(DOXYGEN_VERSION)" ]; then \
	   echo "gen-c-api: local doxygen $$have matches CI's — running natively"; \
	   $(MAKE) -s gen-c-api-run; \
	 else \
	   echo "gen-c-api: local $${have:-none} != CI's $(DOXYGEN_VERSION) — shimming the CI image"; \
	   $(DOXYGEN_IMAGE_CHECK); \
	   shim=$$(mktemp -d); \
	   printf '#!/bin/sh\nexec docker run --rm -i -u %s:%s -v "%s":"%s" -w "$$PWD" %s doxygen "$$@"\n' \
	     "$$(id -u)" "$$(id -g)" "$(CURDIR)" "$(CURDIR)" "$(CI_IMAGE)" > $$shim/doxygen; \
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

# docs/c-api/ is GENERATED and COMMITTED, and until now it was the one
# generated tree with no --check behind it: `docs-drift-check` covers
# gen_related_pages / gen_readme / gen_install_scripts / gen_validation_log
# and stops there. It drifted, exactly as an ungated generated tree does --
# 22 pages, including `util__core_8h.md` missing `ema_step` and
# `ema_alpha_decim` from the EMA primitive campaign, which merged with its C
# API docs never regenerated and nothing to say so.
#
# Regenerates IN PLACE and asks git whether anything moved, rather than
# building into a scratch dir and diffing. That would be a second code path
# for the thing being checked -- doxygen behind a version shim, then mkdoxy,
# then the index.md restore -- and a check that reimplements its subject is
# the duplication this repo keeps paying for. CI's checkout is disposable; a
# developer is left with the tree REGENERATED, which is the state the failure
# message would have told them to produce anyway.
# Compared against the INDEX, not against HEAD, and that distinction is the
# whole difference between a gate and a wall. `git status --porcelain` reports
# a STAGED change too, so the pre-commit hook that runs this could never pass
# on the very commit introducing a regeneration -- you regenerate, `git add`
# exactly as the hook's own message tells you to, and it fails again on the
# files you just staged. The documented recovery did not work, so the habit it
# produced was `--no-verify`, which is worse than the drift.
#
# `git diff` (worktree vs index) is right in both callers. In CI's clean
# checkout the index IS HEAD, so it still asks "does the committed tree match
# the headers"; under pre-commit it asks "does what you are about to commit
# match the headers", which is the same question at the only moment it can be
# answered. Staging a stale tree is still caught: the recipe regenerates in
# place first, so the worktree holds the truth and a stale index differs from
# it.
gen-c-api-check: ## Fail if the committed docs/c-api is stale against the headers
	@$(MAKE) -s gen-c-api >/dev/null
	@d=$$(git diff --name-only -- docs/c-api; \
	      git ls-files --others --exclude-standard -- docs/c-api); \
	 if [ -n "$$d" ]; then \
	     printf '%s\n' "$$d"; \
	     echo "gen-c-api-check: docs/c-api is stale — $$(printf '%s\n' "$$d" \
	         | wc -l | tr -d ' ') file(s) differ from what the headers"; \
	     echo "  produce. The tree has been regenerated in place: review it,"; \
	     echo "  'git add docs/c-api', and commit."; \
	     exit 1; \
	 fi; \
	 echo "gen-c-api-check: docs/c-api matches the headers"

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
# opts in). Tarball naming used to stay in the caller, "per-platform and
# trivial" — see `package-c-tarball` below for how that turned out.
package-c: ## PREFIX=<dir> — build+install the relocatable C library (no Python)
ifndef PREFIX
	@echo "usage: make package-c PREFIX=<dir>"; exit 1
endif
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release \
	    -DBUILD_PYTHON=OFF -DCMAKE_INSTALL_LIBDIR=lib $(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(NPROC)
	$(CMAKE) --install $(BUILD_DIR) --prefix $(PREFIX)

# "Trivial" was true of each copy and false of the set. This was three
# hand-written `tar -czf` lines in release.yml — one per platform — wrapped in
# two more hand-written manylinux `docker run` blocks, and no target anywhere
# produced the artifact. So the tarball a release ships could not be built or
# inspected before cutting a tag, while `release-smoke` could only test one
# already published: doppler could smoke-test an artifact it could not make.
#
# The platform string is DERIVED here rather than passed in. Each CI copy
# hard-coded its own, which is precisely how one could disagree with the runner
# it ran on; uname cannot. `darwin` is spelled `macos` in the published names,
# and that rename is the only special case.
# The staging prefix goes UNDER $(BUILD_DIR), not the repo root. CI installed
# to ./install and got away with it on a throwaway runner; on a dev box that is
# an untracked directory in the tree (`install/` is not even gitignored) and
# scattered cmake output, which this repo's layout rule exists to prevent.
C_PLATFORM ?= $(shell uname -s | tr '[:upper:]' '[:lower:]' \
                  | sed 's/^darwin$$/macos/')-$(shell uname -m)
C_INSTALL_DIR ?= $(BUILD_DIR)/package-c-prefix
DIST_DIR      ?= dist

package-c-tarball: ## VERSION=x.y.z — build + tar the released C library into $(DIST_DIR)/
ifndef VERSION
	@echo "usage: make package-c-tarball VERSION=<x.y.z>"; exit 1
endif
# Staged fresh every time: an install into a surviving prefix MERGES, so a
# header deleted since the last run would ship in the tarball forever.
# $(abspath), not $(CURDIR)/, because C_INSTALL_DIR follows BUILD_DIR and an
# absolute BUILD_DIR made that `/home/.../doppler//tmp/...` -- cmake installed
# there while tar read the real path, and the release tarball came out EMPTY.
	@rm -rf $(C_INSTALL_DIR)
	@$(MAKE) --no-print-directory package-c PREFIX=$(abspath $(C_INSTALL_DIR))
	@mkdir -p $(DIST_DIR)
	@tar -czf "$(DIST_DIR)/doppler-$(VERSION)-$(C_PLATFORM).tar.gz" \
	    -C $(C_INSTALL_DIR) .
# And then CHECK it, because the failure above wrote a 20-byte archive on its
# way out: had the prefix merely been empty rather than absent, tar would have
# exited 0 and this would have shipped an empty tarball to a GitHub Release.
# Assert the shape a consumer needs, not merely that bytes exist -- the three
# documented faces resolve through include/, lib/ and lib/pkgconfig/.
	@tb="$(DIST_DIR)/doppler-$(VERSION)-$(C_PLATFORM).tar.gz"; \
	 for want in ./include/ ./lib/ ./lib/pkgconfig/; do \
	     tar -tzf "$$tb" | grep -q "^$$want" || { \
	         echo "package-c-tarball: $$tb has no $$want — refusing to ship it"; \
	         exit 1; }; \
	 done; \
	 echo "package-c-tarball: $$tb ($$(tar -tzf "$$tb" | wc -l) entries)"

# ── The starter tarball ──────────────────────────────────────────────────────
# `examples/downstream-jm` shipped as something a newcomer can extract and
# build, with doppler INSIDE it. The point is that the two commands in its
# README are the whole story: no doppler checkout, no install step, no
# CMAKE_PREFIX_PATH, no network.
#
# That is why the SDK is bundled rather than fetched. A FetchContent path would
# have made one platform-independent archive instead of three, and it would
# have moved the failure a beginner hits from "it built" to "it could not
# reach github.com" -- on the first command they run, in the artifact whose
# entire job is to demonstrate that consuming doppler is easy.
#
# The project half comes from `git archive`, not from a copy of the working
# tree: the working tree carries build/, .pytest_cache/ and a
# compile_commands.json full of absolute paths from this machine, and a
# starter that ships someone else's paths is a starter that reads as broken.
#
# TWO names, because they answer different questions. The published asset is
# `doppler-starter-<version>-<platform>.tar.gz`: on a release page beside
# `doppler-<version>-<platform>.tar.gz` it has to say what it IS, and
# `iqtools-…` says nothing to someone who has never heard of iqtools. The
# directory inside it stays `iqtools`, because that is the project's own name
# — its package, its manifests and its README all say so, and renaming it to
# yours is step one of using it.
STARTER_STAGE ?= $(BUILD_DIR)/starter-pkg
STARTER_DIR   ?= iqtools
STARTER_ASSET ?= doppler-starter

package-starter-tarball: ## VERSION=x.y.z — tar the starter project with the SDK bundled
ifndef VERSION
	@echo "usage: make package-starter-tarball VERSION=<x.y.z>"; exit 1
endif
	@rm -rf $(STARTER_STAGE)
	@mkdir -p $(STARTER_STAGE)/$(STARTER_DIR)
# HEAD, so the archive is a commit rather than whatever is unsaved. A starter
# built from a dirty tree cannot be reproduced by the person who receives it.
#
# Through a FILE rather than a pipe into tar, so `git archive` failing is
# caught by its own exit status. Piped, the `||` sees tar's status, not git's,
# and whether that fails is up to how the local tar treats an empty stream:
# GNU tar errors ("This does not look like a tar archive", exit 2), which is
# the only reason the pipe version was safe on Linux, and it is not a property
# worth betting the macOS leg on. Measured in the manylinux container against
# a checkout git refuses -- see the safe.directory note in release.yml.
	@git archive HEAD:$(DOWNSTREAM_DIR) > $(STARTER_STAGE)/project.tar \
	    || { echo "package-starter-tarball: git archive failed — is $(DOWNSTREAM_DIR) committed, and is this checkout one git will read?"; exit 1; }
	@tar -xf $(STARTER_STAGE)/project.tar -C $(STARTER_STAGE)/$(STARTER_DIR)
	@rm -f $(STARTER_STAGE)/project.tar
	@$(MAKE) --no-print-directory package-c \
	    PREFIX=$(abspath $(STARTER_STAGE)/$(STARTER_DIR)/third_party/doppler)
	@mkdir -p $(DIST_DIR)
	@tar -czf "$(DIST_DIR)/$(STARTER_ASSET)-$(VERSION)-$(C_PLATFORM).tar.gz" \
	    -C $(STARTER_STAGE) $(STARTER_DIR)
# Assert the shape the README promises, for the reason package-c-tarball does:
# a tar of an empty prefix exits 0 and ships nothing. Both halves are checked
# -- the project AND the doppler it is supposed to carry -- because bundling
# is the whole claim and a tarball missing it still configures on a machine
# that happens to have doppler installed.
	@tb="$(DIST_DIR)/$(STARTER_ASSET)-$(VERSION)-$(C_PLATFORM).tar.gz"; \
	 for want in $(STARTER_DIR)/CMakeLists.txt \
	             $(STARTER_DIR)/just-makeit.toml \
	             $(STARTER_DIR)/third_party/doppler/include/ \
	             $(STARTER_DIR)/third_party/doppler/lib/libdoppler.a \
	             $(STARTER_DIR)/third_party/doppler/lib/cmake/; do \
	     tar -tzf "$$tb" | grep -q "^$$want" || { \
	         echo "package-starter-tarball: $$tb has no $$want — refusing to ship it"; \
	         exit 1; }; \
	 done; \
	 echo "package-starter-tarball: $$tb ($$(tar -tzf "$$tb" | wc -l) entries)"

# The gate on the artifact, not on the source: extract the tarball somewhere
# with no relationship to this repo and run EXACTLY the commands the README
# leads with. `test-example-downstream` builds the same
# project from the source tree against a build-tree doppler -- a different
# question (can a consumer link libdoppler.a, answered on glibc 2.28 without
# Python), and it cannot see a packaging mistake, because nothing it touches
# comes out of the tarball.
#
# Runs in a temp dir OUTSIDE the repo: extracted under $(BUILD_DIR) it would
# still find this checkout through a relative path and pass while the bundle
# was empty, which is the failure it exists to catch.
#
# BUILD_PYTHON=OFF, and that is the README's headline path rather than a
# convenience here. The tarball can bundle doppler; it cannot bundle
# python3-dev and NumPy's headers, and the jm-owned preamble's
# `find_package(Python3 COMPONENTS Development.Module NumPy)` is REQUIRED --
# so a genuinely flag-free configure fails on any box without them. Measured:
# it fails on this one. The C half is what the tarball can promise with
# nothing installed at all, so the C half is what the README leads with and
# what this gate runs; the Python half is a documented second step with one
# prerequisite, already covered by test-example-downstream-python.
test-starter-tarball: ## Extract the starter tarball and build it with no flags
	@v="$$(sed -n 's/^version = "\(.*\)"/\1/p' pyproject.toml | head -1)"; \
	 $(MAKE) --no-print-directory package-starter-tarball VERSION="$$v" \
	   || exit 1; \
	 tb="$(abspath $(DIST_DIR))/$(STARTER_ASSET)-$$v-$(C_PLATFORM).tar.gz"; \
	 tmp="$$(mktemp -d)"; trap 'rm -rf "$$tmp"' EXIT; \
	 tar xzf "$$tb" -C "$$tmp"; \
	 printf "  %-22s" "extract + build"; \
	 if (cd "$$tmp/$(STARTER_DIR)" \
	     && cmake -B build . -DBUILD_PYTHON=OFF > "$$tmp/log" 2>&1 \
	     && cmake --build build --parallel $(NPROC) >> "$$tmp/log" 2>&1 \
	     && $(CTEST) --test-dir build >> "$$tmp/log" 2>&1); then \
	     echo "PASS"; \
	 else \
	     echo "FAIL"; cat "$$tmp/log"; exit 1; \
	 fi

# Source distribution, sibling of standard.mk's `wheel` (= uv build --wheel).
# release.yml's build-sdist job calls this instead of hand-rolling a
# uv-venv/pip/`python -m build --sdist` bootstrap.
sdist: ## Build a source distribution into dist/
	$(UV) build --sdist
	@echo ""
	@ls -lh dist/*.tar.gz

# JM_VERSION already reads just-makeit.toml (see the Container images block).
# release.yml re-derived the same value with its own grep/sed, so one pin had
# two extractions that agreed only by luck -- and the SDK image's jm is built
# from whichever one the caller happened to use. This is the one that counts.
print-jm-version: ## Print the just-makeit pin (release.yml reads it from here)
	@echo "$(JM_VERSION)"

# The ONE definition of the GitHub Release body — release.yml's github-release
# job pipes this into `body_path` rather than carrying its own copy of the
# Install template. Run it locally to see exactly what a tag will publish,
# WITHOUT cutting a release; it fails when the version has no CHANGELOG
# section, which is what an unpromoted [Unreleased] heading looks like.
release-notes: ## VERSION=x.y.z — print the GitHub Release body
ifndef VERSION
	@echo "usage: make release-notes VERSION=<x.y.z>"; exit 1
endif
	@scripts/release-notes.sh "$(VERSION)"

# Regenerates every generated doc region: the "## Related pages" blocks on
# docs/api/*.md, README.md's synced body from docs/index.md, the per-distro
# install scripts from bootstrap.toml, and the release version stamped into
# doc-version regions.
# NOT part of docs-relink, and not in CI, for one reason: this is the only
# generator whose input is off the machine. `--write` reads the live issue
# list through `gh`, so it needs the network and an authenticated CLI, and a
# CI job built on it would fail on a rate limit rather than on the tree.
#
# What IS gated is the half that can be: `--check` re-renders from the
# committed docs/dev/issue-tiers.toml and diffs the page, offline and
# deterministic, so a hand-edit of a generated page is caught (docs-check).
# Freshness against GitHub is not checkable offline, so the page carries the
# date it was derived and the command that derives it -- the same
# date-plus-derivation the rest of this repo requires of a recorded live
# value.
issues: ## Refresh docs/dev/issues.md from the live issue list (needs gh)
	uv run python scripts/gen_issue_tracker.py --write

docs-relink: ## Regenerate every generated doc region
	uv run python scripts/gen_related_pages.py --write
	uv run python scripts/gen_readme.py --write
	uv run python scripts/gen_install_scripts.py --write
	uv run python scripts/gen_validation_log.py --write
	uv run python scripts/gen_doc_versions.py --write
	uv run python scripts/gen_jm_pin.py --write

# The pre-push half of docs-relink: the three regions a source edit can drift
# without touching the generated file. The hook dispatches HERE rather than
# inlining the three commands, so the hook and this target cannot disagree
# about what "drifted" means. If it trips: run `make docs-relink`.
docs-drift-check: ## Check the generated doc regions are up to date
	uv run python scripts/gen_related_pages.py --check
	uv run python scripts/gen_readme.py --check
	uv run python scripts/gen_install_scripts.py --check
	uv run python scripts/gen_validation_log.py --check
	uv run python scripts/gen_issue_tracker.py --check

# Every object certified under the validation campaign owns
# src/doppler/<module>/tests/validation/<object>/, and `results.md` there is
# GENERATED -- so it gets the same treatment as every other generated region:
# a --check that fails when the committed file no longer matches what the
# code produces. Discovered, not registered, so a new object is gated the
# moment its folder exists.
#
# NB this is the REPORT's staleness only. The limits inside it are asserted
# by src/doppler/<module>/tests/test_validation_limits.py, which runs in the
# ordinary pytest suite -- the two gates answer different questions and both
# are needed.
VALIDATORS = $(shell find src/doppler -path '*/tests/validation/*/validate.py' | sort)

validate: ## Regenerate every object's validation report and plots
	@for v in $(VALIDATORS); do \
	    echo "=== $$v ==="; \
	    uv run python $$v || exit 1; \
	 done
	@$(MAKE) --no-print-directory validate-c

# The C half of "refresh the validation". ctest runs each of these with
# `--check`, which is the REGRESSION SUBSET -- the downselect that would catch
# a real defect returning. Run with NO arguments they perform the full sweep:
# every axis, every roll-off, the characterisation the reports quote. That is
# a one-and-done per component, refreshed when the component or the toolchain
# changes, and it is why the two modes exist at all.
#
# Measured, and the reason for the split: ratesync_scurve's full sweep is 81s
# and was 78% of `make test-fast`, so every push re-derived characterisation
# that had already been recorded. Its regression subset is 14s.
#
# Output goes to $(BUILD_DIR), not the tree: these tables are evidence for a
# report a human is writing, not a generated artifact with a staleness gate,
# and a validator that writes into the repo is the thing `write=False` exists
# to prevent on the Python side.
# Derived from the tracked SOURCES, not from what happens to sit in the build
# directory. Globbing `$(BUILD_DIR)/native/validation/validate_*` was tried and
# is wrong: it picked up `validate_symsync_ted_scurve`, a binary left from
# 2026-08-08 whose .c had since been deleted, and ran it as though it were a
# live harness. A stale artifact reporting PASS is worse than no harness at
# all, and a build directory is not a source of truth about what exists.
VALIDATORS_C = $(patsubst native/validation/%.c,\
                 $(BUILD_DIR)/native/validation/validate_%,\
                 $(shell git ls-files 'native/validation/*.c'))
VALIDATE_C_OUT = $(BUILD_DIR)/validation-full

.PHONY: validate-c
validate-c: ## Run every C validation harness's FULL sweep (not the --check subset)
	@if [ -z "$(VALIDATORS_C)" ]; then \
	    echo "validate-c: no harness sources found under native/validation/"; \
	    exit 1; \
	 fi
	@mkdir -p $(VALIDATE_C_OUT)
	@for v in $(VALIDATORS_C); do \
	    n=$$(basename $$v); \
	    if [ ! -x "$$v" ]; then \
	        echo "validate-c: $$n is not built — run 'make build' first"; \
	        exit 1; \
	    fi; \
	    echo "=== $$n (full sweep) ==="; \
	    $$v > $(VALIDATE_C_OUT)/$$n.txt 2>&1 \
	        || { echo "validate-c: $$n FAILED — see $(VALIDATE_C_OUT)/$$n.txt"; \
	             exit 1; }; \
	 done
	@echo "validate-c: $(words $(VALIDATORS_C)) harness(es) → $(VALIDATE_C_OUT)/"

# `--check` prints a unified diff when a report is stale, and this target used
# to send it to /dev/null -- so the ONE caller anyone uses discarded the whole
# diagnosis and printed a filename. Measured: four reports came back stale in
# CI and the log said only which files, which is the half that does not
# distinguish an edited validator (re-run `make validate`) from a machine
# difference (re-running fixes nothing). Captured and replayed on failure now,
# from `STALE:` onward so the per-limit PASS lines stay out of it.
validate-check: ## Fail if any validation report is stale (CI gate)
	@fail=0; \
	 for v in $(VALIDATORS); do \
	     out=$$(uv run python $$v --check 2>&1) || { \
	         echo "validate-check: STALE — $$v"; \
	         printf '%s\n' "$$out" | sed -n '/STALE:/,$$p'; \
	         fail=1; }; \
	 done; \
	 if [ "$$fail" = 0 ]; then \
	     echo "validate-check: OK — $(words $(VALIDATORS)) report(s) up to date"; \
	 else echo "validate-check: run 'make validate'"; exit 1; fi

# ── Characterization ─────────────────────────────────────────────────────────
# A characterization sweeps an object across its whole operating envelope —
# C/N0, Doppler, sample rate, seed — until the answer is statistically
# meaningful. That takes MINUTES, which is the entire reason it is a category
# of its own and not an example.
#
# Both subjects here used to live in src/doppler/examples/ and therefore ran on
# every push through the example smoke gate. Measured: 164.6 s + 117.7 s, i.e.
# **75% of that gate's 376 s**, against ~58 s for the other 65 examples put
# together. Shortening a 300-trial Monte-Carlo to fit a smoke gate trades away
# the statistical confidence that is its whole point, so the sweep moved
# instead of shrinking.
#
# Deliberately NOT in GATES_DEPS and not in ci.yml: this target is run on
# purpose, when the envelope is the question. What guards the code per-push is
# each subject's fast twin in src/doppler/<mod>/tests/, plus
# `characterization-check` below — and be honest about the difference: the twin
# proves the helpers still import and run, NOT that the envelope still holds.
# A regression that moves a pull-in boundary without breaking an import waits
# for the next `make characterize`. That window, and the figure-freshness gap
# below it, are tracked in gh-692 rather than left standing on this comment.
#
# Discovered by glob, like VALIDATORS, so a new subject is covered the moment
# its folder exists. That is not a style choice — the validation tree was
# executed by NOTHING for its first two objects because it needed a
# registration step nobody performed (docs/dev/validation.md).
CHARACTERIZATIONS = $(shell find src/doppler \
                        -path '*/tests/characterization/*/characterize.py' \
                      | sort)

characterize: ## Run every characterization sweep (MINUTES — deliberate, not per-push)
	@for c in $(CHARACTERIZATIONS); do \
	    echo "=== $$c ==="; \
	    uv run python $$c || exit 1; \
	 done
	@echo "characterize: $(words $(CHARACTERIZATIONS)) subject(s) swept"

characterization-check: ## Verify every characterization subject is runnable and has a fast twin
	@uv run python scripts/check_characterization.py

# The write half of the drift gate. `drift-check` was the only jm invocation
# the Makefile carried, so the routine action -- REGENERATING after a manifest
# edit or a pin bump -- had no target and got typed as a bare
# `.venv/bin/just-makeit apply`, which the make-SSOT hook then blocks. A gate
# with no matching write target teaches people to reach around the gate.
#
# `uv run` rather than the venv binary, and after the sync, so apply and
# drift-check cannot run different jms -- the skew this repo has been bitten by
# is a bare console script left over from an older pin.
#
# JM_APPLY_ARGS exists because arguments must travel through a variable, but
# note what a path argument does NOT do: `jm apply objects/x.toml` is a FULL
# regeneration that merely starts from that fragment, not a scoped one. Nothing
# here can limit the blast radius, which is why the reminder below is printed
# every time rather than written down somewhere.
JM_APPLY_ARGS ?=

# The jm pin lives in three files and a lock. `just-makeit.toml` is the SSOT,
# `gen_jm_pin.py --write` (via docs-relink) propagates to pyproject.toml and the
# downstream example, and `uv lock` re-resolves the dev group the pin names.
# Before this target that sequence had no home, so every bump improvised it --
# which is how the 0.63.3 -> 0.65.0 bump came to be done with a raw `sed`, a
# direct script call and a bare `uv lock`, in a repo whose rule is that make is
# the SSOT. `gen_jm_pin.py --check` already gates that the sites agree; this
# gates HOW they get there.
jm-pin: ## JM=x.y.z — move the just-makeit pin everywhere and re-lock
ifndef JM
	@echo "usage: make jm-pin JM=<x.y.z>"; exit 1
endif
# Pre-flight BEFORE touching anything, because the failure mode is asymmetric:
# `docs-relink` runs under `uv run`, so once pyproject.toml names a version uv
# cannot resolve, the environment is broken and this target can no longer run
# to fix itself. Measured while writing it -- `make jm-pin JM=9.9.9` left three
# files moved, a stale lock, and no way back through make.
#
# It also answers the question actually being asked most of the time: has the
# version been released yet? `jm status` says CLOSED, and a closed issue is not
# a release.
	@curl -fsS -o /dev/null "https://pypi.org/pypi/just-makeit/$(JM)/json" || { \
	    echo "jm-pin: just-makeit $(JM) is not on PyPI — nothing changed."; \
	    echo "  A merged fix is not a release. Check:"; \
	    echo "    gh release list --repo just-buildit/just-makeit --limit 3"; \
	    exit 1; }
	@sed -i 's/^jm_version = ".*"/jm_version = "$(JM)"/' just-makeit.toml
	@$(MAKE) --no-print-directory docs-relink
	@uv lock
	@echo "jm-pin: pinned $(JM). Next: make jm-apply, then make drift-check."

jm-apply: ## Regenerate jm-owned glue from the manifest (then run drift-check)
	uv sync --group dev --no-install-project
	uv run just-makeit apply $(JM_APPLY_ARGS)
	@echo
	@echo "jm-apply: regenerated. Two things this does NOT do for you:"
	@echo "  - a sacred native/src/<mod>/<mod>_ext_<obj>.c fragment is"
	@echo "    reconciled member-by-member, never re-rendered -- read the diff"
	@echo "  - the downstream example has its own manifest; 'make drift-check'"
	@echo "    is what covers both. Run it now."

# The jm manifest drift gate. --no-install-project because the gate only reads
# the manifest, so there is no reason to build the C extension for it.
drift-check: ## jm manifest drift gate (CI's 'jm manifest drift')
# FIRST, and before the sync: the pin states the jm version in three files, and
# a bump that misses one used to be invisible — jm's gh-183 skew notice is a
# `warning:` line in a wall of advisory output and does not fail anything
# (mutation-verified: reverting the downstream pin alone left this target
# exiting 0). Checking here, ahead of the sync, also stops us installing one jm
# and gating with a manifest that names another.
	uv run python scripts/gen_jm_pin.py --check
	uv sync --group dev --no-install-project
	uv run just-makeit status --check
	@echo "── downstream jm example ──"
# The example is a just-makeit project in its own right, so its generated
# bindings/stubs/CMake can drift from its manifest exactly like doppler's can.
# Checked with the SAME pinned jm, so the example cannot silently document a jm
# version doppler is not on.
	cd $(DOWNSTREAM_DIR) && uv run --project $(CURDIR) just-makeit status --check

# One definition of "code", used by BOTH questions below — and by
# `issue-link-check`, which asks a third question of the same branch. Splitting
# it was how the halves would drift: a path added to one and not the others
# leaves a gate that is honest about a file the rest have never heard of.
CHANGELOG_CODE_PATHS = src native objects ffi

# The base a BRANCH is measured against. Overridden in CI, where a
# `pull_request` checkout has no local `main` to compare with — see ci.yml.
CHANGELOG_BASE ?= origin/main

# A branch that changes code must SAY what it closes, or say it closes
# nothing. GitHub only closes an issue when a closing keyword reaches the
# default branch; doppler rebase-merges, so a commit message carries it. This
# asks whether any branch used one, because nothing did: c0e0e615 gated the
# generated C API tree, which IS #714, and left it open for a day. The same
# triage found #663/#664/#665 fixed-and-open on PR #717. An open count that
# includes finished work is a backlog nobody can plan from.
#
# `No-issue:` passes, deliberately. Most branches close nothing, and a gate
# that demanded an issue number from a re-vendor would argue with its author —
# the failure mode changelog-check's own comment warns about. Silence is the
# only rejected answer, because silence cannot be told apart from a closure
# nobody wrote down.
#
# Logic in the script rather than inline so the gate's own test can drive it
# over seeded messages instead of fabricating a scratch repository.
issue-link-check: ## A branch changing code must declare Closes #N or No-issue:
	@ISSUE_BASE=$(CHANGELOG_BASE) ISSUE_CODE_PATHS="$(CHANGELOG_CODE_PATHS)" \
	    ./scripts/issue-link-check.sh

# `changelog-check` asks TWO questions, folded into one target rather than
# split into two, because they share the code-path definition above and a
# second target would be a second place to keep it right.
#
#   1. PER BRANCH — did THIS branch change code without touching CHANGELOG.md?
#   2. PER REPO   — has code shipped since the last tag with [Unreleased] empty?
#
# Question 1 is the one that matters, and it is new. The repo-state question
# alone goes INERT the moment a single entry exists, so every branch after the
# first passes for free: #700 shipped a public C API — a whole EMA primitive —
# with no entry at all, straight through this target, and #705 is that hole.
# The 0.58.0 bump on `chore/jm-0.58.0` would have gone the same way this week.
# A question about what this branch did cannot be satisfied by somebody else's
# earlier commit, which is exactly the property the repo-state question lacks.
#
# The design is lifted from just-buildit/just-makeit#956, which cites #705 by
# URL as its motivating case; jm hit the same thing from the other side, having
# assembled a seven-PR release in which not one PR wrote an entry. jm keeps it
# in `local.mk`; doppler has no `local.mk`, and this target already lived here.
#
# TOUCHING the file is the bar, not growing a particular section. A refactor
# that genuinely warrants no user-facing note is one honest line from passing,
# and a gate that tries to judge which changes *deserve* an entry is a gate
# that argues with its author.
#
# INERT on main by construction: HEAD is an ancestor of the base there, so the
# range is empty and question 1 has nothing to judge. It only has an opinion on
# a branch that is ahead.
#
# The RELEASE PR is the one legitimate empty [Unreleased]: promotion moves every
# entry into `## [X.Y.Z]`, so the notes exist, just not under [Unreleased]. That
# exemption is narrow on purpose — the version must HAVE a section AND its tag
# must NOT exist yet, which is what stops it becoming permanent. It applies to
# question 2 only; a release PR still touches CHANGELOG.md, so question 1 needs
# no exemption at all.
changelog-assemble: ## Promote changelog.d/ fragments into [Unreleased]
	@$(UV) run python scripts/changelog-assemble.py

changelog-assembled-check: ## Fail if any fragment is still unassembled
	@$(UV) run python scripts/changelog-assemble.py --check

# Sized against what would actually be PUBLISHED, not what CHANGELOG.md holds:
# a version section may carry a `### Highlights` block, and release-notes.sh
# publishes that instead when the whole section will not fit. The same number
# doubles as the release-cadence signal -- deferring a release is what makes it
# grow -- so there is no separate "days since the last tag" rule to drift out
# of step with this one. It runs here rather than at tag time because
# `github-release` needs `publish-python`: at tag time the version is already
# on PyPI and PyPI refuses a re-upload.
release-notes-size-check: ## Fail if the release notes would exceed GitHub's cap
	@uv run python scripts/check_release_notes_size.py

changelog-check: ## A branch changing code must add a changelog entry
	@base=$$(git merge-base HEAD $(CHANGELOG_BASE) 2>/dev/null) || { \
	  echo "changelog-check: no merge base with $(CHANGELOG_BASE) —"; \
	  echo "  fetch it (CI needs fetch-depth: 0) or set CHANGELOG_BASE."; \
	  exit 1; \
	}; \
	files=$$(git diff --name-only "$$base"..HEAD); \
	if [ -z "$$files" ]; then \
	  echo "changelog-check: no commits ahead of $(CHANGELOG_BASE) — branch check inert"; \
	else \
	  pat=$$(printf '%s\n' $(CHANGELOG_CODE_PATHS) | sed 's|.*|^&/|' | paste -sd'|'); \
	  code=$$(printf '%s\n' "$$files" | grep -E "$$pat" || true); \
	  if [ -z "$$code" ]; then \
	    echo "changelog-check: no code changes on this branch"; \
	  elif printf '%s\n' "$$files" | grep -qE '^(CHANGELOG\.md|changelog\.d/.+\.md)$$'; then \
	    echo "changelog-check: $$(printf '%s\n' "$$code" | grep -c .) code file(s) on this branch, changelog touched — OK"; \
	  else \
	    echo "changelog-check: this branch changes code and says nothing — FAIL"; \
	    printf '%s\n' "$$code" | sed 's/^/  /' | head -20; \
	    echo ""; \
	    echo "  Write ONE FILE naming what changed, so the release is a"; \
	    echo "  promotion rather than an archaeology exercise:"; \
	    echo ""; \
	    echo "    changelog.d/<added|changed|fixed|removed|breaking>/<slug>.md"; \
	    echo ""; \
	    echo "  The directory IS the section heading; the content is the"; \
	    echo "  entry, starting with '- '. See changelog.d/README.md for why"; \
	    echo "  it is a file and not a line in CHANGELOG.md."; \
	    echo "  A purely internal change still gets one honest entry."; \
	    exit 1; \
	  fi; \
	fi; \
	t=$$(git describe --tags --abbrev=0 2>/dev/null || true); \
	n=$$(git log --oneline $${t:+$$t..}HEAD -- $(CHANGELOG_CODE_PATHS) 2>/dev/null | wc -l); \
	e=$$(awk '/^## \[Unreleased\]/{f=1;next} f&&/^## /{exit} f' CHANGELOG.md \
	     | grep -c '^- ' || true); \
	frag=$$(find changelog.d -mindepth 2 -name '*.md' 2>/dev/null | wc -l); \
	e=$$((e + frag)); \
	v=$$(awk -F'"' '/^version = /{print $$2; exit}' pyproject.toml); \
	if [ "$$n" -gt 0 ] && [ "$$e" -eq 0 ] \
	   && grep -q "^## \[$$v\]" CHANGELOG.md \
	   && ! git rev-parse -q --verify "refs/tags/v$$v" >/dev/null 2>&1; then \
	  echo "changelog-check: release PR for $$v — notes are promoted into [$$v], v$$v not yet tagged — OK"; \
	  exit 0; \
	fi; \
	if [ "$$n" -gt 0 ] && [ "$$e" -eq 0 ]; then \
	  echo "changelog-check: $$n code commit(s) since $${t:-repo start}, [Unreleased] is empty and changelog.d/ holds nothing — FAIL"; \
	  exit 1; \
	fi; \
	echo "changelog-check: $$e entry/entries ($$frag unassembled) for $$n code commit(s) since $${t:-repo start}"

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
EXAMPLE_BIN_DIR := $(BUILD_DIR)/native/examples
STANDALONE_BUILD_DIR := example-projects/standalone/build
DOWNSTREAM_DIR := examples/downstream-jm
# Build trees live under doppler's BUILD_DIR, not inside the example: the
# example is a jm project and `jm status` walks its tree, so a build dir there
# would be scanned on every drift check.
DOWNSTREAM_BUILD_DIR := $(BUILD_DIR)/downstream-jm

# Wall-clock ceiling per example, mirroring test_examples.py's TIMEOUT_S.
# It is load-bearing rather than defensive: the failure this gate exists to
# catch is an example nothing runs, and the cheapest way to reintroduce it
# is an example that runs forever -- the gate then hangs instead of failing,
# which reads as "still working" in CI until the job's own ceiling kills it
# and names the wrong thing. A deadline turns that into a FAIL with the
# example's name on it.
C_EXAMPLE_TIMEOUT ?= 120
C_EXAMPLE_SKIPS   := native/examples/.examples-skip

test-examples-c: build ## Smoke-test every standalone C example
# DISCOVERED, never listed. This used to iterate a hand-written list of nine
# names, so the other four compiled, shipped and were executed by nothing --
# with no reason recorded, nothing failing if a fifth joined them, and
# nothing noticing if one was deleted (gh-863). Opting one out now costs an
# entry in $(C_EXAMPLE_SKIPS) with a mandatory reason, the same contract
# src/doppler/examples/.examples-skip already holds the Python side to.
	@echo "Running C example smoke tests..."
	@bash scripts/smoke-c-examples.sh $(EXAMPLE_BIN_DIR) $(C_EXAMPLE_TIMEOUT)
	@echo "Building standalone example..."
	@cmake -B $(STANDALONE_BUILD_DIR) example-projects/standalone \
	    -DDOPPLER_BUILD_DIR=$(abspath $(BUILD_DIR)) \
	    -DCMAKE_BUILD_TYPE=Release \
	    > /dev/null 2>&1
	@cmake --build $(STANDALONE_BUILD_DIR) > /dev/null 2>&1
	@printf "  %-24s" "awgn_example"; \
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
	@printf "  %-24s" "downstream-jm"; \
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
# asks — does libdoppler.a link with the two flags the docs name? — and it is
# answered on Linux and macOS both.
# `-lm -lpthread` is the whole documented link line, and it is spelled here
# by hand ON PURPOSE: this target answers what a downstream typing the
# command gets, so taking the flags from pkg-config would make it agree with
# itself rather than with the docs. pthread joined the line when rs.c moved
# to `pthread_once`; on glibc >= 2.34 it is folded into libc and omitting it
# still links, which is exactly why a gate that names it is worth having.
link-check: ## Smoke-test that a downstream links libdoppler.a with -lm -lpthread
	@t=$$(mktemp -d); \
	 if cc example-projects/consumer/main.c -Inative/inc -I$(BUILD_DIR)/native/inc \
	       $(BUILD_DIR)/libdoppler.a -lm -lpthread -o "$$t/consumer_smoke" \
	    && ( cd "$$t" && ./consumer_smoke > /dev/null ); then \
	     echo "link-check: OK — libdoppler.a links with -lm -lpthread."; \
	     rm -rf "$$t"; \
	 else \
	     echo "link-check: FAIL — a downstream cannot link libdoppler.a" \
	          "with -lm -lpthread"; \
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

# The oldest glibc a released ARTIFACT may reference. Pure inspection, and only
# meaningful against a build MADE on that glibc: pointed at a modern distro's
# build it fails on that build's legitimately newer symbols, which is the
# check working, not a bug. `glibc-gate` below supplies the old-glibc input;
# this target is what it (and nothing else now) runs against the result.
#
# It inspects libdoppler.so AND every example binary, because the .so alone was
# not the whole shipped surface. deploy/docker/Dockerfile.examples ships three
# of those binaries (transmitter/receiver/spectrum_analyzer) in the compose
# image, and they had drifted to GLIBC_2.38 — fmod plus the __isoc23_sscanf /
# __isoc23_strtol redirects glibc's own headers apply from 2.38 onward — while
# this gate stayed green, because it only ever opened one file. `glibc-gate`
# was already BUILDING those binaries at the floor and then throwing the
# evidence away. The list is globbed, never enumerated, so a new example is
# covered the moment it builds.
GLIBC_MAX ?= 2.28
GLIBC_EXAMPLE_DIR = $(BUILD_DIR)/native/examples
glibc-check: ## Verify no glibc symbol newer than $(GLIBC_MAX) (needs an old-glibc build)
# Fail-closed on BOTH ways of reading nothing, because "no bad symbols found"
# and "no symbols found" produced the identical green line: pointed at a dir
# with no .so, objdump wrote its error to stderr, $$BAD came back empty and
# this printed ALL SYMBOLS OK. Harmless while a human ran it right after a
# build; a false pass once `glibc-gate` made it a gate in GATES_DEPS. The same
# trap has two more mouths now that the input is a globbed list: an empty glob,
# and a single file in the list that objdump reads nothing from.
	@so=$(BUILD_DIR)/libdoppler.so; \
	 if [ ! -f "$$so" ]; then \
	     echo "glibc-check: no $$so to inspect — build it first," \
	          "or run 'make glibc-gate' to build one on glibc $(GLIBC_MAX)"; \
	     exit 1; \
	 fi; \
	 examples=$$(find $(GLIBC_EXAMPLE_DIR) -maxdepth 1 -type f -perm -u+x \
	             2>/dev/null | sort); \
	 if [ -z "$$examples" ]; then \
	     echo "glibc-check: no example binaries under $(GLIBC_EXAMPLE_DIR) —" \
	          "the compose image ships three of these, so an empty list is a" \
	          "broken build, not a clean run"; \
	     exit 1; \
	 fi; \
	 rc=0; high=; n=0; \
	 for f in "$$so" $$examples; do \
	     n=$$((n + 1)); \
	     seen=$$(objdump -T "$$f" 2>/dev/null \
	             | grep -oP 'GLIBC_\K[0-9.]+' | sort -Vu); \
	     if [ -z "$$seen" ]; then \
	         echo "glibc-check: $$f references no versioned glibc symbol at" \
	              "all — objdump read nothing, so nothing was asserted"; \
	         rc=1; continue; \
	     fi; \
	     bad=$$(printf '%s\n' "$$seen" \
	            | awk -F. -v mx="$(GLIBC_MAX)" \
	                'BEGIN{split(mx,m,".")} $$1 > m[1] || ($$1 == m[1] && $$2 > m[2])'); \
	     if [ -n "$$bad" ]; then \
	         echo "glibc-check: $$f references glibc > $(GLIBC_MAX):" \
	              "$$(printf '%s ' $$bad)"; \
	         rc=1; \
	     fi; \
	     high=$$(printf '%s\n%s\n' "$$high" "$$seen" \
	             | grep -v '^$$' | sort -Vu | tail -1); \
	 done; \
	 [ $$rc -eq 0 ] || exit 1; \
	 echo "glibc-check: all glibc symbols <= $(GLIBC_MAX) across $$n files" \
	      "(highest: $$high)"

# The missing half: a way to PRODUCE an old-glibc build anywhere. Without it
# the floor was answerable only by pushing and reading CI, and `glibc-check`
# sat in GATES_DEPS as a gate no dev box could pass — so the fix is to supply
# the input, never to weaken the assertion above.
#
# Its own build dir, deliberately. Sharing $(BUILD_DIR) would leave a
# Buster-compiled CMake cache in the dev's tree, and the next local
# `cmake -B build` aborts on the changed compiler. STANDALONE_BUILD_DIR is
# overridden for exactly the same reason: it is the one path reached by
# `test-examples` that is not already derived from BUILD_DIR.
#
# `test-examples` runs here because it is the half of CI's old job that
# proves the artifact WORKS under glibc $(GLIBC_MAX), not merely that its
# symbol table looks right — dropping it would quietly shrink coverage.
#
# The image is a toolchain, not a product: no source baked in, checkout
# bind-mounted, nothing published. That is why this `docker build` sits here
# beside the gate it feeds rather than in the container-images section, whose
# rule is about the images doppler SHIPS.
GLIBC_BUILD_DIR  ?= build-glibc228
GLIBC_IMAGE      ?= $(DOCKER_IMAGE)-glibc228:$(DOCKER_TAG)
GLIBC_DOCKERFILE := deploy/docker/Dockerfile.glibc228

# The toolchain image is its own target because it now has TWO consumers:
# this gate, and `docker-stream`, which compiles the compose image's three
# streaming binaries at the floor rather than on the modern builder. Inlining
# the `docker build` in both would be two definitions of "glibc $(GLIBC_MAX)"
# free to drift — the exact thing Dockerfile.glibc228's own header says moving
# it out of ci.yml was meant to stop.
glibc-image: ## Build the glibc $(GLIBC_MAX) toolchain image (shared: glibc-gate, docker-stream)
	docker build -f $(GLIBC_DOCKERFILE) -t $(GLIBC_IMAGE) deploy/docker

glibc-gate: glibc-image ## Build in a glibc $(GLIBC_MAX) container, then run glibc-check on it
# Run as the caller, not root: the build tree lands in the bind-mounted
# checkout, and a root-owned build-glibc228/ is one `make clean` away from
# needing sudo. One `make` invocation, three goals — command-line overrides
# propagate to the sub-makes `test-examples` spawns.
	docker run --rm -u $$(id -u):$$(id -g) \
	    -v "$(CURDIR)":/w -w /w $(GLIBC_IMAGE) \
	    make build test-examples glibc-check \
	        BUILD_DIR=$(GLIBC_BUILD_DIR) \
	        STANDALONE_BUILD_DIR=$(GLIBC_BUILD_DIR)/standalone
# Repoint the compilation database at the normal build tree. The isolation
# above is about the CMake cache; this is the OTHER thing a BUILD_DIR override
# leaks into the checkout, and it was missed. `build` depends on the
# `compile_commands.json` target, whose recipe is
# `ln -sfn $(BUILD_DIR)/compile_commands.json $@` — so the sub-make above
# repoints the repo-root symlink at build-glibc228/ and LEAVES it there.
#
# What that database contains is the problem: it was generated inside the
# container under `-w /w`, so every entry names `/w/native/src/...`. Measured
# after a gate run: 434 entries, and ZERO of them exist on the host.
#
# doppler's own `make lint-clang-tidy` is unaffected — it overrides
# LINT_clang-tidy to pass `-p $(BUILD_DIR)` and takes its file list from
# `git ls-files`, so it never reads this symlink. The consumers that DO are
# clangd (every C file in the editor resolves against a database of paths that
# do not exist) and standard.mk's default `-p .` recipe, which any repo not
# overriding it would hit.
#
# $(BUILD_DIR) here is the OUTER value (the override applies only to the
# sub-make), so this restores whatever the developer actually builds into.
# Unconditional rather than guarded: re-linking to where it already points is
# a no-op, and a conditional would have to re-derive the default it is fixing.
	@ln -sfn $(BUILD_DIR)/compile_commands.json compile_commands.json

# ── The CI toolchain image (gh-885) ──────────────────────────────────────────
# Beside the glibc image above and for the same reason: this is a build
# ENVIRONMENT for gates, not one of the images doppler ships. Every Linux CI
# job used to open by apt-installing the dev group -- ~112 MB per job, ten
# jobs a run, and most of it already on the runner outside dpkg. That download
# was the whole exposure to mirror weather; baking it removes the step.
#
# TWO BASES, and the pair is load-bearing rather than thorough: build-and-test
# runs ubuntu-22.04 and ubuntu-24.04 on purpose. One image for both would
# leave the matrix naming two environments while testing one.
CI_IMAGE_REPO  ?= ghcr.io/doppler-dsp/doppler-ci
CI_IMAGE_BASES ?= ubuntu:22.04 ubuntu:24.04
CI_DOCKERFILE  := deploy/docker/Dockerfile.ci
# The pin. `include`d rather than parsed so make reads the digests directly,
# and `-` so a fresh clone before the first publish is a clear gate failure
# rather than a parse error.
CI_IMAGE_PIN   := .github/ci-images.env
-include $(CI_IMAGE_PIN)

# What the pinned image was built FROM. Hashing the two inputs is what makes
# the gate offline and instant: it answers "has anyone changed the dependency
# list or the image recipe since this digest was taken", which is the drift a
# developer can actually cause. Whether UPSTREAM packages moved is a different
# question, and the nightly rebuild in ci-image.yml is what asks it -- it
# compares the package fingerprint baked into the image, not this.
# Make the cache's effect VISIBLE. CI calls this after its build steps, so
# the hit rate is in the log rather than being an assumption about a tool
# nobody can see working -- if the cache silently stopped hitting, the only
# other symptom would be builds slowly getting longer.
ccache-stats: ## Print compiler-cache hit statistics (no-op without ccache)
	@if [ -n "$(CCACHE_BIN)" ]; then \
	    $(CCACHE_BIN) --show-stats --verbose 2>/dev/null \
	        || $(CCACHE_BIN) --show-stats; \
	 else \
	    echo "ccache-stats: ccache not installed — builds are uncached"; \
	 fi

# Plain `python3`, not `$(UV) run python`, and deliberately: the script is
# stdlib-only, and ci-image.yml shells out to THIS target so the workflow and
# the check cannot compute different numbers. That workflow builds the image
# the rest of CI runs inside, so it is the one place that cannot assume a
# synced uv environment -- it has neither a setup-python nor a uv install step.
# Nothing is unpinned by this: `uv run` pins dependencies, and this script has
# none.
ci-image-source-hash: ## Print the hash of the CI image's inputs (plumbing)
	@python3 scripts/ci_image_source_hash.py

ci-image: ## Build the CI toolchain image locally, one per base
	@for b in $(CI_IMAGE_BASES); do \
	     tag="doppler-ci:$$(echo $$b | tr ':' '-')"; \
	     echo "=== $$b -> $$tag"; \
	     docker build -f $(CI_DOCKERFILE) --build-arg BASE=$$b -t "$$tag" . \
	         || exit 1; \
	 done

# Run it like CI runs it, in the SAME image CI pins -- by digest, not by a
# local tag that happens to share a name. This is the payoff of baking the
# toolchain: "works on my machine" and "works in CI" become the same
# sentence, and today's two failures are both cases it would have caught
# before a push. `make test-rust` died in CI on a cargo old enough to reject
# the repo's own lockfile, invisible on a box carrying rustup; and a build
# tree from THIS host, handed to the container, failed to link with
# atan2f@GLIBC_2.43 because the host's glibc is newer than the image's.
#
# Hence CI_BUILD_DIR, and it is not a nicety: the container and the host
# produce objects for different glibcs, so sharing one build/ is how you get
# a link error that looks like a code bug. Same separation, same reason, as
# glibc-gate's build-glibc228.
CI_IMAGE     ?= $(CI_IMAGE_2404)
CI_BUILD_DIR ?= build-ci
CI_DOCKER_RUN = docker run --rm -u $$(id -u):$$(id -g) \
                    -v "$(CURDIR)":/w -w /w

ci-shell: ## Interactive shell in the PINNED CI image, checkout at /w
	@docker run --rm -it -u $$(id -u):$$(id -g) \
	    -v "$(CURDIR)":/w -w /w $(CI_IMAGE) bash

ci-run: ## Run `make TARGET=<goals>` inside the PINNED CI image
	@if [ -z "$(TARGET)" ]; then \
	    echo "ci-run: name what to run, e.g."; \
	    echo "    make ci-run TARGET='build test-rust'"; \
	    echo "    make ci-run TARGET=lint"; \
	    exit 2; \
	 fi
	$(CI_DOCKER_RUN) -e DOPPLER_BUILD_DIR=/w/$(CI_BUILD_DIR) $(CI_IMAGE) \
	    make $(TARGET) BUILD_DIR=$(CI_BUILD_DIR) \
	        STANDALONE_BUILD_DIR=$(CI_BUILD_DIR)/standalone
# DOPPLER_BUILD_DIR as well as BUILD_DIR, because they are read by different
# consumers and missing the second one fails in a way that reads as a code
# bug: ffi/rust/build.rs locates the library itself, defaulting to ../../build
# -- the HOST tree -- so `ci-run TARGET=test-rust` linked this box's
# glibc-2.43 .so inside a 2.39 container and died on
# `undefined reference: atan2f@GLIBC_2.43`. Pointing both at the container
# tree is what makes "run it like CI" true rather than nearly true.
# Repoint the compilation database at the HOST build tree, for the same
# reason glibc-gate does: `build` symlinks it at $(BUILD_DIR), so a container
# run leaves the repo root pointing into $(CI_BUILD_DIR), whose every entry
# names /w/... — paths that do not exist here, which breaks clangd silently.
	@ln -sfn $(BUILD_DIR)/compile_commands.json compile_commands.json

# The composition a developer actually wants before pushing: the gate set
# CI runs, in the environment CI runs it in. `gates` is already "every gate CI
# runs" (gates-check enforces that against ci.yml); this is that list with the
# environment question removed too.
#
# Deliberately NOT a git pre-push hook. `gates` includes `coverage`, which is
# ~10 minutes -- a hook that slow is one people pass --no-verify to, and a
# gate routinely bypassed is decoration. It is a target you run when you mean
# it, and CI remains the thing that is not optional.
ci-gates: ## Run the full gate set inside the PINNED CI image (pre-push check)
	@$(MAKE) --no-print-directory ci-run TARGET=gates

ci-image-shell: ## A shell in the LOCALLY BUILT CI image (see ci-image)
	@docker run --rm -it -u $$(id -u):$$(id -g) \
	    -v "$(CURDIR)":/w -w /w doppler-ci:ubuntu-24.04 bash

# The gate. Two questions, both answerable with no network and no docker:
#
#   1. Do the workflows and the pin file agree? A `container:` naming anything
#      other than a pinned digest is how a run stops being reproducible --
#      including a tag, which is mutable by definition.
#   2. Is the pin still describing the current inputs? bootstrap.toml gaining
#      a package with no rebuild means CI provisions an environment the repo
#      no longer describes, and the failure would land later, somewhere else,
#      as a missing tool.
ci-image-check: ## Fail when the pinned CI image no longer matches its inputs
	@rc=0; \
	 if [ ! -f "$(CI_IMAGE_PIN)" ]; then \
	     echo "ci-image-check: $(CI_IMAGE_PIN) is missing — the CI image has"; \
	     echo "  never been published, or the pin was deleted. Run the"; \
	     echo "  ci-image workflow and commit the pin it prints."; \
	     exit 1; \
	 fi; \
	 have=$$($(MAKE) -s ci-image-source-hash); \
	 if [ "$$have" != "$(CI_IMAGE_SOURCE_HASH)" ]; then \
	     echo "ci-image-check: the CI image's inputs changed since the"; \
	     echo "  pinned image was built (bootstrap.toml's package/tool"; \
	     echo "  tables, or $(CI_DOCKERFILE) -- NOT [project], which"; \
	     echo "  no layer reads; see scripts/ci_image_source_hash.py)."; \
	     echo "    pinned inputs: $(CI_IMAGE_SOURCE_HASH)"; \
	     echo "    this tree:     $$have"; \
	     echo "  Push the branch (ci-image.yml builds on those paths), then"; \
	     echo "  commit the pin block it prints into $(CI_IMAGE_PIN)."; \
	     rc=1; \
	 fi; \
	 refs=$$(grep -rhoE '^[[:space:]]*image:[[:space:]]*\S+' \
	     .github/workflows/*.yml | awk '{print $$2}' | sort -u); \
	 for r in $$refs; do \
	     case "$$r" in \
	     *'$${{'*) \
	         : "A workflow expression rather than a literal ref. What it" ; \
	         : "resolves to is a matrix image: line, which this same scan" ; \
	         : "sees and checks as a literal -- so the pin is still gated," ; \
	         : "and a per-leg image stays expressible." ; \
	         continue;; \
	     esac; \
	     case "$$r" in \
	     *@sha256:*) ;; \
	     *) echo "ci-image-check: a container: names '$$r', which is not a"; \
	        echo "  digest — a tag is mutable, so the image a PR passed on"; \
	        echo "  need not be the one it merges with."; \
	        rc=1;; \
	     esac; \
	     case "$$r" in \
	     $(CI_IMAGE_2204)|$(CI_IMAGE_2404)) ;; \
	     *@sha256:*) echo "ci-image-check: '$$r' is pinned but is not one of"; \
	                 echo "  the two digests in $(CI_IMAGE_PIN)."; \
	                 rc=1;; \
	     esac; \
	 done; \
	 [ $$rc -eq 0 ] || exit 1; \
	 echo "ci-image-check: OK — pin matches its inputs;" \
	      "$$(echo "$$refs" | grep -c . ) container ref(s), all digest-pinned"

# The recorded specan demo frames are a projection of the specan source, so a
# change to one without the other ships a demo that no longer matches the code.
SPECAN_BASE ?= HEAD^
# The frames are a projection of the C core as much as of the Python app, and
# this watched only the Python. So `native/src/specan/` could change the very
# numbers in frames.json with the gate reporting `specan=0` — which is exactly
# what happened: the committed frames were recorded at 1601 display bins and
# the code now produces 801, a drift nothing reported because the divergence
# arrived through the C side. A gate that watches half its input is a gate you
# believe when it says OK.
SPECAN_SRC_PATHS = src/specan/doppler_specan/ native/src/specan/ \
                   native/inc/specan/
specan-check: ## Fail if specan changed without re-recording its demo frames
	@s=$$(git diff --name-only "$(SPECAN_BASE)"...HEAD -- $(SPECAN_SRC_PATHS) | wc -l); \
	 f=$$(git diff --name-only "$(SPECAN_BASE)"...HEAD -- docs/specan/frames.json | wc -l); \
	 if [ "$$s" -gt 0 ] && [ "$$f" -eq 0 ]; then \
	     echo "specan-check: specan source changed but"; \
	     echo "  docs/specan/frames.json was not — run 'make record-demo'"; \
	     echo "  watched: $(SPECAN_SRC_PATHS)"; \
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

# Parallel by default, for the same reason `test-python` is: the workload is
# embarrassingly parallel and pytest-xdist is already a dev dependency. Each
# example runs as its own SUBPROCESS in a throwaway cwd with MPLBACKEND=Agg and
# stdin closed (see test_examples.py), so there is no shared state for workers
# to collide over — unlike the benchmark suite, which must stay serial because
# pytest-benchmark disables itself under xdist, and unlike `test-stubs`, where
# a text-mode .pyi shares one doctest namespace across the file.
#
# Measured on 8 cores, and only one half of the win is parallelism:
#
#   376.6 s  before — three Monte-Carlo sweeps dominated the gate
#    68.8 s  after moving them to `make characterize` (5.5x, still serial)
#    ~13  s  with -n auto as well; 20.6 s for this whole target, downstream
#            example included
#
# The two halves are worth keeping separate because the first one is where the
# thinking was: 164.6 s + 117.7 s + 18.9 s of sweep against ~58 s for every
# other example combined. No amount of parallelism fixes a gate whose cost is
# one item, and Amdahl said so — with the biggest sweep still present the floor
# was its own 164.6 s.
#
# What is left is the shape xdist is actually good at: the longest single
# example is now `detector2d_acq_demo.py` at ~6.2 s, with a smooth tail behind
# it (5.6, 5.3, 4.0, 3.0 …), so no one item sets the floor. Sublinear scaling
# is expected and not worth chasing — several examples drive Plan.prepare()'s
# own pthread parallel-for, so workers oversubscribe by design.
#
# TWO passes, and the split is the point — exactly as `test-python` splits out
# its benchmark directories, for the same reason. `ddc_fn_scaling.py` asserts a
# 2-thread speedup (`su2 > 1.25`) to prove `execute()` releases the GIL; under
# xdist the workers already own every core, so it measured **1.15x** and read
# the contention as "execute appears GIL-bound". A gate must not fail for
# running, and equally must not be weakened to fit its harness, so the examples
# whose assertion IS a timing get a second serial pass. Which ones is declared
# in `src/doppler/examples/.examples-serial` (reasons mandatory), and the
# `examples_serial` marker is applied from that registry — so neither pass
# names a script and the registry stays the only place a name appears.
#
# Override with PYTEST_ARGS="-n 0" to force serial, matching `test-python`.
# test_c_example_pairs.py runs here rather than under test-examples-c for
# two reasons that are both about this being the only place it can actually
# run: the C examples it drives need a live broker AND the built binaries,
# and this is the gate CI reaches with both (test-examples-c also runs in
# the macOS build job, where there is no nats-server to start). It
# self-skips without either.
test-examples-python: ## Run the Python example gate (requires pyext)
	uv run pytest -m "examples and not examples_serial" -q -n auto \
	    $(PYTEST_ARGS) src/doppler/tests/test_examples.py \
	    src/doppler/tests/test_c_example_pairs.py
	@# Exit 5 is pytest's "no tests collected", which is what an EMPTY
	@# .examples-serial produces -- every item deselected. That is the
	@# registry doing its job (the constraint was fixed and the line
	@# deleted), so it must not read as a failure: this target would then go
	@# red for the correction, which is the same "a gate must not fail for
	@# running" rule the serial pass exists to honour. Measured, not
	@# assumed -- a marker expression selecting nothing exits 5 here.
	@set +e; \
	 uv run pytest -m "examples and examples_serial" -q \
	     $(PYTEST_ARGS) src/doppler/tests/test_examples.py; \
	 rc=$$?; \
	 if [ $$rc -eq 5 ]; then \
	     echo "test-examples-python: .examples-serial is empty — nothing needs a serial pass"; \
	 elif [ $$rc -ne 0 ]; then exit $$rc; fi
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
# The Python microbenchmarks, MEASURED -- serially, because pytest-benchmark
# disables itself under xdist and a number it refuses to produce is better
# than one taken on eight busy cores.
#
# It is its own target because it is its own activity: `make test-python` runs
# constantly and runs these files as TESTS (xdist, no timing); this runs
# occasionally and is the only thing that times them. Folding the two put a
# 139s measurement inside every test run, six times over in CI, feeding
# nothing -- see TEST_PYTHON_CMD.
bench-python: ## Time the Python microbenchmarks (serial; pytest-benchmark)
	uv run pytest $(PYTEST_BENCH_DIRS) -v $(PYTEST_SELECT) $(PYTEST_ARGS)

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

# Depends on glibc-image because this is the one image whose binaries RUN on a
# different base than the one that compiled them, so they are built at the
# floor (see Dockerfile.examples' stream-build stage). That dependency is also
# why `docker compose up` on a clean checkout cannot be the entry point for
# this image: the FROM in stream-build names a local tag nothing pulls.
# docker-compose.yml says so at the top.
docker-stream: glibc-image ## Build+smoke the lean compose streaming-services image
	docker build -f $(EXAMPLES_DOCKERFILE) --target stream-services \
	    --build-arg GLIBC_BASE=$(GLIBC_IMAGE) \
	    -t $(DOCKER_IMAGE)-stream-services:$(DOCKER_TAG) .
	bash scripts/smoke-image.sh stream \
	    $(DOCKER_IMAGE)-stream-services:$(DOCKER_TAG)

docker-examples: docker-sdk docker-downstream docker-stream ## Build+smoke all build-on-doppler images
	@echo "All build-on-doppler images built and smoked."

# The targets above smoke a LOCAL single-arch `:dev` image; release.yml smokes
# the PUSHED multi-arch one, once per architecture. That per-arch loop was
# written out three times in release.yml (runtime / sdk / downstream),
# identical but for the kind and the image ref — the same hand-copying that
# this file's own comments record twice: the release SDK check silently
# degrading to a version print, and the #601 quoting SyntaxError. Both lived in
# a re-typed copy, so the loop is now typed once too.
SMOKE_ARCHES ?= amd64 arm64

smoke-image: ## KIND=<runtime|sdk|downstream|stream> IMAGE=<ref> — smoke it on $(SMOKE_ARCHES)
ifndef KIND
	@echo "usage: make smoke-image KIND=<kind> IMAGE=<ref>"; exit 1
endif
ifndef IMAGE
	@echo "usage: make smoke-image KIND=<kind> IMAGE=<ref>"; exit 1
endif
	@for a in $(SMOKE_ARCHES); do \
	     bash scripts/smoke-image.sh $(KIND) "$(IMAGE)" "linux/$$a" || exit 1; \
	 done
