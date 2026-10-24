#!/usr/bin/env bash
# stock-pull.sh — pull stock images through STOCK_REGISTRY, retrying a rate
# limit and falling back to STOCK_MIRROR. The one place a stock image is
# pulled (doppler#1979, #2256).
#
#   scripts/stock-pull.sh IMAGE...
#   STOCK_REGISTRY=<registry> scripts/stock-pull.sh --dockerfile FILE...
#
# ECR Public, where STOCK_REGISTRY points (#1950/#1953), rate-limits
# anonymous pulls per IP, and GitHub runners share IPs. A pull that meets
# `toomanyrequests: Rate exceeded` costs a whole CI cycle under strict
# up-to-date merges, though the next attempt seconds later would have
# worked. So this retries, with backoff, and only for what a retry can fix:
# a rate limit, a 5xx, a dropped or timed-out connection. Anything else, a
# missing tag ("manifest unknown") or a denied repository, fails at once
# with docker's own message, the registry's words, which is what the reader
# needs.
#
# An image already in the local store is not pulled at all: that is what
# `docker run` and BuildKit did before this helper existed, so a box that is
# offline, or behind a proxy, keeps working from its cache, and the mirror
# that rate-limits us is not asked about an image we already hold.
#
# --dockerfile pulls every stock image FILE's FROMs name, read by
# check_stock_images.py, the one Dockerfile reader, with STOCK_REGISTRY
# filled in. Run it before `docker build`: BuildKit resolves a FROM it finds
# in the local image store without asking the registry, and a FROM it has
# to pull is not retried. A buildx container-driver build pulls inside its
# own container, which no pre-pull reaches (#1982).
#
# After this, `docker run --pull=never` uses the image without a second,
# unretried pull; scripts/check_stock_images.py (make lint-stock-images)
# requires both halves.
#
# A DATA quota is not a rate (#2256). ECR Public answers `toomanyrequests:
# Data limit exceeded` once an IP has pulled its anonymous volume, and that
# outlasted the whole 75 s window in every leg logged so far. So that
# message stops the retries at once, and the image is pulled from
# STOCK_MIRROR instead, as is one whose retries ran out: the same Docker
# Official Image under the same tag (the index digests of every tag doppler
# pulls matched on both registries on 2026-10-16; a tag can move on
# either). It is then tagged with the primary's name, so the `docker run
# --pull=never` and the FROM that name it find it in the local store. A
# permanent failure is not sent to the mirror: a tag the primary does not
# have is a typo, not an outage. Only an image named under STOCK_REGISTRY
# has a mirror.
#
# Each pull prints the image's size as docker reports it, an upper bound on
# the bytes it pulled: under a volume quota, the biggest pullers are the
# number to read.
#
# --registry-for is --dockerfile for a build that cannot see the pulls: a
# buildx container-driver build, which pulls inside BuildKit's own
# container (#1982, #2572). It asks for each FROM's MANIFEST (`docker
# buildx imagetools inspect`) under the same retry and mirror policy, and
# pulls nothing. Then it names the registry the build should pull from:
# STOCK_MIRROR when any of them had to come from it, else STOCK_REGISTRY. The build then
# meets a quota-refused ECR only if the refusal began in the seconds since.
# The answer is one `stock_registry=<R>` line, appended to the file
# STOCK_REGISTRY_OUT names ($GITHUB_OUTPUT in a workflow), or to stdout.
#
#   STOCK_MIRROR          the fallback registry; the Makefile sets it. Unset,
#                         there is none.
#   STOCK_PULL_ATTEMPTS   tries per image, default 5
#   STOCK_PULL_DELAY      seconds before the first retry, doubling; default 5
#                         (5+10+20+40 = 75 s at most for one image)
#   DOCKER                the docker client, default `docker`
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
attempts="${STOCK_PULL_ATTEMPTS:-5}"
delay="${STOCK_PULL_DELAY:-5}"
docker="${DOCKER:-docker}"

usage() {
    echo "usage: stock-pull.sh IMAGE... | --dockerfile FILE..." \
        "| --registry-for FILE..." >&2
    exit 2
}
[ $# -ge 1 ] || usage

registry_for=0
if [ "$1" = --registry-for ]; then
    : "${STOCK_MIRROR:?stock-pull: --registry-for needs STOCK_MIRROR; run it through make}"
    registry_for=1
    set -- --dockerfile "${@:2}"
fi
# Set by pull() when an image came from STOCK_MIRROR.
fell_back=0
# What a pull is. A probe fetches the manifest alone: ECR Public's quota
# refusal is on the manifest GET (v0.74.0's was on
# .../python/manifests/sha256:...), so a few KB read the same answer a
# ~100 MB pull would, without spending the quota the builds need (#2572
# review).
fetch=(pull --quiet)
[ "$registry_for" = 1 ] && fetch=(buildx imagetools inspect)

# --registry-for's answer. Written on every exit path that succeeds, so a
# build never reads an empty registry.
answer() {
    [ "$registry_for" = 1 ] || return 0
    local chosen="$STOCK_REGISTRY"
    [ "$fell_back" = 1 ] && chosen="$STOCK_MIRROR"
    echo "stock-pull: build from $chosen" >&2
    echo "stock_registry=$chosen" >>"${STOCK_REGISTRY_OUT:-/dev/stdout}"
}

if [ "$1" = --dockerfile ]; then
    shift
    [ $# -ge 1 ] || usage
    : "${STOCK_REGISTRY:?stock-pull: --dockerfile needs STOCK_REGISTRY; run it through make}"
    images=()
    for f in "$@"; do
        refs="$(python3 "$here/check_stock_images.py" \
            --stock-froms "$f" --registry "$STOCK_REGISTRY")"
        while IFS= read -r ref; do
            [ -n "$ref" ] && images+=("$ref")
        done <<<"$refs"
    done
    if [ "${#images[@]}" -eq 0 ]; then
        echo "stock-pull: no stock FROM in $*; nothing to pull"
        answer
        exit 0
    fi
    set -- "${images[@]}"
fi

# What a retry can fix. Word-bounded codes, so a digest's hex is not a 429.
# Go's own timeout wordings too: an HTTP client deadline, a cancelled
# context, a cancelled request.
transient() {
    grep -Eiq 'toomanyrequests|too many requests|rate exceeded|\b(429|500|502|503|504)\b|tls handshake timeout|i/o timeout|timed out|client\.timeout exceeded|context deadline exceeded|request canceled|connection reset|connection refused|unexpected eof|temporarily unavailable' \
        <<<"$1"
}

# A quota that a retry seconds later cannot clear (#2256).
quota() {
    grep -Eiq 'data limit exceeded' <<<"$1"
}

# try_pull REF: 0 when pulled, 1 on a failure no retry or mirror fixes, 3
# when a transient failure outlasted the retries or a quota was met. Sets
# `tries` to the attempts made.
try_pull() {
    local ref=$1 wait=$delay out
    tries=1
    while :; do
        if out="$("$docker" "${fetch[@]}" "$ref" 2>&1)"; then
            return 0
        fi
        # The quota first: its wording need not carry a code transient()
        # knows, and it is what the mirror is for.
        if quota "$out"; then
            printf '%s\n' "$out" >&2
            return 3
        fi
        if ! transient "$out"; then
            printf '%s\n' "$out" >&2
            return 1
        fi
        if [ "$tries" -ge "$attempts" ]; then
            printf '%s\n' "$out" >&2
            return 3
        fi
        echo "stock-pull: $ref: $(tail -n1 <<<"$out") -- retry $((tries + 1))/$attempts in ${wait}s" >&2
        sleep "$wait"
        tries=$((tries + 1))
        wait=$((wait * 2))
    done
}

# The image's size in MB as `docker image inspect` reports it, or `?`: the
# unpacked layers (and, on the containerd store, the compressed content
# too), so never less than the pull transferred.
size_mb() {
    local b
    b="$("$docker" image inspect --format '{{.Size}}' "$1" 2>/dev/null)" || b=
    if [[ "$b" =~ ^[0-9]+$ ]]; then
        echo "$((b / 1000000)) MB"
    else
        echo "? MB"
    fi
}

pull() {
    local img=$1 rc=0 alt="" how=""
    if [ "$registry_for" = 0 ] && "$docker" image inspect "$img" >/dev/null 2>&1; then
        echo "stock-pull: $img (already present)"
        return 0
    fi
    try_pull "$img" || rc=$?
    [ "$tries" -gt 1 ] && how=" (attempt $tries of $attempts)"
    if [ "$rc" -eq 3 ] && [ -n "${STOCK_MIRROR:-}" ] &&
        [ -n "${STOCK_REGISTRY:-}" ] &&
        [[ "$img" == "$STOCK_REGISTRY/"* ]]; then
        alt="$STOCK_MIRROR/${img#"$STOCK_REGISTRY/"}"
        echo "stock-pull: $img: no pull after $tries attempt(s);" \
            "trying $alt" >&2
        rc=0
        try_pull "$alt" || rc=$?
        if [ "$rc" -eq 0 ]; then
            # A probe fetched only a manifest: there is no image to name.
            [ "$registry_for" = 1 ] || "$docker" tag "$alt" "$img"
            how=" (from $alt)"
            fell_back=1
        else
            img=$alt
        fi
    fi
    if [ "$rc" -ne 0 ]; then
        echo "stock-pull: FAILED $img after $tries attempt(s)" >&2
        return 1
    fi
    if [ "$registry_for" = 1 ]; then
        echo "stock-pull: $img$how, manifest only"
    else
        echo "stock-pull: $img$how, $(size_mb "$img")"
    fi
}

for img in "$@"; do
    pull "$img"
done
answer
