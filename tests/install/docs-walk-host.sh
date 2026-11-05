#!/usr/bin/env bash
# docs-walk-host.sh -- drive the docs walk from the HOST (make docs-walk):
# one bare container per stage, so no stage inherits what another installed
# (doppler#2857). Bash on purpose: the Makefile's /bin/sh is dash on a runner.
#
#   docs-walk-host.sh walk      pip, tarball, clone, deb in their own containers
#   docs-walk-host.sh docker    docs/install/docker.md's run line, on the host
#
# Environment: WALK_MODE (published|candidate), WALK_VERSION, WALK_IMAGE,
# CONTAINER (docker|podman), WALK_LOG, WALK_ARTIFACTS (candidate: a directory
# of CI's own wheel/tarball/.debs to use instead of building them), UV_VERSION
# (candidate build), WORK_DIR (scratch, default build/).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MODE="${WALK_MODE:-published}"
CONTAINER="${CONTAINER:-docker}"
V="${WALK_VERSION:?WALK_VERSION}"
IMAGE="${WALK_IMAGE:?WALK_IMAGE}"
WORK="${WORK_DIR:-$ROOT/build}"
LOG="${WALK_LOG:-}"
[ -z "$LOG" ] || exec > >(tee "$LOG") 2>&1

case "${1:-walk}" in
docker)
    # The page's own line, on the host (a container cannot start one), with
    # the host's CONTAINER. Run unpiped, so its exit status is the verdict.
    line="$(bash "$ROOT/tests/install/docs-walk.sh" plan docker | grep -E '^docker run ')"
    [ -n "$line" ] || { echo "docs-walk-docker: the page yields no run line"; exit 1; }
    line="${line/#docker /$CONTAINER }"
    echo "$line"
    out="$($line)"
    printf '%s\n' "$out" | head -5
    [ -n "$out" ] || { echo "docs-walk-docker: the image printed nothing"; exit 1; }
    ;;
walk)
    # A clone of HEAD, never the working tree: a worktree's .git is a file
    # pointing at a host path the container cannot see, and a tree with build
    # directories is not what a reader gets.
    src="$WORK/docs-walk-src"
    { [ -d "$src/.git" ] || git clone -q --no-hardlinks "$ROOT" "$src"; }
    git -C "$src" fetch -q "$ROOT" HEAD
    git -C "$src" checkout -q --detach FETCH_HEAD
    echo "walking commit $(git -C "$src" rev-parse HEAD) ($MODE, v$V, $IMAGE, $CONTAINER)"
    run() { "$CONTAINER" run --rm --init --pull=never "$@"; }
    # A directory of this run's own: an artifact left by an earlier run (an
    # older wheel, other .debs) must never be walked as if it were this one's.
    mkdir -p "$WORK"
    art="$(mktemp -d "$WORK/docs-walk-art.XXXXXX")"
    stages="pip tarball clone deb"
    if [ "$MODE" = candidate ]; then
        [ -z "${WALK_ARTIFACTS:-}" ] || cp "$WALK_ARTIFACTS"/* "$art"/
        echo "### BUILD the candidate artifacts (toolchain confined to this container)"
        run -e UV_VERSION="${UV_VERSION:?UV_VERSION}" -v "$src":/w:ro \
            -v "$art":/out "$IMAGE" bash /w/tests/install/docs-walk-build.sh /w /out "$V"
    fi
    pass=(); fail=()
    for s in $stages; do
        echo
        if run -v "$src":/w:ro -v "$art":/art:ro -w /w "$IMAGE" \
            bash /w/tests/install/docs-walk.sh run "$MODE" "$s" /w /art "$V"; then
            pass+=("$s")
        else
            fail+=("$s")
        fi
    done
    echo; echo "### SUMMARY ($MODE, v$V)"
    for s in "${pass[@]}"; do echo "PASS $s"; done
    for s in "${fail[@]}"; do echo "FAIL $s"; done
    [ "${#fail[@]}" -eq 0 ] || { echo "docs-walk: FAILED"; exit 1; }
    echo "docs-walk: PASS"
    ;;
*) echo "usage: docs-walk-host.sh walk|docker" >&2; exit 2 ;;
esac
