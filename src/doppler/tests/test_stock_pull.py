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

from doppler.tests._platform import skip_without_posix_shell
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
HELPER = REPO / "scripts" / "stock-pull.sh"
IMAGE = "r.io/lib/debian:stable"

#: A docker client that answers `pull` from a script: line N of $PLAN is the
#: Nth call's outcome, `ok` or the error text to print. Every pull is
#: appended to $CALLS, so a test can count them. `image inspect IMG` finds
#: IMG in the local store when it is a line of $PRESENT.
_FAKE = """#!/usr/bin/env bash
if [ "$1 $2" = "image inspect" ]; then
    grep -qxF -- "$3" "$PRESENT" && exit 0
    echo "Error: No such image: $3" >&2; exit 1
fi
[ "$1" = pull ] || { echo "fake docker: pull, image inspect" >&2; exit 2; }
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


def _helper(
    tmp_path: Path,
    plan: list[str],
    *args: str,
    attempts: int = 5,
    present: tuple[str, ...] = (),
):
    # The helper's home is Linux CI, where the stock images are pulled; on
    # Windows a bare `bash` is System32's WSL launcher.
    skip_without_posix_shell("scripts/stock-pull.sh")
    fake = tmp_path / "docker"
    fake.write_text(_FAKE, encoding="utf-8")
    fake.chmod(0o755)
    (tmp_path / "plan").write_text("\n".join(plan) + "\n", encoding="utf-8")
    calls = tmp_path / "calls"
    calls.write_text("", encoding="utf-8")
    (tmp_path / "present").write_text(
        "".join(f"{img}\n" for img in present), encoding="utf-8"
    )
    env = {
        **os.environ,
        "DOCKER": str(fake),
        "PLAN": str(tmp_path / "plan"),
        "CALLS": str(calls),
        "PRESENT": str(tmp_path / "present"),
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


@pytest.mark.parametrize(
    "err",
    [
        # One wording each, so dropping any one from transient() is red.
        "Error response from daemon: Get https://r.io/v2/: net/http: "
        "request canceled",
        "Error response from daemon: Get https://r.io/v2/: "
        "Client.Timeout exceeded while awaiting headers",
        "Error response from daemon: Head https://r.io/v2/lib/debian/"
        "manifests/stable: context deadline exceeded",
    ],
)
def test_a_go_timeout_is_retried(tmp_path: Path, err: str) -> None:
    """Go's own wordings for a timed-out connection, which the header
    promises to retry: a client deadline, a cancelled request or context."""
    r, pulled = _helper(tmp_path, [err, "ok"], IMAGE)
    assert r.returncode == 0, r.stderr
    assert len(pulled) == 2


def test_an_image_already_present_is_not_pulled(tmp_path: Path) -> None:
    """What `docker run` and BuildKit did before the helper: a cached image
    is used, so an offline or proxied box keeps working, and the mirror is
    not asked. The plan would fail any pull."""
    r, pulled = _helper(tmp_path, [MISSING], IMAGE, present=(IMAGE,))
    assert r.returncode == 0, r.stderr
    assert pulled == []
    assert f"{IMAGE} (already present)" in r.stdout


def test_dockerfile_mode_pulls_only_what_is_missing(tmp_path: Path) -> None:
    df = tmp_path / "Dockerfile"
    df.write_text(
        "ARG STOCK_REGISTRY\nFROM ${STOCK_REGISTRY}/debian:bookworm AS b\n"
        "FROM ${STOCK_REGISTRY}/python:3.12-slim\n",
        encoding="utf-8",
    )
    r, pulled = _helper(
        tmp_path,
        ["ok"],
        "--dockerfile",
        str(df),
        present=("r.io/lib/debian:bookworm",),
    )
    assert r.returncode == 0, r.stderr
    assert [p.split()[-1] for p in pulled] == ["r.io/lib/python:3.12-slim"]


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
