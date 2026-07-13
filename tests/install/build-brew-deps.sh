#!/usr/bin/env bash
set -euo pipefail

# Generated from jb.toml by scripts/gen_install_scripts.py -- do
# not edit; change jb.toml's [dev.*] packages and run
# `make docs-relink`.
# macOS (Homebrew).
# --8<-- [start:install]
brew install cmake pkg-config python numpy
# --8<-- [end:install]
