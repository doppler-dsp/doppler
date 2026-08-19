#!/usr/bin/env bash
set -euo pipefail

# Generated from bootstrap.toml by
# scripts/gen_install_scripts.py -- do
# not edit; change bootstrap.toml's [dev.*] packages and run
# `make docs-relink`.
# Fedora, RHEL, CentOS Stream, Rocky, AlmaLinux.
# --8<-- [start:install]
sudo dnf install gcc make ccache cmake pkgconf-pkg-config python3-devel \
  python3-numpy
# --8<-- [end:install]
