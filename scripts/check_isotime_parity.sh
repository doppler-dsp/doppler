#!/usr/bin/env bash
#
# check_isotime_parity.sh — prove dp_isotime.h still agrees with just-bashit.
#
# native/inc/dp_isotime.h does not define the basic ISO 8601 format; it
# follows one. The definition is just-bashit's `iso-8601-basic`
# (src/just_bashit/datetime.sh), and a bash library and a C header cannot
# share an implementation, so the agreement has to be checked rather than
# claimed.
#
# native/tests/test_dp_isotime.c pins a snapshot of that helper's output as
# golden vectors. A snapshot goes stale silently: if `iso-8601-basic` ever
# changes its contract, the committed vectors keep passing and doppler keeps
# emitting the old spelling while the comment above them still says the two
# match. This script is what makes the claim live — it runs the real helper
# and the real C, on the same instants, and diffs them.
#
# Only the BASIC form is checked, because it is the only one just-bashit
# defines. doppler's extended spelling (DP_ISOTIME_EXTENDED) exists for
# SigMF's `core:datetime`, has no counterpart in the shell library, and is
# anchored instead by test_dp_isotime.c's test_styles_agree.
#
# Exit codes: 0 agree (or reference absent, unless --require), 1 disagree,
# 2 harness problem.
set -euo pipefail

REQUIRE=0
[ "${1:-}" = "--require" ] && REQUIRE=1

BUILD_DIR=${BUILD_DIR:-build}
EMIT="$BUILD_DIR/test_dp_isotime"

die() { printf 'check_isotime_parity: %s\n' "$*" >&2; exit 2; }

[ -x "$EMIT" ] || die "missing $EMIT — run 'cmake --build $BUILD_DIR --target test_dp_isotime' first"

# Locate the reference. JUST_BASHIT wins, then a sibling checkout.
DATETIME_SH=""
for cand in \
    "${JUST_BASHIT:-}/src/just_bashit/datetime.sh" \
    "$HOME/just-bashit/src/just_bashit/datetime.sh"
do
    [ -n "$cand" ] && [ -r "$cand" ] && { DATETIME_SH="$cand"; break; }
done

if [ -z "$DATETIME_SH" ]; then
    if [ "$REQUIRE" = 1 ]; then
        die "just-bashit not found and --require was given (set JUST_BASHIT)"
    fi
    echo "check_isotime_parity: SKIP — just-bashit not found."
    echo "  The golden vectors in native/tests/test_dp_isotime.c still ran."
    echo "  Set JUST_BASHIT=/path/to/just-bashit to check them against the"
    echo "  live helper, or pass --require to make absence an error."
    exit 0
fi

# GNU date is needed both to feed the helper and to split the same instant
# into the (seconds, nanoseconds) pair the C API takes.
DATE_CMD=date
if ! $DATE_CMD -u -d '2026-08-05T04:15:30Z' +%s >/dev/null 2>&1; then
    command -v gdate >/dev/null 2>&1 || die "no GNU date (try gdate)"
    DATE_CMD=gdate
fi

# shellcheck disable=SC1090
source "$DATETIME_SH"
command -v iso-8601-basic >/dev/null 2>&1 \
    || die "sourced $DATETIME_SH but iso-8601-basic is not defined"

# The instants worth disagreeing about: an ordinary one, the epoch, a leap
# day, a fraction that a rounding implementation would carry into the next
# second, and one whose sub-second part is all zeros.
INSTANTS=(
    '2026-08-05T04:15:30.123456789Z'
    '1970-01-01T00:00:00.000000000Z'
    '2024-02-29T23:59:59.999888777Z'
    '2000-01-01T00:00:00.000000001Z'
    '2038-01-19T03:14:07.500000000Z'
)

# Parallel arrays, not an associative one: the seconds case is the helper's
# *absent* flag, and bash rejects "" as an array subscript.
FLAGS=(""  "-m" "-u" "-n")
FRACS=(0   3    6    9)

fails=0
checks=0
for iso in "${INSTANTS[@]}"; do
    sec=$($DATE_CMD -u -d "$iso" +%s)  || die "date rejected $iso"
    nsec=$($DATE_CMD -u -d "$iso" +%N) || die "date rejected $iso"
    nsec=$((10#$nsec))                 # strip leading zeros, keep base 10
    for i in "${!FLAGS[@]}"; do
        flag=${FLAGS[$i]}
        # shellcheck disable=SC2086
        want=$(iso-8601-basic -d "$iso" $flag)
        got=$("$EMIT" --emit "$sec" "$nsec" "${FRACS[$i]}")
        checks=$((checks + 1))
        if [ "$want" != "$got" ]; then
            printf 'MISMATCH  %s %-2s\n  just-bashit: %s\n  dp_isotime : %s\n' \
                   "$iso" "${flag:--s}" "$want" "$got"
            fails=$((fails + 1))
        fi
    done
done

if [ "$fails" -ne 0 ]; then
    cat >&2 <<EOF

check_isotime_parity: FAIL — $fails of $checks disagree.

dp_isotime.h follows just-bashit, not the other way round. Fix the C to
match the helper, then refresh the golden vectors in
native/tests/test_dp_isotime.c from the same run. Do not edit an expected
value to make this pass.
EOF
    exit 1
fi

echo "check_isotime_parity: OK — $checks stamps agree with $DATETIME_SH"
