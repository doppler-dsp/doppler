#!/usr/bin/env bash
#
# pr-watch.sh — report what a PR's checks did. It NEVER authorizes a merge.
#
# The merge gate is `gh pr merge --auto`: GitHub evaluates the repo's required
# check set server-side and merges when it is satisfied. That is the real fix
# for everything below — a watcher that decides "looks green, merge it" is
# re-implementing a gate that already exists and cannot be got wrong. Arm
# auto-merge first; use this only to find out whether the PR landed or is
# genuinely stuck, so a failure is noticed rather than waited on forever.
#
# It still has to be careful, because a watcher that lies about the outcome
# sends you looking in the wrong place. Every hand-rolled version of this poll
# fails TOWARD green, silently. Three ways, all hit for real on this repo:
#
#   1. Watching a run id picked by recency. `gh run list -L 1` returns the
#      newest run of ANY workflow on the branch, so it happily reports the
#      docker or artifact workflow finishing while the test matrix is still
#      queued. Anchoring to the PR's head SHA is the fix — a run id is not the
#      thing you care about, the commit's check set is.
#   2. Inferring completion from the ABSENCE of a marker. Right after a
#      force-push GitHub has not created the check runs yet, so `grep pending`
#      finds nothing and the loop exits declaring victory with zero checks run.
#      A trailing blank line does the same to a naive `grep -v pending`.
#   3. Trusting a verdict that predates the push. `gh pr checks --watch` can
#      attach to the PRIOR run after a re-push and render its old conclusions
#      instantly, which looks like a very fast pass.
#
# So: bind to the head SHA, require a non-empty and fully-settled check set,
# and re-anchor if the SHA moves underneath us rather than reporting a result
# for a commit that is no longer the PR.
#
# A fourth way is GitHub's, not the watcher's, and waiting cannot fix it
# (just-buildit/just-makeit#1812). A PR gets a new head while its previous
# run is still QUEUED, with no runner yet. The new head's run, in the same
# concurrency group, goes `pending` with ZERO jobs and stays there:
# `cancel-in-progress` did not cancel the queued run, and `gh run cancel`
# left it queued too. Only `POST .../actions/runs/<id>/force-cancel` freed
# it. Nothing is red, so auto-merge waits forever and so would this. Worse,
# a stuck workflow reports no checks at all, so the OTHER workflows can
# settle green and read as "all settled green" while the required one never
# started. So every poll asks `stuck_runs` for that shape, names both runs
# and the force-cancel command, and never reports green while it holds.
# REPORT-ONLY: cancelling a run is a decision, and this script makes none.
#
# A gh that cannot answer is not a PR with nothing to report
# (just-buildit.github.io#113). `gh pr checks --json` arrived in gh 2.50.0,
# and Debian 13 packages 2.46.0, which rejects the flag. With gh's
# stderr discarded, its empty answer read as "no checks yet" and the
# watcher waited out its whole timeout, as an auth error did too. So every
# read is judged by gh's EXIT CODE, never by an empty answer: a gh too old
# or not authenticated stops at once, naming the fix, and any other error
# is named while it waits. A failed read is never a verdict, green or not.
#
# Advisory checks that must not block a merge are named in ADVISORY (a
# comma-separated list of check names, default `codecov/patch`). They are
# reported but never fatal.
#
# REPO DERIVES from the checkout rather than being a required env var: a
# watcher that needs the answer passed in is one somebody eventually points at
# the wrong repository, and the checkout already knows. Override it for a
# cross-repo watch; otherwise it is correct by construction.
#
# This file is CANONICAL. Consuming repos vendor it verbatim and hold it here
# with `standard-check` (see standard.mk's VENDORED_FILES). Edit it here and
# re-vendor; a local edit is the fork that gate exists to prevent.
#
# Usage:  make pr-watch PR=<n>            # the way to run it
#         scripts/pr-watch.sh <pr-number>
#         REPO=owner/name ADVISORY="codecov/patch,..." scripts/pr-watch.sh <n>
# Exit:   0 settled green (or merged) · 1 real failure · 2 no verdict:
#         timed out, or gh cannot answer (missing, too old, not authenticated)
# Needs:  gh >= 2.50.0, the first with `gh pr checks --json` (cli/cli#9079)
# Pair:   gh pr merge <n> --auto --rebase   # the gate; this is the report
# See:    skills://merge-set
set -uo pipefail

# owner/name from the origin remote, SSH or HTTPS, with any .git suffix cut.
derive_repo() {
  git remote get-url origin 2>/dev/null \
    | sed -E 's#^git@[^:]+:##; s#^https?://[^/]+/##; s#\.git$##'
}

# Progress goes through here so a watcher can be attached to a notifier
# without emitting one message per poll. Repeating "30/33 settled" every 40s
# is noise that trains you to ignore the channel the real result arrives on.
say() {
  local now
  now=$(date +%s)
  if [ "$QUIET" = "1" ]; then
    [ "$1" = "$last" ] && return 0
    if [ $(( now - last_said )) -lt "$PROGRESS_EVERY" ]; then
      last="$1"; return 0
    fi
  fi
  last="$1"; last_said="$now"; echo "  $1"
}

# Every query goes through `gh --jq`, which is gh's OWN embedded jq. Do NOT
# pipe to the external `jq` binary: it is absent on some of these machines, and
# the failure is silent inside a poll loop — the captured variable comes back
# empty, no branch matches, and the watcher spins until timeout reporting
# nothing. Two monitors were lost to exactly that before this script existed.
# GH names the binary so the tests can answer from a recorded fixture.
#
# gh's stderr goes to GH_ERR, which the main loop sets, rather than away: a
# failed call and an empty answer look alike in a variable, and gh_failed
# needs what gh SAID to tell "too old" from "no checks yet" (#113).
# Sourced, as the tests do, it is discarded as before.
q() { "${GH:-gh}" "$@" 2>"${GH_ERR:-/dev/null}"; }

# The wait for a check set that does not exist yet. NOT a green one: this is
# failure mode 2 above, and it is the whole reason this script exists.
no_checks_yet() {
  say "no checks reported yet for ${sha:0:9} — waiting (not green)"
  sleep "$INTERVAL"
}

# gh_failed RC WHAT -- a q call exited RC while trying to WHAT. Stop if
# waiting cannot change gh's answer; otherwise say what gh said and wait
# one interval, and the caller polls again:
#   x=$(q ...) || { gh_failed $? "read the checks"; continue; }
# Never inside $(...): its `exit` must end the script, not a subshell.
gh_failed() {
  local rc="$1" what="$2" said version
  said=$(head -n 1 "${GH_ERR:-/dev/null}" 2>/dev/null)
  case "$rc:$said" in
    4:* | *"HTTP 401"*)   # 4 is gh's own exit code for an auth failure
      echo "::error:: gh cannot authenticate: ${said:-exit $rc}"
      echo "  Waiting cannot fix this; see \`gh auth status\`."
      exit 2 ;;
    *"unknown flag: "*)
      version=$("${GH:-gh}" --version 2>/dev/null | head -n 1)
      echo "::error:: gh rejected a flag this script needs: $said"
      echo "  Installed: ${version:-gh (version unreadable)}"
      echo "  Needed:    gh >= 2.50.0, the first with \`gh pr checks --json\`"
      echo "             (cli/cli#9079). Waiting cannot fix this: upgrade gh."
      exit 2 ;;
    *"no checks reported"*)   # how gh >= 2.50 answers for an empty set
      no_checks_yet ;;
    *)
      said="gh exited $rc: ${said:-(no message)}"
      say "cannot $what: $said — retrying (not green)"
      sleep "$INTERVAL" ;;
  esac
}

# Is $1 in the comma-separated ADVISORY list?
advisory() {
  local name="$1" item
  local IFS=,
  for item in $ADVISORY; do [ "$name" = "$item" ] && return 0; done
  return 1
}

# stuck_runs REPO SHA BRANCH -- the #1812 shape, one line per stuck run:
#   "<blocked run id> <superseded run id> <superseded head sha> <workflow>"
# A run is stuck when ALL of these hold:
#   1. it is on the PR's head SHA and its status is `pending`;
#   2. it has zero jobs (the second call, made only for a candidate);
#   3. a run of the SAME workflow, on the same branch, for ANOTHER head SHA,
#      created EARLIER, is still `queued`.
# Each condition alone is normal. A run waits `pending` behind an IN-PROGRESS
# run of its group whenever cancel-in-progress is off, and a just-created
# run has zero jobs for a moment; neither is stuck while nothing older is
# queued. Runs are matched by branch and event, not by `pull_requests`:
# both real cases had that list EMPTY.
# A failed read returns gh's exit code: "could not look" is not "nothing is
# stuck", which would let the loop call the other workflows' checks green.
stuck_runs() {
  local repo="$1" sha="$2" branch="$3" line new jobs runs
  [ -n "$sha" ] && [ -n "$branch" ] || return 0
  # The jq is single-quoted on purpose: `$r`/`$n` are jq's, only the SHA
  # is spliced in (hex, so it cannot break the quoting).
  # shellcheck disable=SC2016
  runs=$(q api \
    "repos/$repo/actions/runs?branch=$branch&event=pull_request&per_page=50" \
    --jq '.workflow_runs as $r
      | $r[] | select(.head_sha == "'"$sha"'" and .status == "pending")
      | . as $n
      | $r[]
      | select(.workflow_id == $n.workflow_id
               and .head_sha != "'"$sha"'"
               and .status == "queued"
               and .created_at < $n.created_at)
      | "\($n.id) \(.id) \(.head_sha[0:9]) \(.name)"') || return
  while IFS= read -r line; do
    [ -z "$line" ] && continue
    new="${line%% *}"
    jobs=$(q api "repos/$repo/actions/runs/$new/jobs" --jq '.total_count') \
      || return
    if [ "$jobs" = "0" ]; then echo "$line"; fi
  done <<<"$runs"
}

# report_stuck REPO LINE -- what to do about one stuck_runs line.
report_stuck() {
  local repo="$1" new old oldsha name
  read -r new old oldsha name <<<"$2"
  echo "::warning:: run $new ($name) is pending with 0 jobs: run $old,"
  echo "  for the superseded head $oldsha, is still QUEUED in its"
  echo "  concurrency group and cancel-in-progress did not cancel it"
  echo "  (just-buildit/just-makeit#1812). Waiting will not fix this."
  echo "  \`gh run cancel $old\` may leave it queued; this frees it:"
  echo "    gh api -X POST repos/$repo/actions/runs/$old/force-cancel"
}

# Sourced (the tests do), the functions above are all there is.
if [ "${BASH_SOURCE[0]}" != "$0" ]; then return 0; fi

PR="${1:?usage: make pr-watch PR=<n>   (or scripts/pr-watch.sh <pr-number>)}"

REPO="${REPO:-$(derive_repo)}"
if [ -z "$REPO" ]; then
  echo "::error:: no origin remote, and REPO is unset — run this from a" >&2
  echo "  checkout, or pass REPO=owner/name explicitly." >&2
  exit 2
fi
ADVISORY="${ADVISORY:-codecov/patch}"
INTERVAL="${INTERVAL:-40}"
QUIET="${QUIET:-1}"        # 1 = throttle progress; the result always prints
PROGRESS_EVERY="${PROGRESS_EVERY:-300}"   # seconds between progress lines
TIMEOUT_MIN="${TIMEOUT_MIN:-60}"

deadline=$(( $(date +%s) + TIMEOUT_MIN * 60 ))
anchor=""
sha=""
last=""
last_said=0
reported=""   # stuck_runs lines already reported, so each prints once

command -v "${GH:-gh}" >/dev/null || { echo "::error:: gh not on PATH"; exit 2; }

# gh's stderr from the latest q call, for gh_failed to read.
GH_ERR=$(mktemp)
trap 'rm -f "$GH_ERR"' EXIT

echo "pr-watch: $REPO #$PR  (advisory: $ADVISORY)"

while :; do
  if [ "$(date +%s)" -ge "$deadline" ]; then
    echo "::error:: timed out after ${TIMEOUT_MIN}m — checks never settled"
    exit 2
  fi

  state=$(q pr view "$PR" -R "$REPO" --json state --jq .state) \
    || { gh_failed $? "read the PR"; continue; }
  sha=$(q pr view "$PR" -R "$REPO" --json headRefOid --jq .headRefOid) \
    || { gh_failed $? "read the PR"; continue; }
  if [ -z "$state" ] || [ -z "$sha" ]; then
    say "cannot read PR (transient?) — retrying"; sleep "$INTERVAL"; continue
  fi

  case "$state" in
    MERGED) echo "#$PR is MERGED"; exit 0 ;;
    CLOSED) echo "::error:: #$PR is CLOSED without merging"; exit 1 ;;
  esac

  # Re-anchor on a force-push instead of reporting the old commit's verdict.
  if [ -z "$anchor" ]; then
    anchor="$sha"; echo "  anchored to ${sha:0:9}"
  elif [ "$sha" != "$anchor" ]; then
    echo "  head moved ${anchor:0:9} -> ${sha:0:9} (force-push) — re-anchoring"
    anchor="$sha"
  fi

  # The #1812 shape. Reported once per pair, never through `say`'s
  # throttle, and it holds off a green verdict: the stuck workflow has
  # reported no checks, so "all settled" would be about the others.
  branch=$(q pr view "$PR" -R "$REPO" --json headRefName --jq .headRefName) \
    || { gh_failed $? "read the PR"; continue; }
  stuck=$(stuck_runs "$REPO" "$sha" "$branch") \
    || { gh_failed $? "look for stuck runs"; continue; }
  while IFS= read -r line; do
    [ -z "$line" ] && continue
    case $'\n'"$reported"$'\n' in *$'\n'"$line"$'\n'*) continue ;; esac
    report_stuck "$REPO" "$line"
    reported="$reported"$'\n'"$line"
  done <<<"$stuck"

  # gh >= 2.50 answers an empty set with an error, which gh_failed
  # recognises; a gh that answers it with `[]` is caught here.
  n=$(q pr checks "$PR" -R "$REPO" --json name --jq 'length') \
    || { gh_failed $? "read the checks"; continue; }
  if [ -z "$n" ] || [ "$n" -eq 0 ]; then no_checks_yet; continue; fi

  pending=$(q pr checks "$PR" -R "$REPO" --json bucket \
            --jq '[.[] | select(.bucket=="pending")] | length') \
    || { gh_failed $? "read the checks"; continue; }
  if [ -n "$pending" ] && [ "$pending" -gt 0 ]; then
    say "$(( n - pending ))/${n} settled for ${sha:0:9}…"
    sleep "$INTERVAL"; continue
  fi

  # Fully settled. Split real failures from advisory ones -- read first,
  # so a failed read is not an empty list of failures.
  failing=$(q pr checks "$PR" -R "$REPO" --json name,bucket \
            --jq '.[] | select(.bucket=="fail") | .name') \
    || { gh_failed $? "read the checks"; continue; }
  real=(); adv=()
  while IFS= read -r f; do
    [ -z "$f" ] && continue
    if advisory "$f"; then adv+=("$f"); else real+=("$f"); fi
  done < <(sort -u <<<"$failing")

  [ "${#adv[@]}" -gt 0 ] && echo "  advisory (not blocking): ${adv[*]}"

  if [ "${#real[@]}" -gt 0 ]; then
    echo "::error:: #$PR has failing checks on ${sha:0:9}: ${real[*]}"
    exit 1
  fi

  if [ -n "$stuck" ]; then
    say "${n} checks settled, but a workflow is stuck (above) — not green"
    sleep "$INTERVAL"; continue
  fi

  echo "#$PR: all $n checks settled green on ${sha:0:9}"
  exit 0
done
