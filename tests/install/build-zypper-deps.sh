#!/usr/bin/env bash
set -euo pipefail

# openSUSE Leap and Tumbleweed.
# --8<-- [start:install]
sudo zypper install gcc gcc-c++ make cmake pkg-config \
  python3-devel python3-numpy zeromq-devel fftw3-devel
# --8<-- [end:install]
