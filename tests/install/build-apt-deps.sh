#!/usr/bin/env bash
set -euo pipefail

# Generated from bootstrap.toml by
# scripts/gen_install_scripts.py -- do
# not edit; change bootstrap.toml's [dev.*] packages and run
# `make docs-relink`.
# Ubuntu, Debian, and derivatives (Mint, Pop!_OS, ...).
# --8<-- [start:install]
sudo apt-get install build-essential ccache cmake pkg-config python3-dev \
  python3-numpy
# --8<-- [end:install]
