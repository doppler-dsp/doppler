#!/usr/bin/env bash
#
# release-watch.sh — autonomously see a tagged release through to verified.
#
# Given a version, finds its release.yml run, streams job outcomes, recovers
# from ONE pre-publish flake (rerun a failed job, or cancel+rerun a hung one),
# and verifies the real artifacts a consumer touches: PyPI (per-version
# endpoint, then `latest`, which lags 30-60s), the GitHub Release (published,
# not a draft, has notes), and — doppler being a C+Python project — the three
# pre-built C-library tarballs attached as release assets.
#
# The recovery is SAFE because release.yml gates `publish-python` behind
# `smoke-wheel`: while the PyPI publish has not succeeded, nothing has reached
# PyPI, so a rerun cannot double-publish. Once it succeeds we never rerun — a
# later failure (container / github-release / post-release smoke) is a real
# problem for a human, and re-uploading an existing version would fail.
#
# Usage:  REPO=owner/name scripts/release-watch.sh <x.y.z>
# See:    skills://release-process
set -uo pipefail

VERSION="${1:?usage: REPO=owner/name release-watch.sh <x.y.z>}"
REPO="${REPO:?set REPO=owner/name}"
TAG="v$VERSION"
PKG="doppler-dsp"
HANG_MIN="${HANG_MIN:-30}"   # a pre-publish job in_progress this long = hung
RETRIED=0

echo "release-watch: $REPO $TAG"

# The PyPI publish already succeeded? Recovery is only safe while this is false.
# Match the PyPI publish job SPECIFICALLY — "Publish container image" runs later
# and must not be read as "PyPI is live".
published() {
  gh run view "$RUN" -R "$REPO" --json jobs --jq \
    '[.jobs[] | select(.name|test("publish to pypi";"i"))
              | select(.conclusion=="success")] | length' \
    2>/dev/null | grep -q '^[1-9]'
}

# ── 1. Find the release run for the tag (it may lag the push by a few seconds).
RUN=""
for i in $(seq 1 12); do
  RUN=$(gh run list --workflow=release.yml -R "$REPO" -L 15 \
        --json databaseId,headBranch \
        --jq "[.[] | select(.headBranch==\"$TAG\")][0].databaseId" 2>/dev/null)
  [ -n "$RUN" ] && [ "$RUN" != "null" ] && break
  echo "  waiting for release run to appear ($i/12)…"; sleep 10
  RUN=""
done
if [ -z "$RUN" ]; then echo "::error:: no release.yml run found for $TAG"; exit 1; fi
echo "  run: $RUN  (https://github.com/$REPO/actions/runs/$RUN)"

# Jobs that are NOT eligible for a pre-publish cancel+rerun: the publish jobs
# themselves and everything downstream of them. "Smoke-test the built wheel" is
# pre-publish and IS eligible; "Post-release C smoke" is not (it runs after
# publish) — hence the precise "post-release" exclusion.
POST='publish|post-release|github release|create github|container|docker|manifest'

# ── 2. Watch, recovering from one pre-publish flake or hang.
declare -A SEEN
while true; do
  J=$(gh run view "$RUN" -R "$REPO" --json status,conclusion,jobs 2>/dev/null)
  [ -z "$J" ] && { sleep 15; continue; }

  # Report newly-completed jobs (docker manifest jobs are noise here).
  while IFS=$'\t' read -r name concl; do
    [ -z "$name" ] && continue
    if [ -z "${SEEN[$name]:-}" ]; then echo "  - $name: $concl"; SEEN[$name]=1; fi
  done < <(echo "$J" | jq -r '.jobs[]
            | select(.status=="completed")
            | select((.name|test("docker|manifest";"i"))|not)
            | "\(.name)\t\(.conclusion)"')

  STATUS=$(echo "$J" | jq -r '.status')
  CONCL=$(echo "$J" | jq -r '.conclusion // ""')

  if [ "$STATUS" = "completed" ]; then
    [ "$CONCL" = "success" ] && break
    if [ "$RETRIED" = 0 ] && ! published; then
      echo "  run failed before publish (likely a flake) — rerunning failed jobs once…"
      gh run rerun "$RUN" -R "$REPO" --failed >/dev/null 2>&1 || true
      RETRIED=1; sleep 20; continue
    fi
    echo "::error:: release run concluded '$CONCL' (no safe auto-recovery left)"; exit 1
  fi

  # Hang: a pre-publish job stuck in_progress past the threshold.
  if [ "$RETRIED" = 0 ] && ! published; then
    STUCK=$(echo "$J" | jq -r --argjson lim $((HANG_MIN * 60)) --arg post "$POST" '
      now as $n | .jobs[]
      | select(.status=="in_progress")
      | select((.name|test($post;"i"))|not)
      | select(.startedAt!=null)
      | select(($n - (.startedAt|fromdate)) > $lim)
      | .name' 2>/dev/null | head -1)
    if [ -n "$STUCK" ]; then
      echo "  '$STUCK' stuck >${HANG_MIN}m (hung runner) — cancel + rerun once…"
      gh run cancel "$RUN" -R "$REPO" >/dev/null 2>&1 || true
      for _ in $(seq 1 20); do
        [ "$(gh run view "$RUN" -R "$REPO" --json status --jq .status 2>/dev/null)" = "completed" ] && break
        sleep 10
      done
      gh run rerun "$RUN" -R "$REPO" >/dev/null 2>&1 || true
      RETRIED=1; sleep 20; continue
    fi
  fi
  sleep 25
done

echo "  release run is green — verifying published artifacts…"

# ── 3. PyPI: per-version endpoint first (updates first), then `latest` (lags).
ok=0
for i in $(seq 1 12); do
  v=$(curl -s "https://pypi.org/pypi/$PKG/$VERSION/json" | jq -r '.info.version // "absent"' 2>/dev/null)
  [ "$v" = "$VERSION" ] && { ok=1; break; }
  sleep 15
done
[ "$ok" = 1 ] || { echo "::error:: PyPI $PKG $VERSION not present after publish"; exit 1; }
echo "  - PyPI $VERSION present"
for i in $(seq 1 8); do
  l=$(curl -s "https://pypi.org/pypi/$PKG/json" | jq -r '.info.version' 2>/dev/null)
  [ "$l" = "$VERSION" ] && { echo "  - PyPI latest = $VERSION"; break; }
  sleep 15
done

# ── 4. GitHub Release: published, not a draft, has notes (awk extraction ran)
#      AND carries the three C-library tarballs (doppler is C + Python).
rel=$(gh release view "$TAG" -R "$REPO" --json isDraft,body,assets 2>/dev/null)
draft=$(echo "$rel" | jq -r '.isDraft')
notes=$(echo "$rel" | jq -r '.body | length')
tarballs=$(echo "$rel" | jq -r '[.assets[] | select(.name|test("\\.tar\\.gz$"))] | length')
if [ "$draft" = "false" ] && [ "${notes:-0}" -gt 0 ]; then
  echo "  - GitHub Release $TAG published (notes: ${notes} chars)"
else
  echo "::error:: GitHub Release $TAG not clean (draft=$draft, notes=${notes:-0})"; exit 1
fi
if [ "${tarballs:-0}" -ge 3 ]; then
  echo "  - C-library tarballs attached: ${tarballs}"
else
  echo "::error:: expected >=3 C tarballs on $TAG, found ${tarballs:-0}"; exit 1
fi

echo "release-watch: $TAG SHIPPED + VERIFIED"
