# example-projects/rpm-consumer/ccsds-415 — a downstream project's own
# Makefile.
#
# Thin on purpose: CMake is the build system, and this wraps the commands
# you would otherwise type. `make help` lists everything.

BUILD_DIR        ?= build
CMAKE_BUILD_TYPE ?= Release
# Where doppler is installed. Empty means a system install CMake can already
# find, which the -devel RPM is; set it for any other install:
#   make PREFIX=~/.local run
PREFIX           ?=
# More CMake options, e.g. CMAKE_ARGS=-DCCSDS415_FETCH_FTXUI=ON to fetch
# FTXUI for the TUI where no package provides it.
CMAKE_ARGS       ?=
# The program's own options, e.g. ARGS='--carrier lo --esn0 6'.
ARGS             ?=
# Where the captures go, and the plots from them.
OUT              ?= $(BUILD_DIR)/out
PYTHON           ?= python3

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) $(CMAKE_ARGS)
ifneq ($(strip $(PREFIX)),)
CMAKE_FLAGS += -DCMAKE_PREFIX_PATH=$(abspath $(PREFIX))
endif

.PHONY: all build run tui plot clean help
.DEFAULT_GOAL := build

all: build

# Configure every time: cheap, and a cached configure would silently ignore
# a changed PREFIX.
build:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR)

# The program checks itself and exits non-zero on any failed check, so
# `make run` succeeding is the test.
run: build
	@mkdir -p $(OUT)
	$(BUILD_DIR)/ccsds415 --out $(OUT) $(ARGS)

# The same run, drawn live at real time (q quits). Built only where FTXUI
# is found: see CMakeLists.txt.
tui: build
	@mkdir -p $(OUT)
	@[ -x $(BUILD_DIR)/ccsds415-tui ] || { \
	    echo "no ccsds415-tui: install ftxui-devel, or" \
	         "CMAKE_ARGS=-DCCSDS415_FETCH_FTXUI=ON"; exit 1; }
	$(BUILD_DIR)/ccsds415-tui --out $(OUT) $(ARGS)

# Plots what `make run` captured: numpy and matplotlib, no doppler wheel.
# PLOT_ARGS=--save writes PNGs beside the captures instead of showing them.
PLOT_ARGS ?=
plot:
	$(PYTHON) plot.py $(OUT) $(PLOT_ARGS)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "rpm-consumer/ccsds-415 — the 415.1-B-1 receiver from C++"
	@echo
	@echo "  make build              configure and compile"
	@echo "  make run                build, then run and self-check"
	@echo "  make tui                the same run drawn live (needs FTXUI)"
	@echo "  make plot               plot the captures (numpy, matplotlib)"
	@echo "  make clean              remove $(BUILD_DIR)/"
	@echo
	@echo "  PREFIX=<dir>            where doppler is installed"
	@echo "  CMAKE_ARGS='<flags>'    more CMake options"
	@echo "  ARGS='<options>'        the program's options (--help lists them)"
	@echo "  OUT=<dir>               captures and plots (default $(BUILD_DIR)/out)"
	@echo "  PLOT_ARGS=--save        write PNGs rather than showing them"
	@echo "  CMAKE_BUILD_TYPE=<t>    default Release"
	@echo "  BUILD_DIR=<dir>         default build"
