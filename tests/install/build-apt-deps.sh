#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:install]
sudo apt-get install build-essential cmake pkg-config \
  python3-dev python3-numpy libzmq3-dev libfftw3-dev libfftw3-bin
# --8<-- [end:install]
