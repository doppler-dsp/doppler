#!/bin/sh
# Fail on a merge-conflict marker left in a tracked text file.
#
# Ported from just-buildit/just-makeit#977 (gh-974), where a conflict sat in
# `docs/configuration.md` for ten days and a release and rendered on the
# published docs site. doppler's tree was clean when this landed, so this is a
# hole being closed rather than damage being repaired — but doppler had THREE
# ways to miss the same thing, and each one alone is sufficient.
#
# 1. mdformat NORMALISES the markers rather than refusing them, so every pass
#    through `make format` makes the corruption harder to see, not easier:
#
#      <<<<<<< HEAD          ->  \<<\<<\<<< HEAD        (every `<` escaped)
#      =======               ->  a setext H1 — the line ABOVE it becomes a
#                                heading and the `=======` disappears entirely
#      >>>>>>> d19e3ae (...) ->  > > > > > > > d19e3ae  (seven blockquotes)
#
#    A check written against the literal three markers finds ONE of those
#    three, and the `=======` case cannot be found after the fact at all.
#    Hence the extra patterns below, and hence running on the way IN.
#
# 2. mdformat ran BEFORE the conflict check in `.pre-commit-config.yaml`, so
#    even a raw marker was rewritten before anything looked for it. This hook
#    is ordered ahead of mdformat now.
#
# 3. `check-merge-conflict` from pre-commit-hooks — which this replaces — opens
#    with `if not is_in_merge() and not args.assume_in_merge: return 0`. It is
#    INERT unless git is mid-merge, so on every ordinary `make lint` and every
#    CI run it did nothing while printing "Passed". A conflict that survived a
#    rebase, or was resolved badly and committed, was invisible to it. This
#    script has no such gate: it looks every time.
#
# Usage: conflict-check.sh [file...]      (no arguments: every tracked file)
#
# Deliberately anchored at column 1. A marker indented inside a fenced code
# block is documentation about conflicts — this repo's own CHANGELOG and
# docs quote them — and git never writes one indented.
set -eu

pattern='^(<{7}([ ]|$)|>{7}([ ]|$)|={7}$|\\<<\\<<\\<<<|(> ){7})'

# -H on both arms: grep omits the filename when handed exactly one file, so
# without it the single-file mode reports a bare `12:<<<<<<< HEAD` and the
# caller has to guess where. The two modes now say the same thing.
if [ "$#" -gt 0 ]; then
    hits=$(grep -HnIE "$pattern" "$@" 2>/dev/null || true)
else
    hits=$(git ls-files -z | xargs -0 grep -HnIE "$pattern" 2>/dev/null || true)
fi

if [ -n "$hits" ]; then
    echo "ERROR: merge-conflict marker(s) in tracked file(s):"
    printf '%s\n' "$hits" | sed 's/^/  /'
    echo ""
    echo "  Resolve the conflict. If a line here is deliberate prose about"
    echo "  conflicts, indent it inside a fenced code block — this only looks"
    echo "  at column 1."
    exit 1
fi

echo "conflict-check: no conflict markers in tracked files"
