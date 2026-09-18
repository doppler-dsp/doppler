#!/bin/sh
# Fail on a tracked path a shell mangled into existence.
#
# This repo carried `fm_stream_sink_close (self->h);,+25p` — 1500 lines, a
# stale COPY of native/src/app/wfmgen.c — in its root for a week, committed by
# a change that had nothing to do with it. It is the residue of a quoting slip
# in a `sed -n '/…/,+25p'`: the shell split the pattern, `sed` read the rest as
# filenames, and a redirect wrote the fragment out under whatever was left.
#
# Nothing noticed, and nothing could: it compiled nowhere, so no build broke;
# it was not a `.py`, `.md`, `.c` under `native/`, or a docs page, so no
# formatter, linter or docs gate opened it; and the drift gate reads the
# manifest's files, not the tree's. A second copy of a CLI, drifting from the
# first, was invisible to every check this repo has.
#
# What makes the class findable is not the content but the NAME. A path a
# person meant contains letters, digits, dot, dash, underscore and slash; a
# path a shell produced by accident carries the punctuation of the command
# that leaked — a space, a paren, a semicolon, `>`, `+`, a comma. So the rule
# is the character set, and it is exact rather than heuristic: one offender
# today, zero after this, and any future one names itself.
#
# Usage: check-tracked-paths.sh [path...]   (no arguments: every tracked path)
set -eu

if [ "$#" -gt 0 ]; then
    paths=$(printf '%s\n' "$@")
else
    paths=$(git ls-files)
fi

bad=$(printf '%s\n' "$paths" | grep -vE '^[A-Za-z0-9._/-]+$' || true)

if [ -n "$bad" ]; then
    printf 'check-tracked-paths: FAIL — tracked path(s) outside [A-Za-z0-9._/-]:\n' >&2
    printf '%s\n' "$bad" | sed 's/^/  /' >&2
    printf '\n  A name like this is a shell fragment, not a decision: check the\n' >&2
    printf '  command that created it (a quoting slip in sed/awk redirects the\n' >&2
    printf '  pattern into a filename), delete the file, and re-run.\n' >&2
    exit 1
fi

# Two paths that differ only in case are one path on Windows and on macOS's
# default filesystem, so a checkout there holds only one of them and shows the
# other as a modification nobody made. The repo had exactly one pair:
# src/doppler/resample/tests/test_Resampler.py (jm's scaffold, named after the
# class) beside test_resampler.py (the real suite, named by hand), found by a
# Windows checkout rather than by anything here. Always the WHOLE tree: a
# collision is a relation between two names, and a pre-commit run handed one
# of them cannot see the other.
collide=$(git ls-files | awk '
    { k = tolower($0); n[k]++; m[k] = m[k] "\n  " $0 }
    END { for (k in n) if (n[k] > 1) print substr(m[k], 2) }')

if [ -n "$collide" ]; then
    printf 'check-tracked-paths: FAIL — tracked paths that differ only in case:\n' >&2
    printf '%s\n' "$collide" >&2
    printf '\n  Windows and macOS keep one of each group, so the checkout shows\n' >&2
    printf '  a phantom edit. Keep one name. For a jm scaffold, keep jm'"'"'s name\n' >&2
    printf '  and move the real content into it: jm recreates a deleted scaffold.\n' >&2
    exit 1
fi

printf 'check-tracked-paths: OK — %s tracked path(s), every name is one a person could type, none differ only in case\n' \
    "$(printf '%s\n' "$paths" | grep -c .)"
