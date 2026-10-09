"""scripts/stock-pull.sh: the one stock-image pull, retrying a rate limit.

ECR Public, the mirror STOCK_REGISTRY points at, rate-limits anonymous pulls
per IP, and GitHub runners share IPs: a package leg died on `docker:
toomanyrequests: Rate exceeded` before reaching the code (doppler#1979). The
helper retries what a retry can fix, a rate limit or a transient network
error, with bounded backoff, and fails at once, with the registry's own
words, on anything else.

Each case puts a fake `docker` client in front of the helper (its `DOCKER`
knob), so the registry's behaviour is scripted rather than hoped for, and
`STOCK_PULL_DELAY=0` keeps the backoff from costing the suite real time.
"""

from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
HELPER = REPO / "scripts" / "stock-pull.sh"
IMAGE = "r.io/lib/debian:stable"

#: A docker client that answers `pull` from a script: line N of $PLAN is the
#: Nth call's outcome, `ok` or the error text to print. Every call is
#: appended to $CALLS, so a test can count them.
_FAKE = """#!/usr/bin/env bash
[ "$1" = pull ] || { echo "fake docker: only pull" >&2; exit 2; }
echo "$*" >> "$CALLS"
n=$(wc -l < "$CALLS")
line=$(sed -n "${n}p" "$PLAN")
[ -n "$line" ] || line=$(tail -n1 "$PLAN")
if [ "$line" = ok ]; then echo "${@: -1}"; exit 0; fi
echo "$line" >&2
exit 1
"""

RATE = "Error response from daemon: toomanyrequests: Rate exceeded"
MISSING = (
    "Error response from daemon: manifest for r.io/lib/debian:nope not "
    "found: manifest unknown: manifest unknown"
)


def _helper(tmp_path: Path, plan: list[str], *args: str, attempts: int = 5):
    fake = tmp_path / "docker"
    fake.write_text(_FAKE, encoding="utf-8")
    fake.chmod(0o755)
    (tmp_path / "plan").write_text("\n".join(plan) + "\n", encoding="utf-8")
    calls = tmp_path / "calls"
    calls.write_text("", encoding="utf-8")
    env = {
        **os.environ,
        "DOCKER": str(fake),
        "PLAN": str(tmp_path / "plan"),
        "CALLS": str(calls),
        "STOCK_PULL_DELAY": "0",
        "STOCK_PULL_ATTEMPTS": str(attempts),
        "STOCK_REGISTRY": "r.io/lib",
    }
    r = subprocess.run(
        ["bash", str(HELPER), *args],
        capture_output=True,
        text=True,
        env=env,
    )
    pulled = calls.read_text(encoding="utf-8").splitlines()
    return r, pulled


def test_a_rate_limit_that_clears_is_retried_to_success(
    tmp_path: Path,
) -> None:
    """429 twice, then the registry answers: the pull passes on try 3."""
    r, pulled = _helper(tmp_path, [RATE, RATE, "ok"], IMAGE)
    assert r.returncode == 0, r.stderr
    assert len(pulled) == 3
    assert "retry 2/5" in r.stderr and "retry 3/5" in r.stderr
    assert f"{IMAGE} (attempt 3 of 5)" in r.stdout


def test_a_permanent_failure_fails_at_once_with_the_registrys_words(
    tmp_path: Path,
) -> None:
    """A missing tag is not a rate limit: one call, and docker's message."""
    r, pulled = _helper(tmp_path, [MISSING], "r.io/lib/debian:nope")
    assert r.returncode == 1
    assert len(pulled) == 1, "a permanent failure must not be retried"
    assert "manifest unknown" in r.stderr
    assert "after 1 attempt(s)" in r.stderr


def test_a_rate_limit_that_never_clears_is_bounded(tmp_path: Path) -> None:
    r, pulled = _helper(tmp_path, [RATE], IMAGE, attempts=3)
    assert r.returncode == 1
    assert len(pulled) == 3
    assert "toomanyrequests: Rate exceeded" in r.stderr
    assert "after 3 attempt(s)" in r.stderr


def test_a_digest_with_429_in_it_is_not_a_rate_limit(tmp_path: Path) -> None:
    """The code is matched as a word, so hex that contains it is not one."""
    err = "Error: manifest unknown: sha256:ab429cd"
    r, pulled = _helper(tmp_path, [err], IMAGE)
    assert r.returncode == 1
    assert len(pulled) == 1


def test_dockerfile_mode_pulls_its_stock_froms(tmp_path: Path) -> None:
    """--dockerfile pulls what the FROMs name, through the gate's reader."""
    df = tmp_path / "Dockerfile"
    df.write_text(
        "ARG STOCK_REGISTRY\nFROM ${STOCK_REGISTRY}/debian:bookworm AS b\n"
        "FROM b\nFROM ghcr.io/x/y:1\n"
        "FROM ${STOCK_REGISTRY}/python:3.12-slim\n",
        encoding="utf-8",
    )
    r, pulled = _helper(tmp_path, ["ok"], "--dockerfile", str(df))
    assert r.returncode == 0, r.stderr
    assert [p.split()[-1] for p in pulled] == [
        "r.io/lib/debian:bookworm",
        "r.io/lib/python:3.12-slim",
    ]


@pytest.mark.parametrize("args", [(), ("--dockerfile",)])
def test_no_image_is_a_usage_error(
    tmp_path: Path, args: tuple[str, ...]
) -> None:
    r, pulled = _helper(tmp_path, ["ok"], *args)
    assert r.returncode == 2
    assert pulled == []
