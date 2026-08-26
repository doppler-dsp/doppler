#!/usr/bin/env bash
# Smoke-run every standalone C example, DISCOVERED rather than listed.
#
# `make test-examples-c` used to iterate a hand-written list of nine binary
# names. The other four compiled, shipped, and were executed by nothing --
# with no reason recorded, nothing failing if a fifth joined them, and
# nothing noticing if one was deleted (gh-863). A C example is documentation
# that claims to be executable, so one that runs nowhere is the shape this
# repo already calls indistinguishable from a gate passing.
#
# Discovery is over `native/examples/*.c`, so a new example is gated the moment it
# exists. Opting one out costs an entry in `native/examples/.examples-skip` with a
# mandatory reason -- the same contract `src/doppler/examples/.examples-skip`
# holds the Python side to, and the mechanism this mirrors.
#
# Four properties, each of which fails rather than warns:
#
#   1. a registry entry naming an example that no longer exists is a stale
#      waiver;
#   2. a registry entry with no reason is an absence with extra steps;
#   3. an example with a source and no binary is the same fail-open bug one
#      layer down, in native/examples/CMakeLists.txt's own hand lists;
#   4. running nothing at all is not a pass -- the trap the glibc and tarball
#      gates were both caught by.
#
# Every run is under a deadline. That is load-bearing, not defensive: the
# failure this gate exists to catch is an example nothing runs, and the
# cheapest way to reintroduce it is an example that runs forever. Without a
# deadline the gate HANGS instead of failing, which reads as "still working"
# until CI's own ceiling kills the job and names the wrong thing.
#
# Usage: smoke-c-examples.sh <bin-dir> <timeout-seconds>
set -uo pipefail

BIN_DIR=${1:?usage: smoke-c-examples.sh <bin-dir> <timeout-seconds>}
TIMEOUT=${2:?usage: smoke-c-examples.sh <bin-dir> <timeout-seconds>}
SRC_DIR=native/examples
REGISTRY=$SRC_DIR/.examples-skip

# The deadline runs through the repo's own wrapper, not bare `timeout`.
# `timeout(1)` is coreutils and is MEASURED ABSENT on GitHub's macOS runner
# -- neither it nor `gtimeout` is on PATH there (scripts/with-deadline.sh
# says so from its own first live run). A bare `timeout` therefore does not
# time anything out on macOS; it fails with 127 and reports the example as
# broken. with-deadline.sh already resolves timeout/gtimeout/a POSIX
# watchdog behind one contract, 124-on-expiry included, so this gate gets a
# real deadline on every platform instead of a Linux-only one.
DEADLINE=$(dirname "$0")/with-deadline.sh

# A "broker:" reason is conditional rather than an exclusion: the example
# runs wherever a NATS broker answers (CI starts one) and is skipped
# elsewhere. Probed once, with bash's own /dev/tcp so nothing needs
# installing -- the same probe scripts/start-nats.sh uses.
broker=0
if (exec 3<>/dev/tcp/127.0.0.1/4222) 2>/dev/null; then
    broker=1
fi

# A read loop rather than `mapfile`: mapfile is bash 4, and the macOS job
# this gate runs on ships bash 3.2. It failed there with `mapfile: command
# not found`, and -- because the failure left `examples` unset rather than
# empty -- the emptiness check below never got to speak; `set -u` reported
# `examples: unbound variable` instead, naming the symptom two lines down
# from the cause.
examples=()
while IFS= read -r _name; do
    examples+=("$_name")
done < <(find "$SRC_DIR" -maxdepth 1 -name '*.c' | sed 's|.*/||; s|\.c$||' |
         sort)

if [ ${#examples[@]} -eq 0 ]; then
    echo "  no native/examples/*.c found -- this gate has not run, so it has"
    echo "  not passed."
    exit 1
fi

# Read the registry once into two parallel arrays; bash 3.2 (macOS) has no
# associative arrays, and this gate runs on the macOS C job.
reg_names=()
reg_reasons=()
if [ -f "$REGISTRY" ]; then
    while IFS= read -r line; do
        case "$line" in ''|'#'*) continue ;; esac
        reg_names+=("$(printf '%s' "$line" | cut -d: -f1 | tr -d '[:space:]')")
        reg_reasons+=("$(printf '%s' "$line" | cut -d: -f2- | sed 's/^ *//')")
    done < "$REGISTRY"
fi

reason_for() {
    local want=$1 i
    for ((i = 0; i < ${#reg_names[@]}; i++)); do
        if [ "${reg_names[$i]}" = "$want" ]; then
            printf '%s' "${reg_reasons[$i]}"
            return 0
        fi
    done
    return 1
}

# ── 1 + 2: the registry must be honest ──────────────────────────────────────
bad=0
for ((i = 0; i < ${#reg_names[@]}; i++)); do
    name=${reg_names[$i]}
    if [ ! -f "$SRC_DIR/$name.c" ]; then
        echo "  REGISTRY  $name is listed in $REGISTRY but"
        echo "            $SRC_DIR/$name.c does not exist -- a stale waiver."
        bad=1
    fi
    if [ -z "${reg_reasons[$i]}" ]; then
        echo "  REGISTRY  $name is listed in $REGISTRY with no reason."
        bad=1
    fi
done
[ $bad -eq 0 ] || { echo "  C example registry is not honest."; exit 1; }

# ── 3 + 4: run everything not waived ────────────────────────────────────────
ran=0
for ex in "${examples[@]}"; do
    if reason=$(reason_for "$ex"); then
        case "$reason" in
            broker:*)
                if [ $broker -eq 0 ]; then
                    printf "  %-26s SKIP (no broker on 127.0.0.1:4222)\n" "$ex"
                    continue
                fi
                ;;
            *)
                printf "  %-26s SKIP (%.46s)\n" "$ex" "$reason"
                continue
                ;;
        esac
    fi

    if [ ! -x "$BIN_DIR/$ex" ]; then
        printf "  %-26s NOT BUILT\n" "$ex"
        echo "            $SRC_DIR/$ex.c exists and produced no binary."
        echo "            Add it to $SRC_DIR/CMakeLists.txt (which carries"
        echo "            hand lists of its own), or to $REGISTRY with a"
        echo "            reason."
        exit 1
    fi

    printf "  %-26s" "$ex"
    if "$DEADLINE" "$TIMEOUT" 1 "$BIN_DIR/$ex" > /dev/null 2>&1; then
        echo "PASS"
        ran=$((ran + 1))
    else
        rc=$?
        if [ $rc -eq 124 ]; then
            echo "FAIL (timed out after ${TIMEOUT}s)"
            echo "            An example that never exits is an example"
            echo "            nothing can gate. Give it an exit condition,"
            echo "            or waive it in $REGISTRY with a reason."
        else
            echo "FAIL (exit $rc)"
        fi
        exit 1
    fi
done

if [ $ran -eq 0 ]; then
    echo "  every discovered example was waived -- this gate has not passed."
    exit 1
fi

echo "  $ran C example(s) ran, each under ${TIMEOUT}s"
