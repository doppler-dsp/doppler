# standard.mk — the shared Makefile for just-buildit and doppler-dsp repos.
#
# Canonical copy:  https://just-buildit.github.io/standard.mk
# Design RFC:      doppler-dsp/doppler#555
# Plan + criteria: just-buildit/.github README, "Makefile standard"
#
# This file is VENDORED VERBATIM and never edited in place. `make lint` runs
# the drift gate (`standard-check`), which fetches the canonical copy and fails
# on any difference. Per-repo variation is expressed as CONFIGURATION — the
# variables below, set in the repo's own Makefile before `include standard.mk`
# — never as a local edit, because a local edit is the fork this exists to
# prevent.
#
# The contract:
#
#   Makefile                configuration only: feature flags, command
#                           variables, `include standard.mk`, and any target
#                           genuinely local to the repo.
#   standard.mk             the shared targets (this file).
#   local.mk                optional, auto-included; may only ADD targets,
#                           never redefine a standard one.
#   pyproject.toml          WHICH tools, at WHAT versions.
#   uv.lock                 pins them, committed.
#   .pre-commit-config.yaml WHEN a check fires; dispatches to `make -s
#                           lint-<tool>` so it cannot drift from `make format`.
#   jb.toml                 system packages, consumed by `install-deps`.
#   .github/workflows       calls `make <target>`; anything else must be
#                           provably environment plumbing.
#
# Two rules keep the whole thing honest, and both are gates rather than
# review, because none of the drift this replaces was decided — it accumulated:
#
#   * `help` is GENERATED. Every active target carries a `## description` on
#     its rule line, and `help-check` fails if one is missing or if `help`
#     would advertise something that does not exist.
#   * `.PHONY` is derived from `STD_TARGETS`, and `ghost-check` fails if a
#     declared target has no recipe behind it — the failure mode where `make
#     wheel` exits 0 having done nothing.
#
# ── Configuration reference ──────────────────────────────────────────────────
#
# Feature flags (set to 1 to enable; unset means the group is not defined at
# all, so its targets do not exist and `help` does not list them):
#
#   HAS_C HAS_PYTHON HAS_RUST HAS_DOCS HAS_DOXYGEN HAS_BENCH HAS_COVERAGE
#   HAS_RELEASE HAS_EXAMPLES
#
# A command variable either has a universally correct default (`TEST_RUST_CMD`
# is `cargo test`) or is REQUIRED once its flag is on — see "Required
# configuration" below for why that is a parse-time error rather than a gate.
# Genuinely optional hooks (`DOCS_PREPARE`, `DOCS_CHECK_CMD`,
# `RELEASE_BRANCH_NOTES`) default to empty and simply do nothing.

# ── Preconditions ────────────────────────────────────────────────────────────

# GNU make only, 3.81 or newer. `.FEATURES` arrived in 3.81 and exists in no
# BSD make, so its absence is the probe. Checked because the failure otherwise
# surfaces as an inscrutable syntax error hundreds of lines away.
#
# 3.81 specifically, NOT 3.82: that is what macOS still ships as `make`, so a
# probe for anything newer rejects every macOS CI runner. It cost a red matrix
# to learn — `.RECIPEPREFIX` (3.82+) was the first probe here.
ifeq ($(origin .FEATURES),undefined)
$(error standard.mk requires GNU make 3.81+ (try gmake))
endif

# POSIX sh, so no recipe may rely on a bashism. Overridable, but only
# deliberately: make always has SHELL defined, so `?=` here would never fire.
STD_SHELL ?= /bin/sh
SHELL      = $(STD_SHELL)

# The vendored file the drift gate compares, and where it compares against.
#
# The URL is the DEFAULT, not something each adopter opts into, because the
# opt-in version fails open: a repo that vendors this file and forgets the one
# line has no drift protection at all, and says so only as a cheerful "inert"
# notice that reads like a pass. Armed on vendoring is the only default that
# cannot be silently skipped.
#
# A repo can still opt out deliberately with `STANDARD_URL =` in its Makefile,
# which is greppable across the org — unlike an omission.
STANDARD_FILE ?= standard.mk
STANDARD_URL  ?= https://just-buildit.github.io/standard.mk

# ── Tooling ──────────────────────────────────────────────────────────────────
# The ONLY place a tool binary is named. Versions live in pyproject.toml's dev
# group and are pinned by uv.lock; humans, hooks and CI all reach the tools
# through the targets below, so a flag change is a one-line edit rather than a
# hunt through the Makefile, the hook config and the workflows.
#
# Corollary: never invoke a linter with `uvx` or a global install. `uvx ruff`
# resolves to whatever released today, which formats differently from the
# pinned ruff and silently rewrites unrelated files.
UV         ?= uv
DEV_RUN    ?= $(UV) run --group dev
PRE_COMMIT ?= $(DEV_RUN) pre-commit
SYNC_CMD   ?= $(UV) sync

# ── Required configuration ───────────────────────────────────────────────────
# A flag turns a group ON, which makes that group's commands mandatory.
#
# This is a parse-time error rather than a gate because an empty command is
# invisible to every gate: it stamps out a target with an EMPTY recipe, and
# make records that as having a recipe, so ghost-check sees a healthy target
# and `help` advertises it — while `make <it>` exits 0 having done nothing.
# That is the exact shape of the bug this standard exists to end, so the
# standard must not be able to reintroduce it through configuration.
_std_require = $(if $(strip $($(1))),,$(error $(2) is on, but $(1) is empty \
    — set it in the Makefile, or turn $(2) off))

# ── Universal command variables ──────────────────────────────────────────────

# The table's rule: the suite, or the build where there is native code.
ALL_DEPS       ?= $(if $(filter 1,$(HAS_C)),build,test)
TEST_CMD       ?=
TEST_FAST_CMD  ?=
CLEAN_PATHS    ?=
CLEAN_CMD      ?=

# `lint-<tool>` dispatch. LINT_TOOLS names the tools; LINT_<tool> holds the
# command (a `define` block for anything multi-line). The rules are stamped out
# below, so adding a tool is one entry plus one variable, in the repo Makefile,
# with the hook config dispatching inward to the target that results.
LINT_TOOLS   ?=
# Which of those to run, in order, for `format`. Separate from LINT_TOOLS
# because a checker that cannot fix anything has no business in `format`.
FORMAT_TOOLS ?= $(LINT_TOOLS)

# Aggregates. TEST_ALL_DEPS and GATES_DEPS are prerequisite lists, so a repo
# extends them by naming targets rather than by copying recipes.
TEST_ALL_DEPS ?= test
GATES_DEPS    ?= lint test-all

$(call _std_require,TEST_CMD,every repo)
$(call _std_require,TEST_FAST_CMD,every repo)
# `clean` needs at least one of the two to be doing anything at all.
ifeq ($(strip $(CLEAN_PATHS)$(CLEAN_CMD)),)
$(error neither CLEAN_PATHS nor CLEAN_CMD is set, so `make clean` is a no-op)
endif

# ── Universal targets ────────────────────────────────────────────────────────

STD_TARGETS += all help setup clean test test-fast lint format install-deps

.DEFAULT_GOAL := all

all: $(ALL_DEPS) ## Default goal

test: ## Run the default test suite
	$(TEST_CMD)

test-fast: ## Run tests, stopping at the first failure
	$(TEST_FAST_CMD)

# `lint` is the gate — CI runs exactly this and nothing else. The three
# consistency gates come first because they are near-free and catch the class
# of rot that review demonstrably does not.
lint: standard-check help-check ghost-check ## Run the full lint gate (CI runs this)
	@hook=$$(git rev-parse --git-path hooks/pre-commit 2>/dev/null); \
	 if [ -n "$$hook" ] && [ ! -f "$$hook" ]; then \
	     $(PRE_COMMIT) install >/dev/null 2>&1 \
	         || echo "note: git hook not installed (continuing with lint)"; \
	 fi
	$(PRE_COMMIT) run --all-files

# The fixer. Runs the tools sequentially via recursive make rather than as
# prerequisites, because prerequisite order is only guaranteed without -j and
# formatters are order-dependent (a reformat can invalidate a fix).
format: ## Auto-fix formatting with every configured formatter
	@for t in $(FORMAT_TOOLS); do $(MAKE) -s lint-$$t || exit 1; done

# System packages, from jb.toml. A repo that declares none still gets a
# working target — `jbx install-deps` is a no-op there.
install-deps: ## Install system build dependencies (jb.toml)
	@command -v jbx >/dev/null 2>&1 \
	    || curl -sSL https://just-buildit.github.io/get-jb.sh | bash
	PATH="$$HOME/.local/bin:$$PATH" jbx install-deps

# Project dependencies plus the git hook. There is deliberately no second
# deps-ish target: `install` was a strict subset of this and has been folded in.
setup: ## One-time per clone: sync dependencies + install the git hook
	$(SYNC_CMD)
	$(PRE_COMMIT) install

clean: ## Remove build artifacts
	$(if $(CLEAN_PATHS),rm -rf $(CLEAN_PATHS))
	$(CLEAN_CMD)

# ── lint-<tool> dispatch ─────────────────────────────────────────────────────
# One rule per configured tool. The hook config calls `make -s lint-<tool>`,
# so the hook and `make format` cannot disagree about how a tool runs — which
# is what closes the "same command, different environment" class of drift.
#
# `$$(LINT_$(1))` is expanded when the rule runs, so a `define`d multi-line
# command works as a canned recipe.
define _std_lint_rule
STD_TARGETS += lint-$(1)
lint-$(1):
	$$(LINT_$(1))
endef
$(foreach _t,$(LINT_TOOLS),$(eval $(call _std_lint_rule,$(_t))))

# ── Aggregates ───────────────────────────────────────────────────────────────

STD_TARGETS += test-all gates

test-all: $(TEST_ALL_DEPS) ## Run every test suite in the repo

gates: $(GATES_DEPS) ## Run every gate that guards a merge
	@echo ""
	@echo "gates: ALL PASS"

# ── HAS_C ────────────────────────────────────────────────────────────────────
# `release` is RESERVED for the C build type. The release workflow is `ship`
# and `tag-release`, so the two senses never collide.
ifeq ($(HAS_C),1)
STD_TARGETS += build debug release

BUILD_DIR     ?= build
BUILD_TYPE    ?= RelWithDebInfo
CMAKE         ?= cmake
CMAKE_FLAGS   ?=
PYEXT_CMD     ?=

# The in-place extension build belongs to the OVERLAP of the two flags, not to
# either alone: a C repo with no Python bindings has no extension to build.
ifeq ($(HAS_PYTHON),1)
STD_TARGETS += pyext
$(call _std_require,PYEXT_CMD,HAS_C=1 with HAS_PYTHON=1)
endif

build: ## Configure and build the native library
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	    $(CMAKE_FLAGS)
	$(CMAKE) --build $(BUILD_DIR)

debug: ## Build the native library with BUILD_TYPE=Debug
	@$(MAKE) build BUILD_TYPE=Debug

release: ## Build the native library clean, with BUILD_TYPE=Release
	@$(MAKE) clean
	@$(MAKE) build BUILD_TYPE=Release

ifeq ($(HAS_PYTHON),1)
pyext: ## Build the Python extension in place
	$(PYEXT_CMD)
endif
endif

# ── HAS_PYTHON ───────────────────────────────────────────────────────────────
# `wheel`, not `build`: in a repo with C the noun `build` is the native build,
# and one standard target may not mean two things (criterion 9).
ifeq ($(HAS_PYTHON),1)
STD_TARGETS += wheel test-python

WHEEL_CMD       ?= $(UV) build --wheel
TEST_PYTHON_CMD ?= $(TEST_CMD)

wheel: ## Build a wheel into dist/
	$(WHEEL_CMD)
	@echo ""
	@ls -lh dist/*.whl

test-python: ## Run the Python test suite
	$(TEST_PYTHON_CMD)
endif

# ── HAS_RUST ─────────────────────────────────────────────────────────────────
ifeq ($(HAS_RUST),1)
STD_TARGETS += test-rust

TEST_RUST_CMD ?= cargo test

test-rust: ## Run the Rust test suite
	$(TEST_RUST_CMD)
endif

# ── HAS_DOCS ─────────────────────────────────────────────────────────────────
# `docs-check` is the pre-push gate: a strict build (which catches broken
# anchors that the test suite does not) plus whatever invariants the repo
# checks. It exists so `zensical build --strict` has exactly ONE implementation
# — CI calls this target rather than carrying its own copy of the command.
ifeq ($(HAS_DOCS),1)
STD_TARGETS += docs docs-serve docs-check

ZENSICAL        ?= $(DEV_RUN) zensical
DOCS_PREPARE    ?=
DOCS_BUILD_CMD  ?= $(ZENSICAL) build --clean --strict
DOCS_SERVE_CMD  ?= $(ZENSICAL) serve
# The strict build for `docs-check`, as a variable so CI calls this target
# instead of carrying its own copy of the command — the duplication that put
# `zensical build --strict` in three disagreeing places (criterion 5).
#
# Exported, not interpolated into the recipe: a value containing quotes
# would otherwise be re-parsed by the shell and break. make hands it over.
DOCS_CHECK_BUILD_CMD ?= $(ZENSICAL) build --strict
export DOCS_CHECK_BUILD_CMD

# Gates that run around the build, in order, one shell command per line.
# PRE runs before it, POST after — `check_site_links`-shaped checks need a
# built site, cheap script gates want to fail before you pay for a build.
#
# They ACCUMULATE: every command runs, every failure is reported, and the
# target fails at the end if any did. A gate that stops at the first failure
# lets a later red hide behind an earlier one, so you fix one thing, push, and
# discover the next — which is how a docs gate teaches people to stop running
# it locally.
DOCS_CHECK_PRE_CMDS  ?=
DOCS_CHECK_POST_CMDS ?=
export DOCS_CHECK_PRE_CMDS
export DOCS_CHECK_POST_CMDS

# Legacy: a single canned recipe run after everything above. Superseded by
# DOCS_CHECK_POST_CMDS, and kept because it may be a `define` beginning with a
# recipe prefix (`@`), which only works as a recipe line and cannot be folded
# into the accumulator. It runs only if the accumulator passed; migrate to
# POST to get accumulate-don't-abort for it too.
DOCS_CHECK_CMD  ?=

docs: ## Build the docs site
	$(DOCS_PREPARE)
	$(DOCS_BUILD_CMD)

docs-serve: ## Build and serve the docs with live reload
	$(DOCS_PREPARE)
	$(DOCS_SERVE_CMD)

docs-check: ## Pre-push docs gate: strict build + docs invariants
	@echo "Docs gate: strict build (broken anchors) + docs invariants..."
	$(DOCS_PREPARE)
	@tmp=$$($(_STD_TMP)); trap 'rm -f "$$tmp"' EXIT; fail=0; \
	 run() { echo "=== $$1 ==="; sh -c "$$1" || fail=1; }; \
	 printf '%s\n' "$$DOCS_CHECK_PRE_CMDS" >"$$tmp"; \
	 while IFS= read -r c; do [ -n "$$c" ] && run "$$c"; done <"$$tmp"; \
	 run "$$DOCS_CHECK_BUILD_CMD"; \
	 printf '%s\n' "$$DOCS_CHECK_POST_CMDS" >"$$tmp"; \
	 while IFS= read -r c; do [ -n "$$c" ] && run "$$c"; done <"$$tmp"; \
	 if [ "$$fail" != 0 ]; then \
	     echo "docs-check: FAILURES above — every gate ran, all reported"; \
	     exit 1; \
	 fi
	$(DOCS_CHECK_CMD)
endif

# ── HAS_DOXYGEN ──────────────────────────────────────────────────────────────
ifeq ($(HAS_DOXYGEN),1)
STD_TARGETS += doxygen doxygen-check

DOXYGEN_CMD       ?= doxygen
DOXYGEN_CHECK_CMD ?=

$(call _std_require,DOXYGEN_CHECK_CMD,HAS_DOXYGEN)

doxygen: ## Build the C API documentation
	$(DOXYGEN_CMD)

doxygen-check: ## Fail on any Doxygen warning
	$(DOXYGEN_CHECK_CMD)
endif

# ── HAS_BENCH ────────────────────────────────────────────────────────────────
ifeq ($(HAS_BENCH),1)
STD_TARGETS += bench bench-save bench-compare

BENCH_CMD         ?=
BENCH_SAVE_CMD    ?=
BENCH_COMPARE_CMD ?=

$(call _std_require,BENCH_CMD,HAS_BENCH)
$(call _std_require,BENCH_SAVE_CMD,HAS_BENCH)
$(call _std_require,BENCH_COMPARE_CMD,HAS_BENCH)

bench: ## Run the benchmarks
	$(BENCH_CMD)

bench-save: ## Save a benchmark baseline
	$(BENCH_SAVE_CMD)

bench-compare: ## Compare against the last saved baseline
	$(BENCH_COMPARE_CMD)
endif

# ── HAS_COVERAGE ─────────────────────────────────────────────────────────────
ifeq ($(HAS_COVERAGE),1)
STD_TARGETS += coverage coverage-gate

COVERAGE_CMD      ?=
COVERAGE_GATE_CMD ?=

$(call _std_require,COVERAGE_CMD,HAS_COVERAGE)
$(call _std_require,COVERAGE_GATE_CMD,HAS_COVERAGE)

coverage: ## Produce a coverage report
	$(COVERAGE_CMD)

# Separate from `coverage` because a report is not a gate. This one fails when
# the threshold is missed, so CI can call exactly that rather than "produce a
# report" and hope someone reads it.
coverage-gate: ## Fail when coverage falls below the threshold
	$(COVERAGE_GATE_CMD)
endif

# ── HAS_RELEASE ──────────────────────────────────────────────────────────────
# The release workflow, in dependency order: release-branch (bump on a branch)
# -> PR -> merge -> tag-release -> release-watch, with `ship` doing the last
# two in one go. `release` is NOT part of this — it is the C build type.
ifeq ($(HAS_RELEASE),1)
STD_TARGETS += bump-version version-check release-branch tag-release \
               release-watch ship

BUMP_VERSION_CMD  ?=
RELEASE_WATCH_CMD ?=
# Newline-separated `label|command` pairs; each command prints a version
# string. `version-check` requires every probe to agree — and to equal
# VERSION= when one is given. Exported so the recipe's shell can read it.
VERSION_PROBES ?=
export VERSION_PROBES
# Extra guidance echoed after `release-branch`, repo-specific by nature.
RELEASE_BRANCH_NOTES ?=

$(call _std_require,BUMP_VERSION_CMD,HAS_RELEASE)
$(call _std_require,VERSION_PROBES,HAS_RELEASE)
$(call _std_require,RELEASE_WATCH_CMD,HAS_RELEASE)

bump-version: ## VERSION=x.y.z — write the version into the repo's manifests
ifndef VERSION
	@echo "usage: make bump-version VERSION=<x.y.z>"
	@exit 1
endif
	$(BUMP_VERSION_CMD)
	@echo "Bumped to $(VERSION)"

# Two jobs in one target, because they are the same question: do the manifests
# agree with each other, and (when asked) with the version being released?
# doppler answered only the first and just-makeit only the second under the
# same name, which is exactly the divergence this standard exists to end.
version-check: ## [VERSION=x.y.z] Verify version strings agree
	@printf '%s\n' "$$VERSION_PROBES" | grep -v '^[[:space:]]*$$' \
	 | while IFS='|' read -r label cmd; do \
	     got=$$(eval "$$cmd"); \
	     printf '  %-24s %s\n' "$$label" "$$got"; \
	     if [ -n "$(VERSION)" ] && [ "$$got" != "$(VERSION)" ]; then \
	         echo "ERROR: $$label has $$got, expected $(VERSION)"; exit 1; \
	     fi; \
	     if [ -n "$$first" ] && [ "$$got" != "$$first" ]; then \
	         echo "ERROR: $$label has $$got, but $$firstlabel has $$first"; \
	         exit 1; \
	     fi; \
	     first=$$got; firstlabel=$$label; \
	 done
	@echo "Version OK"

# The explicit origin/main start point matters: a bare `checkout -b` forks from
# whatever HEAD the invoker happens to be on (a feature branch, a stale main),
# silently building the release on the wrong base — the bump then misses
# everything merged since.
release-branch: ## VERSION=x.y.z — branch off origin/main and bump
ifndef VERSION
	@echo "usage: make release-branch VERSION=<x.y.z>"
	@exit 1
endif
	git fetch origin main
	git checkout -b chore/release-$(VERSION) origin/main
	@$(MAKE) bump-version VERSION=$(VERSION)
	@echo ""
	@echo "Now:"
	@echo "  - edit CHANGELOG.md ([Unreleased] -> [$(VERSION)])"
	$(RELEASE_BRANCH_NOTES)
	@echo "  - git commit -am 'chore: release v$(VERSION)', push, open a PR"
	@echo "  - merge once green, then: git checkout main && git pull &&"
	@echo "    make ship VERSION=$(VERSION)"

# Tags an already-merged, CI-green main commit and pushes ONLY the tag. main is
# never pushed to directly, so the tag always points at code the required
# checks already passed.
tag-release: ## VERSION=x.y.z — tag merged main and push the tag
ifndef VERSION
	@echo "usage: make tag-release VERSION=<x.y.z>"
	@exit 1
endif
	@test "$$(git rev-parse --abbrev-ref HEAD)" = "main" || \
	    { echo "ERROR: not on main — tags only point at merged main"; exit 1; }
	@git fetch --quiet origin main
	@test "$$(git rev-parse HEAD)" = "$$(git rev-parse origin/main)" || \
	    { echo "ERROR: local main != origin/main — git pull first"; exit 1; }
	@$(MAKE) version-check VERSION=$(VERSION)
	git tag -a "v$(VERSION)" -m "Release v$(VERSION)"
	git push origin "v$(VERSION)"
	@echo "Tagged v$(VERSION) — release workflow starting on GitHub"

release-watch: ## VERSION=x.y.z — watch the release workflow and verify it
ifndef VERSION
	@echo "usage: make release-watch VERSION=<x.y.z>"
	@exit 1
endif
	$(RELEASE_WATCH_CMD)

ship: tag-release release-watch ## VERSION=x.y.z — tag-release then release-watch
endif

# ── HAS_EXAMPLES ─────────────────────────────────────────────────────────────
ifeq ($(HAS_EXAMPLES),1)
STD_TARGETS += test-examples

TEST_EXAMPLES_CMD ?=

$(call _std_require,TEST_EXAMPLES_CMD,HAS_EXAMPLES)

test-examples: ## Run the end-to-end example builds
	$(TEST_EXAMPLES_CMD)
endif

# ── Gates ────────────────────────────────────────────────────────────────────
# Three invariants that review has been shown not to catch, each failing rather
# than warning. A gate that cannot run has not passed.

STD_TARGETS += standard-check help-check ghost-check

# A temp file, portably: bare `mktemp` is a GNU extension, and the BSD one
# macOS ships requires a template. The gates parse make's own database, which
# is far too big to hold in a shell variable comfortably.
_STD_TMP = mktemp "$${TMPDIR:-/tmp}/std.XXXXXX"

# Resolve one target's help text, for both `help` and `help-check`. Defined
# once here so the two can never disagree about what counts as documented.
#
# The `## description` on the rule line is the source. The dispatch rules are
# the one exception: they are stamped out by `$(eval)`, so their source text
# reads `lint-$(1):` and no scrape can find them by name — their description
# comes from the macro that generated them.
_STD_DESC = d=$$(sed -n "s/^$$t:.*\#\# *//p" $(MAKEFILE_LIST) | head -1); \
            case "$$t" in \
                lint-*) [ -n "$$d" ] \
                    || d="Run $${t\#lint-} (pre-commit dispatch target)";; \
            esac

# Drift. Fetches canonical EVERY time, with no cache: a cache would mean the
# most likely failure — the fetch failing while the network is fine (CDN
# outage, bad deploy, a 404 after a rename) — silently degrades into "compared
# against something older", and one bad deploy would disable the drift gate
# across every repo at once with nothing going red.
standard-check: ## Verify the vendored standard.mk matches canonical
	@if [ -z "$(STANDARD_URL)" ]; then \
	    echo "standard-check: OFF — STANDARD_URL is empty, so drift is NOT"; \
	    echo "  checked in this repo. That is a deliberate opt-out, not a"; \
	    echo "  pass; unset it only if you mean it."; \
	    exit 0; \
	fi; \
	tmp=$$(mktemp); \
	if ! curl -fsSL "$(STANDARD_URL)" -o "$$tmp" 2>/dev/null; then \
	    rm -f "$$tmp"; \
	    echo "ERROR: cannot fetch $(STANDARD_URL)"; \
	    echo "  A gate that cannot reach its reference has not passed;"; \
	    echo "  it has not run. Fix the fetch, do not skip the gate."; \
	    exit 1; \
	fi; \
	if ! diff -u "$$tmp" "$(STANDARD_FILE)" >/dev/null 2>&1; then \
	    echo "ERROR: $(STANDARD_FILE) differs from $(STANDARD_URL)"; \
	    diff -u "$$tmp" "$(STANDARD_FILE)" | head -40; \
	    rm -f "$$tmp"; \
	    echo ""; \
	    echo "  standard.mk is vendored verbatim. Per-repo variation is"; \
	    echo "  configuration in the Makefile; change canonical, not this."; \
	    exit 1; \
	fi; \
	rm -f "$$tmp"; \
	echo "standard-check: vendored standard.mk matches canonical"

# Help completeness, both directions: every listed target carries a
# description, and every target that exists is listed.
#
# The second direction is deliberately NOT "does each listed target exist" —
# that check cannot fail, because `.PHONY` alone is enough to put a name in the
# make database, so a name with nothing behind it still looks like a target.
# The case that DOES happen is the reverse: a rule gets added without being
# named in STD_TARGETS/LOCAL_TARGETS, so `help` silently omits it. Whether the
# targets `help` lists actually do anything is ghost-check's job.
help-check: ## Verify help documents every target, and every target is listed
	@rc=0; \
	 db=$$($(_STD_TMP)); trap 'rm -f "$$db"' EXIT; \
	 $(MAKE) -rpn --no-print-directory >"$$db" 2>/dev/null; \
	 for t in $(ALL_TARGETS); do \
	     $(_STD_DESC); \
	     if [ -z "$$d" ]; then \
	         echo "ERROR: '$$t' has no '## description', so help omits it"; \
	         rc=1; \
	     fi; \
	 done; \
	 withrecipe=$$(awk '/^[a-zA-Z0-9_.-]+:/ { n = $$0; sub(/:.*/, "", n); \
	                                          b = 1; r = 0; next } \
	      b && / to execute/ { r = 1 } \
	      b && /^$$/ { if (r) print n; b = 0 } \
	      END { if (b && r) print n }' "$$db" | sort -u); \
	 if [ -z "$$withrecipe" ]; then \
	     echo "ERROR: parsed no rules out of make's database"; \
	     echo "  This half of the gate did not run, so it has not passed."; \
	     rc=1; \
	 fi; \
	 for t in $$withrecipe; do \
	     case " $(ALL_TARGETS) " in *" $$t "*) continue;; esac; \
	     case "$$t" in .*) continue;; esac; \
	     if [ -e "$$t" ]; then continue; fi; \
	     echo "ERROR: '$$t' has a rule but help does not list it"; \
	     echo "  add it to LOCAL_TARGETS (or STD_TARGETS, if it is shared)"; \
	     rc=1; \
	 done; \
	 if [ $$rc -eq 0 ]; then \
	     echo "help-check: $(words $(ALL_TARGETS)) targets documented"; \
	 fi; \
	 exit $$rc

# Ghost targets: declared .PHONY with no recipe AND no prerequisites, so they
# exit 0 having done nothing. doppler shipped `make wheel` in this state
# through a lint gate, CI on every PR, and a help entry advertising it.
#
# The prerequisite half of that test is what keeps the aggregates legitimate:
# `all: test`, `test-all: ...` and `ship: ...` carry no recipe by design and do
# their work entirely through what they depend on.
ghost-check: ## Verify every .PHONY target has a recipe
	@db=$$($(_STD_TMP)); phony=$$($(_STD_TMP)); norecipe=$$($(_STD_TMP)); \
	 trap 'rm -f "$$db" "$$phony" "$$norecipe"' EXIT; \
	 $(MAKE) -rpn --no-print-directory >"$$db" 2>/dev/null; \
	 sed -n 's/^\.PHONY:[ ]*//p' "$$db" | tr ' ' '\n' | sed '/^$$/d' \
	     | sort -u >"$$phony"; \
	 awk '/^[a-zA-Z0-9_.-]+:/ { n = $$0; sub(/:.*/, "", n); \
	                            p = $$0; sub(/^[^:]*:/, "", p); \
	                            b = 1; r = 0; next } \
	      b && / to execute/ { r = 1 } \
	      b && /^$$/ { if (!r && p !~ /[^[:space:]]/) print n; b = 0 } \
	      END { if (b && !r && p !~ /[^[:space:]]/) print n }' "$$db" \
	     | sort -u >"$$norecipe"; \
	 ghosts=$$(comm -12 "$$phony" "$$norecipe"); \
	 if [ -n "$$ghosts" ]; then \
	     echo "ERROR: .PHONY targets with no recipe behind them:"; \
	     printf '  %s\n' $$ghosts; \
	     echo ""; \
	     echo "  These exit 0 having done nothing."; \
	     exit 1; \
	 fi; \
	 echo "ghost-check: no ghost targets"

# ── Help ─────────────────────────────────────────────────────────────────────
# Generated from the `## description` on each active target's rule line. Never
# hand-maintained: a hand-written list is how `make wheel` stayed advertised
# for as long as it did.
help: ## Show this message
	@echo ""
	@echo "  $(notdir $(CURDIR)) — make targets"
	@echo ""
	@for t in $(ALL_TARGETS); do \
	    $(_STD_DESC); \
	    printf '  %-20s %s\n' "$$t" "$$d"; \
	done
	@echo ""

# ── local.mk ─────────────────────────────────────────────────────────────────
# May only ADD targets. Redefining a standard one makes it the fork this file
# exists to prevent — and make would warn about the override anyway.
-include local.mk

# Repo-local targets are named in LOCAL_TARGETS (in the Makefile or local.mk),
# which puts them in `.PHONY` and in `help` — and so under the same gates as
# the standard ones. Criterion 2 is "help lists EVERY target", not "every
# standard target": a local target that help omits is exactly as invisible.
ALL_TARGETS = $(STD_TARGETS) $(LOCAL_TARGETS)

.PHONY: $(ALL_TARGETS)
