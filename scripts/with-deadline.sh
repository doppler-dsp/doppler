#!/bin/sh
# Run a command under a per-attempt deadline, retrying a bounded number of
# times.
#
# ## Why this exists
#
# doppler's dependency provisioning (`make install-deps`, `make
# install-docs-deps`) reaches the network three times — the jbx bootstrap
# `curl … get-jb.sh | bash`, then jbx's own apt/brew — and NONE of those calls
# carries a timeout of its own. `get-jb.sh` makes three curls with neither
# `--retry` nor `--max-time`.
#
# So a stalled mirror does not FAIL the step. It HANGS it, until GitHub's
# 360-minute job limit, reporting `pending` the whole way — which a reader
# cannot tell apart from slow CI. Measured on 2026-08-19: the same commit hung
# 3h45m and then 2h42m at `Install system dependencies`, blocking an otherwise
# green PR, while a sibling branch passed the identical step ninety seconds
# later (doppler#879).
#
# ## Why a retry alone would have been decoration
#
# This repo already runs a retry-on-failure pattern, in
# `.github/actions/setup-uv/action.yml`:
#
#     continue-on-error: true          # attempt 1 only records its outcome
#     if: steps.attempt1.outcome == 'failure'
#
# It keys on the step EXITING non-zero, and it is correct for the incident it
# was built for — a 2026-08-07 fetch that timed out and *returned an error*.
# A hung step never exits, so that same pattern bolted onto provisioning would
# never once fire. It would read as a fix and do nothing, which is the
# "a gate that cannot fail is decoration" failure this repo names elsewhere.
#
# **The deadline is the load-bearing half.** It converts a hang into a failure;
# only then is there anything for a retry to act on. That is why this script
# does both and does them in that order, rather than living as a retry in YAML.
#
# ## Portability
#
# `timeout(1)` is coreutils. It is present on the Linux runners; on macOS it
# may be absent or installed as `gtimeout`. When neither exists the retries
# still run and the deadline is simply not enforced HERE — the workflow steps
# carry `timeout-minutes` as the backstop that cannot be bypassed, so the
# 360-minute case stays closed either way. The degradation is announced rather
# than silent, because a deadline you believe in and do not have is worse than
# one you know you lack.
#
# `-k 10` escalates to SIGKILL ten seconds after SIGTERM, for a child that
# traps or ignores the first. Without it a deadline can itself hang, which
# would reproduce the bug this script exists to fix one level up.
#
# The wrapped command is a sub-make, so what actually needs killing is a
# GRANDCHILD (`curl`, `apt`). Verified rather than assumed, because a leaked
# apt holding the dpkg lock would make every retry fail and turn recovery into
# a slower failure: GNU timeout puts the child in its own process group and
# signals the group, so `timeout 3 make …` with a `sleep 120` under it leaves
# zero survivors.
#
# Usage:  with-deadline.sh <seconds> <tries> <command> [args...]
set -eu

if [ "$#" -lt 3 ]; then
    echo "usage: $0 <seconds> <tries> <command> [args...]" >&2
    exit 2
fi

deadline=$1
tries=$2
shift 2

# Pick a deadline wrapper, or announce that there is none.
if command -v timeout >/dev/null 2>&1; then
    wrap="timeout -k 10 $deadline"
elif command -v gtimeout >/dev/null 2>&1; then
    wrap="gtimeout -k 10 $deadline"
else
    wrap=""
    echo "with-deadline: no timeout(1)/gtimeout(1) on PATH — retrying" \
         "WITHOUT a per-attempt deadline; the workflow's timeout-minutes" \
         "is the only ceiling for: $*" >&2
fi

attempt=1
while :; do
    # `set -e` must not kill us on a failed attempt: the retry IS the point.
    rc=0
    # shellcheck disable=SC2086  # $wrap is a deliberate two-word prefix
    $wrap "$@" || rc=$?
    [ "$rc" -eq 0 ] && exit 0

    # 124 is timeout(1)'s "deadline expired" — worth naming, because it is the
    # case this script exists for and it reads as an ordinary failure
    # otherwise.
    if [ "$rc" -eq 124 ]; then
        why="exceeded the ${deadline}s deadline"
    else
        why="failed (rc=$rc)"
    fi

    if [ "$attempt" -ge "$tries" ]; then
        echo "with-deadline: attempt $attempt/$tries $why; no tries left" >&2
        exit "$rc"
    fi

    # Linear backoff. The failure this guards is a stalled or rate-limited
    # mirror, which clears on the order of seconds; an exponential ramp would
    # mostly buy dead time inside a step that already carries a ceiling.
    backoff=$((attempt * 10))
    echo "with-deadline: attempt $attempt/$tries $why; retrying in ${backoff}s" >&2
    sleep "$backoff"
    attempt=$((attempt + 1))
done
