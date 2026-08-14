#!/bin/sh
# A branch that changes code must SAY what it closes — or say that it closes
# nothing.
#
# ## Why this exists
#
# GitHub closes an issue only when a closing keyword reaches the default
# branch, in a PR body or in a commit message. doppler rebase-merges, so
# commit messages land on `main` and are a valid carrier. Nothing checked that
# any branch used one, and the cost was not hypothetical:
#
#   c0e0e615  "gate the generated C API tree, which had drifted 22 pages"
#   #714      "docs/c-api is generated, committed, and gated by nothing"
#
# The commit IS the fix for the issue. It shipped, the gate went live, and the
# issue stayed open for a day until someone re-read the backlog and closed it
# by hand. Two more (#663/#664/#665 on PR #717) were found in the same triage,
# already fixed and still open. An open-issue count that includes fixed work is
# a backlog nobody can plan from.
#
# ## The bar is a STATEMENT, not a link
#
# Most branches legitimately close nothing — a re-vendor, a pin bump, a
# refactor. Requiring every branch to name an issue would be a gate that
# argues with its author, which is the failure mode `changelog-check`'s own
# comment warns about. So `No-issue:` passes, and passes silently.
#
# What is not allowed is SILENCE, because silence is indistinguishable between
# "this closes nothing" and "this closes #714 and nobody said so". That
# distinction is the whole point, and only the author can make it.
#
# Reading the commit messages rather than the PR body is deliberate: it works
# on a developer's clone with no GitHub context, exactly as `changelog-check`
# does, so the gate a developer runs is the gate CI runs.
#
# Usage: issue-link-check.sh [file...]
#          no arguments: derive from `git log <base>..HEAD`
#          with files:   treat each file's contents as a commit message
#                        (this is how the gate's own test drives it, without
#                        needing a scratch repository)
#
# Env: ISSUE_BASE          base ref to compare against  (default origin/main)
#      ISSUE_CODE_PATHS    space-separated code roots   (default from Makefile)
set -eu

BASE=${ISSUE_BASE:-origin/main}
CODE_PATHS=${ISSUE_CODE_PATHS:-"src native objects ffi"}

# `Closes|Fixes|Resolves #N` is GitHub's own set (the -es/-ed forms too).
# `No-issue:` is the opt-out, spelled as a git trailer so it reads as metadata
# rather than prose and cannot be matched by accident inside a sentence.
KEYWORD='([Cc]los(e|es|ed)|[Ff]ix(es|ed)?|[Rr]esolv(e|es|ed))[ ]+#[0-9]+'
OPTOUT='^[Nn]o-issue:'

if [ "$#" -gt 0 ]; then
    msgs=$(cat "$@")
    changed_code=1
else
    base=$(git merge-base HEAD "$BASE" 2>/dev/null) || {
        echo "issue-link-check: no merge base with $BASE —"
        echo "  fetch it (CI needs fetch-depth: 0) or set ISSUE_BASE."
        exit 1
    }
    files=$(git diff --name-only "$base"..HEAD)
    if [ -z "$files" ]; then
        echo "issue-link-check: no commits ahead of $BASE — inert"
        exit 0
    fi
    pat=$(printf '%s\n' $CODE_PATHS | sed 's|.*|^&/|' | paste -sd'|')
    if printf '%s\n' "$files" | grep -qE "$pat"; then
        changed_code=1
    else
        changed_code=0
    fi
    msgs=$(git log --format=%B "$base"..HEAD)
fi

if [ "$changed_code" -eq 0 ]; then
    echo "issue-link-check: no code changes on this branch"
    exit 0
fi

if printf '%s\n' "$msgs" | grep -qE "$KEYWORD"; then
    n=$(printf '%s\n' "$msgs" | grep -oE "$KEYWORD" | grep -oE '#[0-9]+' \
        | sort -u | tr '\n' ' ')
    echo "issue-link-check: branch declares ${n}— OK"
    exit 0
fi

if printf '%s\n' "$msgs" | grep -qE "$OPTOUT"; then
    echo "issue-link-check: branch declares No-issue — OK"
    exit 0
fi

echo "issue-link-check: this branch changes code and says nothing about"
echo "  issues — FAIL"
echo ""
echo "  Add ONE of these to a commit message on this branch:"
echo ""
echo "    Closes #123        so merging actually closes it"
echo "    No-issue:          when it genuinely closes nothing"
echo ""
echo "  Silence is the one answer that cannot be acted on: it looks the same"
echo "  whether the branch closes nothing or closes an issue nobody linked."
echo "  c0e0e615 fixed #714 and left it open for a day; #663/#664/#665 sat"
echo "  fixed-and-open until a triage pass found them."
exit 1
