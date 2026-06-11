#!/usr/bin/env bash
set -euo pipefail

# Arch Linux and derivatives (Manjaro, EndeavourOS, CachyOS, …).
# --8<-- [start:install]
sudo pacman -S --needed base-devel cmake python python-numpy zeromq fftw
# --8<-- [end:install]
