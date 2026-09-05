#!/bin/sh
# A CI-image repin that nobody merges must not be invisible.
#
# ## Why this exists
#
# `.github/workflows/ci-image.yml` rebuilds the CI toolchain image nightly and
# asks one question the rest of the tree cannot ask: **have the upstream
# packages moved?** A rebuild whose package fingerprint differs from the pinned
# one means CI is provisioning an environment `bootstrap.toml` no longer
# describes.
#
# `make ci-image-check` does NOT answer that. It compares
# CI_IMAGE_SOURCE_HASH -- *our* inputs, the Dockerfile and bootstrap.toml's
# package tables -- and that every `container:` names a pinned digest. It never
# reads CI_IMAGE_FINGERPRINT_*, and nothing else in the tree does either. So
# the two drift cases are not symmetric:
#
#   our inputs move       -> ci-image-check fails, on the PR that moved them
#   upstream packages move -> the nightly notices, and NOTHING gates it
#
# The nightly used to deliver its answer by opening a repin PR. That step could
# never succeed: the `doppler-dsp` org forbids GitHub Actions from creating
# pull requests, so it force-pushed `ci/repin-image` and then died on
# `gh pr create`. The workflow was red on every `main` run for three releases,
# feeds no aggregator, and so gated nothing -- a check that cannot succeed,
# reporting a failure nobody could act on. doppler#1212.
#
# The branch push always worked. This gate is what makes that branch mean
# something: the nightly pushes `ci/repin-image` and stops, and a pending repin
# turns into a RED REQUIRED CHECK here instead of a PR nobody opened.
#
# ## What it compares, and why not the whole file
#
# The three SEMANTIC values, never the digests. A rebuild changes the digest
# every time -- timestamps, layer ids -- while the content is identical, so
# diffing the file would report drift on any night the image was rebuilt at
# all. This mirrors exactly what ci-image.yml's own `changed` computation asks.
#
# Comparing values also makes the gate self-clearing: once a repin lands on
# `main`, the tree's fingerprints equal the branch's and this goes green with
# no branch deletion required (the nightly deletes it anyway, as hygiene).
#
# Usage: ci-image-repin-check.sh [pending-env pinned-env]
#          no arguments: fetch origin/ci/repin-image, compare against the tree
#          two files:    compare those two pin blocks directly (this is how
#                        the gate's own test drives it, without a network or a
#                        scratch repository)
#
# Env: REPIN_BRANCH   branch the nightly pushes   (default ci/repin-image)
#      REPIN_PIN      pin file in the tree        (default .github/ci-images.env)
#      CI             when set, an unreachable remote is an ERROR, not a skip
set -eu

BRANCH=${REPIN_BRANCH:-ci/repin-image}
PIN=${REPIN_PIN:-.github/ci-images.env}

# The three values that decide whether a repin is owed. Digests are
# deliberately absent -- see the header.
KEYS='CI_IMAGE_FINGERPRINT_2204 CI_IMAGE_FINGERPRINT_2404 CI_IMAGE_SOURCE_HASH'

# Read one KEY=VALUE out of a pin block. Missing key prints nothing, which
# compares unequal to any real value -- an unpinned side must not read as
# agreement.
value_of() {
    grep "^$2=" "$1" 2>/dev/null | head -n1 | cut -d= -f2- || true
}

if [ "$#" -eq 2 ]; then
    pending=$1
    pinned=$2
else
    if [ ! -f "$PIN" ]; then
        echo "ci-image-repin-check: $PIN is missing — run ci-image-check first"
        exit 1
    fi
    pinned=$PIN

    # A missing branch is the normal, healthy state: no repin is pending.
    if ! git fetch --quiet --depth=1 origin "$BRANCH" 2>/dev/null; then
        if git ls-remote --exit-code --heads origin "$BRANCH" >/dev/null 2>&1
        then
            echo "ci-image-repin-check: $BRANCH exists but could not be"
            echo "  fetched — refusing to report a verdict it did not read."
            exit 1
        fi
        # Distinguish "no such branch" from "no network at all". Only the
        # first is a pass; the second is a check that did not run, and in CI
        # that must not look like one that passed.
        if git ls-remote --heads origin >/dev/null 2>&1; then
            echo "ci-image-repin-check: no $BRANCH on origin — no repin" \
                 "pending, OK"
            exit 0
        fi
        if [ -n "${CI:-}" ]; then
            echo "ci-image-repin-check: cannot reach origin — FAIL"
            echo "  In CI an unreachable remote is a check that did not run,"
            echo "  which must not be reported as one that passed."
            exit 1
        fi
        echo "ci-image-repin-check: cannot reach origin — skipped (local)."
        echo "  This gate's execution home is the ci.yml job of the same name."
        exit 0
    fi

    pending=$(mktemp)
    # shellcheck disable=SC2064  # expand $pending now, at trap-set time
    trap "rm -f '$pending'" EXIT
    if ! git show "FETCH_HEAD:$PIN" > "$pending" 2>/dev/null; then
        echo "ci-image-repin-check: $BRANCH carries no $PIN — FAIL"
        echo "  The nightly writes that file and nothing else on that branch;"
        echo "  a branch without it was not written by ci-image.yml."
        exit 1
    fi
fi

rc=0
for k in $KEYS; do
    want=$(value_of "$pending" "$k")
    have=$(value_of "$pinned" "$k")
    if [ "$want" != "$have" ]; then
        if [ "$rc" -eq 0 ]; then
            echo "ci-image-repin-check: a CI-image repin is PENDING and"
            echo "  unmerged — FAIL"
            echo ""
        fi
        printf '  %s\n    pinned:  %s\n    rebuilt: %s\n' \
            "$k" "${have:-<unset>}" "${want:-<unset>}"
        rc=1
    fi
done

if [ "$rc" -eq 0 ]; then
    echo "ci-image-repin-check: pin matches the latest rebuild — OK"
    exit 0
fi

echo ""
echo "  The nightly rebuilt the CI toolchain image and got a different"
echo "  package set, so the image CI pins is no longer the environment"
echo "  bootstrap.toml describes. The refreshed pin is already committed"
echo "  on '$BRANCH'; it just needs to land."
echo ""
echo "    gh pr create --head $BRANCH --fill"
echo ""
echo "  Merging that turns this gate green. This is deliberately BLOCKING:"
echo "  an unmerged repin means every Linux job is running in an image the"
echo "  repo no longer describes, and the previous delivery mechanism (a PR"
echo "  the nightly opened) could not run at all — doppler#1212."
exit 1
