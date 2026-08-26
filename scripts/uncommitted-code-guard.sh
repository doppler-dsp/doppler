#!/usr/bin/env bash
# A branch-relative gate cannot see uncommitted work — say so, don't pass.
#
# `issue-link-check` and `changelog-check` both answer a question about the
# DIFF against origin/main: does this branch declare an issue, does it add a
# changelog entry. With no commits ahead there is no diff, so both correctly
# report `inert` and exit 0.
#
# That is right on a genuinely clean branch and WRONG the moment the working
# tree is dirty: the work exists, it just is not committed yet, and "inert"
# then reads exactly like "checked, fine". Running `make lint` before
# committing is the most natural thing to do and it is precisely when these
# gates answer about nothing — which is how a branch reaches CI red on a
# check that passed locally minutes earlier.
#
# So: inert + a dirty tree is a FAILURE, and the message says what to do.
# Inert + a clean tree stays a pass, because then there really is nothing to
# check.
#
# With commits ahead the gate does have something real to read, so a dirty
# tree there only WARNS. The trap is milder but not absent -- a branch whose
# commits are docs-only reports "no code changes" while uncommitted code sits
# beside it, and flips red the moment that is committed. Failing on it would
# fire on every `make lint` run mid-edit, and a gate that fires constantly is
# a gate someone switches off.
#
# Usage: uncommitted-code-guard.sh [--inert] <gate-name> <code-path>...
# The caller passes its OWN code-path list rather than this file keeping a
# third copy of `src native objects ffi`.
set -euo pipefail

inert=0
if [ "${1:-}" = "--inert" ]; then
    inert=1
    shift
fi
gate=${1:?usage: uncommitted-code-guard.sh [--inert] <gate-name> <code-path>...}
shift
[ "$#" -gt 0 ] || {
    echo "$gate: uncommitted-code-guard needs at least one code path" >&2
    exit 1
}

# --porcelain covers staged, unstaged and untracked alike: all three are work
# the gate cannot see. Paths are matched at a leading component so `srcfoo/`
# does not count as `src/`.
dirty=$(git status --porcelain -- "$@" 2>/dev/null || true)
[ -n "$dirty" ] || exit 0

n=$(printf '%s\n' "$dirty" | wc -l)

if [ "$inert" -eq 0 ]; then
    echo "$gate: NOTE — $n uncommitted code path(s) are not covered by the"
    echo "  verdict below; it reads the commits, not the working tree."
    exit 0
fi

echo "$gate: FAIL — inert, but the tree has uncommitted code changes"
echo
printf '%s\n' "$dirty" | sed 's/^/    /' | head -20
[ "$n" -gt 20 ] && echo "    ... and $((n - 20)) more"
echo
echo "  This gate compares against origin/main, so with no commits ahead it"
echo "  has nothing to read — and the changes above are exactly what it"
echo "  would have judged. Passing here would mean answering a question"
echo "  about an empty diff and calling it a verdict."
echo
echo "  Commit, then re-run. That is also the only order in which the"
echo "  answer matches what CI will compute."
exit 1
