#!/usr/bin/env bash
#
# release-notes.sh — build the GitHub Release body for a version.
#
# The body is the Install instructions followed by that version's CHANGELOG
# section, verbatim. `softprops/action-gh-release` appends its own
# auto-generated "What's Changed" PR list AFTER this text, so the result reads:
# how to install it, what actually changed and why, then the commit-level list.
#
# WHY THIS EXISTS: the release body used to be a hardcoded template in
# release.yml ending in "See CHANGELOG.md for details", so the curated
# Breaking/Added/Changed prose — the part written with care — was never
# published anywhere a consumer lands. A runbook had claimed for months that an
# awk extraction gated the notes; no such extraction existed anywhere in the
# tree. Now it does, it is a make target, and it is testable without cutting a
# release:
#
#   make release-notes VERSION=0.42.0
#
# TOO LARGE TO PUBLISH: GitHub caps the release body, and `github-release`
# runs AFTER `publish-python` — so an oversized body fails once the version is
# already on PyPI, which refuses a re-upload. CHANGELOG.md is the engineering
# record and is allowed to be enormous; the RELEASE PAGE is not. So when the
# section will not fit, this publishes the version's `### Highlights` block and
# links to the full section. `scripts/check_release_notes_size.py` owns the
# budget and gates it on every PR, long before a tag exists.
#
# Usage:  scripts/release-notes.sh <x.y.z> [owner/repo]
set -euo pipefail

VERSION="${1:?usage: release-notes.sh <x.y.z> [owner/repo]}"
REPO="${2:-doppler-dsp/doppler}"
CHANGELOG="${CHANGELOG:-CHANGELOG.md}"

# The section runs from `## [x.y.z]` to the next `## ` heading. `index(...) == 1`
# anchors the match to the start of the line so `## [0.4.0]` cannot match inside
# some other line, and the version is passed as a VARIABLE rather than
# interpolated into the program text — a dotted version is a regex otherwise,
# and `0.4.0` would match `0y4z0`.
section=$(awk -v ver="$VERSION" '
  index($0, "## [" ver "]") == 1 { found = 1; next }
  found && /^## / { exit }
  found { print }
' "$CHANGELOG")

# Fail loudly rather than publish a release whose notes are silently empty.
# This is the whole point of the target: an empty extraction means the heading
# was never promoted out of [Unreleased], which is a release-blocking mistake
# that is otherwise invisible until someone reads the published page.
if [ -z "$(printf '%s' "$section" | tr -d '[:space:]')" ]; then
  echo "release-notes: no '## [$VERSION]' section in $CHANGELOG" >&2
  echo "release-notes: was the [Unreleased] heading promoted?" >&2
  exit 1
fi

# The budget comes from the checker so there is exactly one copy of it.
SIZE_CHECK="$(dirname "$0")/check_release_notes_size.py"
BUDGET=$(python3 "$SIZE_CHECK" --print-budget)

# `### Highlights` runs to the next `### ` heading.
#
# A here-string, NOT a pipe: this awk exits at the next heading, so a piped
# `printf` is still writing when the reader goes away and takes SIGPIPE --
# which `set -o pipefail` turns into a failed release. Measured: exit 141 on a
# 132 KB section, silently producing an empty body.
highlights=$(awk '
  /^### Highlights/ { found = 1; next }
  found && /^### / { exit }
  found { print }
' <<<"$section")

published="$section"
note=""
if [ "${#section}" -gt "$BUDGET" ]; then
  if [ -z "$(printf '%s' "$highlights" | tr -d '[:space:]')" ]; then
    echo "release-notes: the [$VERSION] section is ${#section} characters," >&2
    echo "release-notes: over the $BUDGET budget, and has no ### Highlights" >&2
    echo "release-notes: block to publish instead. Summarise it." >&2
    exit 1
  fi
  published="$highlights"
  note="
This release is large. The highlights are above; every entry, in full, is in
[CHANGELOG.md](https://github.com/${REPO}/blob/v${VERSION}/CHANGELOG.md).
"
fi

cat <<EOF
## Install

**Python:**
\`\`\`sh
pip install doppler-dsp==${VERSION}
pip install "doppler-dsp[cli]==${VERSION}"
pip install "doppler-dsp[specan]==${VERSION}"
\`\`\`

**C library:** download the pre-built tarballs from the assets below.

**Container** (\`linux/amd64\` + \`linux/arm64\`, \`cli\`/\`specan-web\`
extras pre-installed):
\`\`\`sh
docker pull ghcr.io/${REPO}:${VERSION}
docker run --rm ghcr.io/${REPO}:${VERSION} doppler --help
docker run --rm ghcr.io/${REPO}:${VERSION} wfmgen --help
\`\`\`
See [Docker](https://doppler-dsp.github.io/doppler/install/docker/#published-container) for details.

## What's in ${VERSION}

${published}
${note}
---
EOF
