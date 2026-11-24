#!/usr/bin/env bash
# readme-commands.sh -- run every `make` command an example's README shows,
# as written, in the example's directory; then the expected failures.
#
# Usage: readme-commands.sh <example dir>
#   with BUILD_DIR, PREFIX, OUT and PYTHON in the environment: the example's
#   Makefile reads each with ?=, so the commands run unchanged.
#
# A page states what works, so every command on it must exit 0. The
# commands are DISCOVERED -- each `make ...` in a ```sh fence and each in an
# inline code span -- so a command added to the page is run the day it is
# added. A command this script cannot run headless is skipped by name
# below, with its reason; nothing else is.
#
# EXPECTED_FAIL rows are commands the page does NOT show: a known defect's
# reproducer, required to fail WITH the line its defect prints (any other
# failure is red). One that starts to pass is red too, so the row is
# removed the day the defect is fixed.
set -euo pipefail
dir="$1"
readme="$dir/README.md"
die() { echo "FAIL: $*" >&2; exit 1; }
[ -f "$readme" ] || die "no README at $readme"

# command | issue whose defect makes it fail | the line it must print
EXPECTED_FAIL=(
    "make run ARGS='--carrier lo --esn0 6'|#3349|FAIL: \[ccsds415-2.2GHz\] BER"
)

# Every `make ...` on the page: fenced sh lines (a trailing comment cut), and
# inline code spans.
mapfile -t cmds < <(
    {
        awk '/^```sh/{f=1; next} /^```/{f=0} f && /^make /' "$readme" \
            | sed -e 's/[[:space:]]*#.*$//'
        grep -o '`make [^`]*`' "$readme" | tr -d '`'
    } | sed -e 's/[[:space:]]*$//' | awk 'NF && !seen[$0]++'
)
[ "${#cmds[@]}" -gt 0 ] || die "no make command found in $readme"

ran=0
for c in "${cmds[@]}"; do
    case "$c" in
        "make package-linux-smoke" | "make rpm-consumer-check")
            echo "skip: $c -- the repository's target, run by CI as itself"
            continue ;;
        "make tui"*)
            echo "skip: $c -- needs a terminal; its --snapshot and q run in" \
                 "package-linux-smoke's Fedora leg"
            continue ;;
    esac
    # The page's placeholder is the prefix this run installed to.
    c="${c//<prefix>/$PREFIX}"
    echo ">> $c"
    # `make plot` shows its figures; headless, matplotlib's Agg draws them
    # and show() returns, so the command runs as written.
    (cd "$dir" && MPLBACKEND=Agg eval "$c") || die "the README's '$c' exited $?"
    ran=$((ran + 1))
done

for row in "${EXPECTED_FAIL[@]}"; do
    IFS='|' read -r c issue why <<<"$row"
    grep -qF "$c" "$readme" \
        && die "'$c' fails ($issue) and the README shows it"
    echo ">> $c (expected to fail: $issue)"
    if out="$(cd "$dir" && eval "$c" 2>&1)"; then
        die "'$c' passed: $issue looks fixed, so remove its EXPECTED_FAIL row" \
            "(tests/install/readme-commands.sh)"
    fi
    grep -qE "$why" <<<"$out" \
        || die "'$c' failed, but not with /$why/ ($issue's line):" \
               "$(tail -3 <<<"$out")"
done
echo "readme-commands: $ran README command(s) exit 0;" \
     "${#EXPECTED_FAIL[@]} expected failure(s) still fail"
