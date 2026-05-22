#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:install]
pip install doppler-dsp
# --8<-- [end:install]

# --8<-- [start:verify]
python -c "import doppler; print(doppler.__version__)"
# --8<-- [end:verify]
