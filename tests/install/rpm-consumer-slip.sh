#!/usr/bin/env bash
# rpm-consumer-slip.sh -- force whole-symbol slips into the 415.1-B-1
# example's decided stream and require that it reads across them and says so.
#
# Usage: rpm-consumer-slip.sh <ccsds415 binary> <scratch dir>
#
# A receiver that slips whole symbols (#3349) moves every later frame by the
# slip. The example's reader must follow it, read every frame, and report
# the slip; its truth check must place each frame at the period it is,
# within a few symbols, and never blame the reader for it. The binary's
# --drop-symbol and --repeat-symbol force one slip each, inside a frame and
# at a frame's first symbol.
#
# A forced slip fails the BER check, which is true (the meter keeps the one
# alignment it detected), so that FAIL line is expected; any other is not.
set -euo pipefail
bin="$1"
out="$2"
die() { echo "FAIL: $*" >&2; exit 1; }

# option | what it forces
cases=(
    "--drop-symbol 30000|a symbol dropped inside a frame"
    "--repeat-symbol 30000|a symbol repeated inside a frame"
    "--drop-symbol 31210|a symbol dropped at a frame's first symbol"
)
for c in "${cases[@]}"; do
    opt="${c%%|*}"
    what="${c#*|}"
    # shellcheck disable=SC2086  # opt is two words, on purpose
    log="$("$bin" --carrier hi --seconds 20 $opt --out "$out" 2>&1 || true)"
    grep -qE 'frames: ([0-9]+) of the \1 sent in the locked stream read' \
        <<<"$log" || die "$what: not every frame sent was read: $(grep frames: <<<"$log")"
    grep -qE '; 0 verdicts the truth contradicts' <<<"$log" \
        || die "$what: the truth check blamed the reader: $(grep frames: <<<"$log")"
    grep -qE 'slip: [1-9][0-9]* frame\(s\) read shifted .* re-acquired [1-9][0-9]* time\(s\) after a slip' \
        <<<"$log" || die "$what: the slip was not reported by both: $(grep -E 'slip:|frames:' <<<"$log")"
    other="$(grep '^FAIL' <<<"$log" | grep -v '\] BER ' || true)"
    [ -z "$other" ] || die "$what: a failure besides the BER's: $other"
    echo "rpm-consumer-slip: $what: every frame read, the slip reported" \
         "($(grep -oE 'the first by [-+][0-9]+ symbol' <<<"$log"))"
done
