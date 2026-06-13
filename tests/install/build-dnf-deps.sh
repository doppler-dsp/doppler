#!/usr/bin/env bash
set -euo pipefail

# Fedora, RHEL, CentOS Stream, Rocky, AlmaLinux.
# --8<-- [start:install]
sudo dnf install gcc gcc-c++ make cmake pkgconf-pkg-config \
  python3-devel python3-numpy
# --8<-- [end:install]
