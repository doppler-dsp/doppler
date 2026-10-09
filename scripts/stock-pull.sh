#!/usr/bin/env bash
# stock-pull.sh — pull stock images through STOCK_REGISTRY, retrying a rate
# limit. The one place a stock image is pulled (doppler#1979).
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
    echo "usage: stock-pull.sh IMAGE... | --dockerfile FILE..." >&2
    exit 2
}
[ $# -ge 1 ] || usage

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
        exit 0
    fi
    set -- "${images[@]}"
fi

# What a retry can fix. Word-bounded codes, so a digest's hex is not a 429.
transient() {
    grep -Eiq 'toomanyrequests|too many requests|rate exceeded|\b(429|500|502|503|504)\b|tls handshake timeout|i/o timeout|timed out|connection reset|connection refused|unexpected eof|temporarily unavailable' \
        <<<"$1"
}

pull() {
    local img=$1 n=1 wait=$delay out
    while :; do
        if out="$("$docker" pull --quiet "$img" 2>&1)"; then
            if [ "$n" -gt 1 ]; then
                echo "stock-pull: $img (attempt $n of $attempts)"
            else
                echo "stock-pull: $img"
            fi
            return 0
        fi
        if ! transient "$out" || [ "$n" -ge "$attempts" ]; then
            printf '%s\n' "$out" >&2
            echo "stock-pull: FAILED $img after $n attempt(s)" >&2
            return 1
        fi
        echo "stock-pull: $img: $(tail -n1 <<<"$out") -- retry $((n + 1))/$attempts in ${wait}s" >&2
        sleep "$wait"
        n=$((n + 1))
        wait=$((wait * 2))
    done
}

for img in "$@"; do
    pull "$img"
done
