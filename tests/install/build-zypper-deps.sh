#!/usr/bin/env bash
set -euo pipefail

# Generated from jb.toml by scripts/gen_install_scripts.py -- do
# not edit; change jb.toml's [dev.*] packages and run
# `make docs-relink`.
# openSUSE Leap and Tumbleweed.
# --8<-- [start:install]
sudo zypper install gcc make cmake pkg-config python3-devel python3-numpy
# --8<-- [end:install]
