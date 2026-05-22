#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:install]
cmake --install build
sudo ldconfig
# --8<-- [end:install]

# --8<-- [start:verify]
pkg-config --modversion doppler
# --8<-- [end:verify]

# --8<-- [start:custom-prefix]
cmake --install build --prefix ~/.local
export PKG_CONFIG_PATH=~/.local/lib/pkgconfig
# --8<-- [end:custom-prefix]
