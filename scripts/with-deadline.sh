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
# ## Why a deadline expiry is NOT retried
#
# Measured on this script's own first live run (doppler#879, CI job
# 96034123946): attempt 1 hit the 300s deadline inside `apt-get`, and attempts
# 2 and 3 then failed in seconds with
#
#     E: Could not get lock /var/lib/dpkg/lock-frontend.
#        It is held by process 2429 (apt-get)
#
# The killed apt-get SURVIVED. jbx runs it under `sudo`, so the process is
# root-owned and an unprivileged group kill gets EPERM — GNU timeout signals
# the group faithfully and the root child simply ignores it. It then holds the
# dpkg lock forever, because the reason it was killed is that it was hung.
#
# So after a deadline expiry the environment is POISONED. Retrying blind is not
# merely wasteful — it is guaranteed to fail and it buries the real cause under
# two lock errors.
#
# **Unless the lock can be RECLAIMED, which on a CI runner it can.** The holder
# is root-owned and a runner gives us root: kill it, remove the lock files, let
# dpkg finish any half-completed transaction, and the next attempt starts
# clean. Gated on `CI` being set and on sudo existing, because clearing dpkg
# locks is a reasonable thing to do to a disposable VM and an unreasonable
# thing to do to somebody's laptop. Off a runner a deadline expiry stays
# terminal.
#
# ## What the retry actually buys, measured
#
# The reasoning above used to end "quite possibly against a different mirror
# node, which is the whole reason a retry is worth having against a stall
# localised to one path." Measured on 2026-08-19 (run 32250340944), that is
# NOT what happens. Python 3.9 stalled on `Get:7 … cmake … [11.2 MB]`, the
# reclaim ran, and attempt 2 stalled on THE SAME cmake from the same host.
# The retry does not move nodes.
#
# What it does buy is that apt keeps its partial downloads across the kill:
# attempt 2 opened with `Need to get 97.4 MB/114 MB` rather than 114 MB. That
# is real and it is why the reclaim is worth having, but it is a weaker claim
# than the one it replaces, and the retry cannot rescue a mirror path that
# stays bad.
#
# The failure is also STOCHASTIC PER RUNNER, which is the sizing fact: in that
# same run 16 of 19 jobs passed this step untouched, and re-running the two
# failures passed clean with no deadline message at all. So a small try count
# is not obviously wrong -- it just cannot help a runner that drew a bad path.
#
# Ordinary failures (a curl that exits non-zero, a transient resolver error)
# leave no lock behind and are retried without any of this, which is the
# 2026-08-07-class flake this repo already knows about.
#
# The earlier reasoning here was that GNU timeout kills the whole process
# group, verified against a `make` over a `sleep`. That verification was sound
# and irrelevant: `sleep` is not run under sudo, so it proved the mechanism on
# a case that does not resemble the one that matters.
#
# ## Portability
#
# `timeout(1)` is coreutils, and MEASURED ABSENT on GitHub's macOS runner —
# neither `timeout` nor `gtimeout` is on PATH there, which the first live run
# of this script reported from the macos-latest job. That is a whole platform,
# so it gets a real deadline rather than a warning: the fallback below is a
# POSIX watchdog with the same contract, including the 124 exit code.
#
# `set -m` is what makes the fallback able to signal a TREE. Without job
# control a background child is not a process-group leader, so `kill -- -$pid`
# has no group to signal and only the sub-make would die. With it, the child
# leads its own group — matching what GNU timeout does for us on Linux.
#
# Neither can kill a `sudo` child, which is why a deadline expiry is terminal
# rather than retried (see above). The signal is still worth sending: it ends
# the sub-make promptly so the step fails at the deadline instead of running
# to the workflow's `timeout-minutes`.
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
    echo "with-deadline: no timeout(1)/gtimeout(1) — using the shell" \
         "watchdog for: $*" >&2
fi

# The fallback. Same contract as timeout(1): 0 on success, the command's own
# code on failure, 124 when the deadline expires.
run_with_watchdog() {
    set -m                      # child leads its own process group (see above)
    "$@" &
    _cmd=$!
    set +m

    # SIGKILL follows SIGTERM after a grace period, matching `timeout -k 10`,
    # so a child that traps the first signal cannot make the deadline hang.
    (
        sleep "$deadline"
        kill -TERM -"$_cmd" 2>/dev/null || kill -TERM "$_cmd" 2>/dev/null
        sleep 10
        kill -KILL -"$_cmd" 2>/dev/null || kill -KILL "$_cmd" 2>/dev/null
    ) &
    _watch=$!

    _rc=0
    wait "$_cmd" 2>/dev/null || _rc=$?
    kill "$_watch" 2>/dev/null || :
    wait "$_watch" 2>/dev/null || :

    # A killed child reports 143 (128+TERM) or 137 (128+KILL). Report the
    # deadline's own 124 instead, so a caller reading this script's exit code
    # cannot tell the two implementations apart.
    case "$_rc" in
        143|137) return 124 ;;
        *)       return "$_rc" ;;
    esac
}

# Reclaim a package manager that outlived its kill. Disposable CI only; see
# the header. Non-zero means "not safe or not possible here", which leaves a
# deadline expiry terminal.
reclaim_pkg_manager() {
    [ -n "${CI:-}" ] || return 1
    command -v sudo    >/dev/null 2>&1 || return 1
    command -v apt-get >/dev/null 2>&1 || return 1

    echo "with-deadline: reclaiming the package manager (CI, root available)" >&2
    sudo pkill -KILL -x apt-get 2>/dev/null || :
    sudo pkill -KILL -x apt     2>/dev/null || :
    sudo pkill -KILL -x dpkg    2>/dev/null || :
    sudo rm -f /var/lib/dpkg/lock-frontend \
               /var/lib/dpkg/lock \
               /var/lib/apt/lists/lock \
               /var/cache/apt/archives/lock 2>/dev/null || :
    # Finish whatever the kill interrupted, or the next apt-get refuses to run.
    sudo dpkg --configure -a >/dev/null 2>&1 || :
    return 0
}

attempt=1
while :; do
    # `set -e` must not kill us on a failed attempt: the retry IS the point.
    rc=0
    if [ -n "$wrap" ]; then
        # shellcheck disable=SC2086  # $wrap is a deliberate multi-word prefix
        $wrap "$@" || rc=$?
    else
        run_with_watchdog "$@" || rc=$?
    fi
    [ "$rc" -eq 0 ] && exit 0

    # 124 is timeout(1)'s "deadline expired" — worth naming, because it is the
    # case this script exists for and it reads as an ordinary failure
    # otherwise.
    if [ "$rc" -eq 124 ]; then
        why="exceeded the ${deadline}s deadline"
    else
        why="failed (rc=$rc)"
    fi

    # A deadline expiry leaves a root-owned package manager holding the dpkg
    # lock, so a blind retry cannot succeed. Reclaim first; where that is not
    # possible, stop rather than pile lock errors on top of the real cause.
    if [ "$rc" -eq 124 ] && [ "$attempt" -lt "$tries" ]; then
        if ! reclaim_pkg_manager; then
            echo "with-deadline: attempt $attempt/$tries $why — NOT retrying." >&2
            echo "  A killed package manager keeps its lock (root-owned, so" \
                 "it outlives the signal) and this is not a CI runner where" \
                 "it can be reclaimed, so every further attempt would fail on" \
                 "the lock instead of on the real cause." >&2
            exit "$rc"
        fi
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
