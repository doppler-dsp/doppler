#!/usr/bin/env bash
# Install doppler build dependencies.
# Uses jbx (just-runit) to fetch install-deps from just-bashit, or falls
# back to a local just-bashit checkout if jbx is not available.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_FILE="${SCRIPT_DIR}/deps.toml"
JBS_SCRIPT="just-bashit:install-deps"

if command -v jbx >/dev/null 2>&1; then
	jbx "${JBS_SCRIPT}" "$@" <"${DEPS_FILE}"
elif command -v jb >/dev/null 2>&1; then
	jb run "${JBS_SCRIPT}" "$@" <"${DEPS_FILE}"
elif command -v just-runit >/dev/null 2>&1; then
	just-runit "${JBS_SCRIPT}" "$@" <"${DEPS_FILE}"
elif [ -f "${SCRIPT_DIR}/../just-bashit/src/install-deps.sh" ]; then
	bash "${SCRIPT_DIR}/../just-bashit/src/install-deps.sh" "$@" "${DEPS_FILE}"
else
	echo "error: just-buildit (jb) not found." >&2
	echo "  . <(curl -sSL https://just-buildit.github.io/get-jb.sh)" >&2
	exit 1
fi
