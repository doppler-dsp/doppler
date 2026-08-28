#!/usr/bin/env bash
# An entry is a FILE. A direct CHANGELOG.md edit is the churn, so refuse it.
#
# `changelog.d/README.md` has asked for a fragment since 2026-08-16, measured
# on the reason: twelve PRs in flight all appended near the top of the same
# `[Unreleased]`, so EACH merge knocked the other eleven to CONFLICTING --
# O(N^2) hand-resolutions, none of them about the code. It is a property of
# the layout, not of anyone's discipline.
#
# But `changelog-check` accepted "CHANGELOG.md OR a fragment", so the file
# that produces the churn stayed a passing answer. It kept happening. On
# 2026-08-27 three PRs in one stack all edited CHANGELOG.md directly and the
# conflict was hand-resolved twice on two rebases before anyone noticed the
# directory existed. Guidance without a gate is the thing this repo keeps
# re-learning.
#
# So this is the gate. The rule is behavioural rather than diff-shaped,
# because "did you add a line inside [Unreleased]" is not something a diff
# hunk answers reliably: COUNT the `- ` entries under `## [Unreleased]` at
# the base and at HEAD. More of them now, and no fragment consumed, is a
# hand-written entry in the shared file.
#
# Two things deliberately still pass:
#
#   * `make changelog-assemble` -- it promotes fragments INTO [Unreleased]
#     and DELETES them, so the entry count rises and fragments disappear.
#     That is the one legitimate writer, and it is detected by the deletion
#     rather than by a flag somebody has to remember to pass.
#   * Editing the WORDING of an entry that already exists, or anything in an
#     already-released section. The count does not move, so neither does this.
#
# Usage: scripts/changelog-fragment-guard.sh <base-ref-or-sha>
set -uo pipefail

BASE="${1:?usage: changelog-fragment-guard.sh <base>}"

# `- ` entries under `## [Unreleased]`, up to the next `## ` heading.
unreleased_entries() {
  awk '/^## \[Unreleased\]/{f=1;next} f&&/^## /{exit} f' \
    | grep -c '^- ' || true
}

base_n=$(git show "$BASE:CHANGELOG.md" 2>/dev/null | unreleased_entries)
head_n=$(unreleased_entries < CHANGELOG.md)
: "${base_n:=0}" "${head_n:=0}"

[ "$head_n" -le "$base_n" ] && exit 0

# An assemble consumes fragments. Any deletion under changelog.d/ is that.
if git diff --diff-filter=D --name-only "$BASE"..HEAD -- changelog.d \
   | grep -q '\.md$'; then
  echo "changelog-fragment-guard: [Unreleased] grew and fragments were" \
       "consumed — an assemble, OK"
  exit 0
fi

cat >&2 <<MSG
changelog-fragment-guard: this branch adds an entry to CHANGELOG.md's
  [Unreleased] section by hand — $base_n entr(ies) at the base, $head_n now.

  That is the file every other open PR is also appending to, near the same
  line, so each merge knocks the rest to CONFLICTING. Measured with twelve
  PRs in flight: all twelve conflicted, none of them about the code.

  Move it to its own file, which nobody else is touching:

    changelog.d/<added|changed|fixed|removed|breaking|deprecated|security>/<slug>.md

  The directory IS the '###' heading; the content is the entry verbatim,
  starting with '- ', continuation lines indented four spaces. Then revert
  your CHANGELOG.md hunk -- 'make changelog-assemble' writes that file at
  release time, and it is the only thing that should.

  Why it is a file and not a line: changelog.d/README.md.
MSG
exit 1
