#!/usr/bin/env bash
set -euo pipefail

# Generated from bootstrap.toml by
# scripts/gen_install_scripts.py -- do
# not edit; change bootstrap.toml's [dev.*] packages and run
# `make docs-relink`.
# Arch Linux and derivatives (Manjaro, EndeavourOS, CachyOS, ...).
# --8<-- [start:install]
sudo pacman -S --needed base-devel ccache cmake pkgconf python python-numpy
# --8<-- [end:install]
