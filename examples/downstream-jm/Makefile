# iqtools project Makefile
#
# Start here:
#
#     make setup      create .venv and install what the project declares
#     make test       build, then run the C and Python tests
#     make bench      measure it
#
# `make help` lists every target.

# PYTHON picks the in-tree .venv when it exists, and falls back to whatever
# python3 is on PATH. `make setup` creates that venv, so no packages are ever
# installed outside this directory and nothing needs root.
#
# The path is absolute because CMake does not resolve a relative
# -DPython3_EXECUTABLE.
VENV       ?= .venv
SHELL      = /bin/sh
# The just-makeit version is declared once, in just-makeit.toml, which is the
# file jm itself reads. `make setup` installs exactly that version rather than
# naming it a second time, so the two can never disagree.
JM_VERSION := $(shell sed -n 's/^jm_version *= *"\(.*\)"/\1/p' just-makeit.toml)
NPROC      ?= $(shell nproc 2>/dev/null || echo 4)
VENV_PY    := $(CURDIR)/$(VENV)/bin/python
PYTHON     ?= $(or $(JUST_BUILDIT_PYTHON),$(wildcard $(VENV_PY)),$(shell command -v python3))
BUILD_DIR  ?= build
BUILD_TYPE ?= Release

.PHONY: all setup build test bench just-build clean help

# Creates .venv and installs the project into it, so the package list comes
# from pyproject.toml rather than from a copy here. Run this once; every
# target below then works.
setup: ## Provision an in-tree venv from pyproject.toml — no sudo, nothing system-wide
	@python3 -m venv $(VENV)
	@$(VENV_PY) -m pip -q install --upgrade pip
	@$(VENV_PY) -m pip -q install -e ".[dev]" "just-makeit==$(JM_VERSION)"
	@echo "setup: $(VENV) ready — 'make test' now builds the Python half too"

all: build ## Configure and build (the default)

$(BUILD_DIR)/CMakeCache.txt:
	@$(PYTHON) -c "import numpy" 2>/dev/null || $(PYTHON) -m pip install numpy
	@$(PYTHON) -c "import pytest" 2>/dev/null || $(PYTHON) -m pip install pytest
	cmake -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DPython3_EXECUTABLE=$(PYTHON) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

compile_commands.json: $(BUILD_DIR)/CMakeCache.txt
	cp $(BUILD_DIR)/compile_commands.json $@

build: $(BUILD_DIR)/CMakeCache.txt ## Configure and build
	cmake --build $(BUILD_DIR) --parallel $(NPROC)

test: build ## Run CTest and pytest
	ctest --test-dir $(BUILD_DIR) --output-on-failure
# --ignore-glob keeps the benchmarks out of the TEST run. pyproject's
# `python_files` deliberately collects `bench_*.py` so `make bench` can find
# them, and that setting is global — without this, `make test` would collect
# them too and fail on the missing `benchmark` fixture for anyone who has not
# installed the dev extra.
#
# Exit 5 is pytest's "no tests collected", which is not a failure here: the C
# half is the half that must always run, and a project mid-scaffold has no
# Python tests yet.
	$(PYTHON) -m pytest src/ -v --ignore-glob="*/benchmarks/*"; ret=$$?; \
		[ $$ret -eq 0 ] || [ $$ret -eq 5 ] || exit $$ret

# Builds and runs every benchmark, then merges the results into one snapshot.
# The runner is just-makeit, the same tool that generated the bindings, taken
# from .venv so the version matches the one this project declares.
bench: ## Run the C and Python benchmarks
	@$(VENV)/bin/just-makeit bench

# The hook just-buildit calls when pip builds a wheel. No `## ` comment, so
# it stays out of `make help` — you do not run this one by hand.
just-build:
	cmake -B $(BUILD_DIR) -S . \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DPython3_EXECUTABLE=$(JUST_BUILDIT_PYTHON) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build $(BUILD_DIR) --parallel $(NPROC)
	mkdir -p $(JUST_BUILDIT_OUTPUT_DIR)
	cp -r src/iqtools $(JUST_BUILDIT_OUTPUT_DIR)/iqtools

clean: ## Remove build artifacts
	rm -rf $(BUILD_DIR)
	find src -name "*.so" | xargs rm -f 2>/dev/null; true

# Prints the `## ` comment beside each target above, so adding a target adds
# its own help line.
help: ## Show this message
	@echo ""
	@echo "iqtools — make targets"
	@echo ""
	@awk 'BEGIN{FS=":.*?## "} /^[a-zA-Z_-]+:.*?## /{printf "  %-10s %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo ""
	@echo "First time:  make setup && make test"
	@echo ""
