#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:specan]
pip install "doppler-dsp[specan]"
# --8<-- [end:specan]

# --8<-- [start:specan-web]
pip install "doppler-dsp[specan-web]"
# --8<-- [end:specan-web]

# --8<-- [start:cli]
pip install "doppler-dsp[cli]"
# --8<-- [end:cli]

# --8<-- [start:multiple]
pip install "doppler-dsp[specan,specan-web]"
# --8<-- [end:multiple]
