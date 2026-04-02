#!/usr/bin/env bash
# retag_wheel.sh -- fix wheel tags then repair shared-lib dependencies
#
# uv_build produces a py3-none-any wheel because it treats the package as
# pure Python.  This script:
#   1. Retags the wheel to the correct CPython ABI tag (cp312-cp312, etc.)
#   2. Repairs shared-lib dependencies (auditwheel on Linux, delocate on macOS)
#
# Usage: retag_wheel.sh <wheel> <dest_dir>
set -euo pipefail

WHEEL="$1"
DEST_DIR="$2"
WHEEL_DIR="$(dirname "$WHEEL")"

PYTAG=$(python -c \
    "import sys; v=sys.version_info; print(f'cp{v.major}{v.minor}')")

pip install --quiet wheel

# Replace py3-none-any tags with cpXYZ-cpXYZ-<platform>
python -m wheel tags \
    --python-tag "$PYTAG" \
    --abi-tag    "$PYTAG" \
    --remove     "$WHEEL"

RETAGGED=$(ls "${WHEEL_DIR}/${PYTAG}-${PYTAG}"-*.whl | head -1)

if [[ "$(uname)" == "Darwin" ]]; then
    pip install --quiet delocate
    delocate-wheel --wheel-dir "$DEST_DIR" "$RETAGGED"
else
    uvx auditwheel repair --wheel-dir "$DEST_DIR" "$RETAGGED"
fi

rm -f "$RETAGGED"
