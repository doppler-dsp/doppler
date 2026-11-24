#!/usr/bin/env bash
# tui-snapshot-check.sh -- read ccsds415-tui --snapshot's output and require
# every panel's label, each with a value of its kind, once per carrier.
#
# Usage: tui-snapshot-check.sh <snapshot output file> <carriers>
#
# The TUI's panels are its contract with a reader: a label with no value, or
# a value that is not the kind the label promises, is a broken panel even
# when the program exits 0. So each label is matched with the value pattern
# it must carry, and counted: it must appear once per carrier.
set -euo pipefail
out="$1"
want="$2"
die() { echo "FAIL: $*" >&2; exit 1; }

num='-?[0-9]+(\.[0-9]+)?'
# label | the value that must follow it on its line
rows=(
    "processing time per sample|$num ns"
    "over the run so far|$num ns"
    "sample period|$num ns"
    "share of the sample period|$num %"
    "times faster than real time|$num"
    "display time per refresh|$num ms"
    "state|(searching|refining|tracking|idle|lost)"
    "code lock|(up|down)"
    "symbol lock|(up|down)"
    "lock metric|$num"
    "threshold|$num"
    "C/N0|$num dB-Hz"
    "Doppler|$num Hz"
    "code rate \(received / nominal\)|$num"
    "carrier|$num GHz"
    "time|$num s"
    "symbols|[0-9]+"
    "start sequence|(tracking|searching)"
    "frames read|[0-9]+"
    "CRC passed|[0-9]+"
    "CRC failed|[0-9]+"
    "start sequences missed|[0-9]+"
    "last frame's data|[0-9]+ octets"
    "SCID|[0-9]+"
    "VCID|[0-9]+"
    "frame sequence number|[0-9]+"
    "polarity|(upright|inverted)"
)
# The terminal's escapes and box drawing are not the panel's text.
plain="$(sed -e 's/\x1b\[[0-9;?]*[a-zA-Z]//g' -e 's/[│╭╮╰╯├┤─]/ /g' "$out")"
for r in "${rows[@]}"; do
    label="${r%%|*}"
    value="${r#*|}"
    # Two panels share a line, so a label is found anywhere on it, after a
    # space or at its start.
    # No match is an answer (0), not an error: die names the row below.
    n="$({ grep -oE "(^| )${label} +${value}( |$)" <<<"$plain" || true; } \
        | wc -l)"
    [ "$n" -eq "$want" ] \
        || die "panel row '$label' with a value like /$value/: found $n, want $want"
done
echo "tui-snapshot-check: ${#rows[@]} rows, each with its value, on $want carrier(s)"
