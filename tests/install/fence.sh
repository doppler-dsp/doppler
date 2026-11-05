#!/usr/bin/env bash
# fence.sh <file> <heading regex> <line regex> [language, default sh]
#
# Print the first ```sh block after the heading that contains the line,
# without its fences. consumer-walk.sh runs what this prints, and
# src/doppler/tests/test_consumer_walk_extraction.py runs THIS on the PR tree
# (no container, no network): a page that stops yielding its command, or
# yields one bash cannot parse, fails at review time, not at release.
set -euo pipefail
awk -v h="$2" -v l="$3" -v lang="${4:-sh}" '
    $0 ~ h { on = 1 }
    on && $0 == "```" lang { blk = ""; infence = 1; next }
    infence && /^```$/ { if (blk ~ l) { printf "%s", blk; exit } infence = 0; next }
    infence { blk = blk $0 "\n" }
' "$1"
