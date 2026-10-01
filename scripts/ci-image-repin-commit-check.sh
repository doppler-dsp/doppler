#!/bin/sh
# A CI-image repin commit is ONE file on top of the commit it was built from.
#
# ci-image.yml commits the refreshed pin onto ci/repin-image and a human is
# told to land that branch as is (`gh pr create --head ci/repin-image
# --fill`). So whatever the commit carries lands with it. This refuses,
# before the push, every shape that is not a plain repin:
#
#   - no parent (an orphan snapshot of the whole tree);
#   - more than one parent (a merge);
#   - a parent that is not BASE (built on something other than the run's
#     commit);
#   - a diff against BASE naming anything but .github/ci-images.env.
#
# It reads the commit object itself (`git cat-file`), never `rev-list` or
# `merge-base`: in a shallow clone those report a grafted commit as
# parentless, which is how an ordinary one-file repin (bc37092f) was
# misdiagnosed as an orphan on 2026-10-01.
#
# Usage: ci-image-repin-commit-check.sh BASE HEAD
set -eu

PIN=.github/ci-images.env
base=$(git rev-parse --verify "$1^{commit}")
head=$(git rev-parse --verify "$2^{commit}")

parents=$(git cat-file -p "$head" | sed -n 's/^parent //p')
n=$(printf '%s' "$parents" | grep -c . || true)
if [ "$n" -ne 1 ]; then
    echo "ci-image-repin-commit-check: $head has $n parent(s) -- FAIL"
    echo "  A repin is one commit on top of the commit the run built from."
    exit 1
fi
if [ "$parents" != "$base" ]; then
    echo "ci-image-repin-commit-check: $head's parent is $parents,"
    echo "  not the base $base -- FAIL"
    exit 1
fi
files=$(git diff --name-only "$base" "$head")
if [ "$files" != "$PIN" ]; then
    echo "ci-image-repin-commit-check: the repin touches more than $PIN -- FAIL"
    printf '%s\n' "$files" | sed 's/^/    /'
    exit 1
fi
echo "ci-image-repin-commit-check: one file, $PIN, on $base -- OK"
