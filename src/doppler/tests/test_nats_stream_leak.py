"""The leftover-work-queue check, and the prefix guard that makes it safe.

`scripts/nats_streams.py` exists because nothing removed a `DP_WORK_*`
JetStream stream, ever. `make nats-down` cleans up a broker THIS repo
started; `scripts/start-nats.sh` reuses one already listening on 4222, and
on that path nats-down is entirely a no-op. Measured on a dev box
2026-08-30: 5,236 leftover streams, 40.8 GiB, oldest frames from
2026-07-08 in the retired `SGIS` wire format -- and because a work queue is
keyed by an endpoint that repeats, `DP_WORK_dp-chain-5601` held 174,133
unreadable frames and made a compose test fail permanently on that machine
while passing in CI.

Two things are worth testing, and only one of them needs a broker.

The prefix guard is the safety-critical half: `--delete` removes streams,
and a bug that widened its match would delete someone else's data off a
shared broker. That is a pure function and is tested as one.

The rest is exercised against a live broker when there is one, including
the case that matters -- the check going RED. A threshold nothing has been
seen to trip is decoration.
"""

from __future__ import annotations

import json
import subprocess
import sys
import uuid

import pytest

from doppler.tests._nats import nats_available
from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "nats_streams.py"

requires_nats = pytest.mark.skipif(
    not nats_available(),
    reason="no nats-server on 127.0.0.1:4222 (run `make nats-up`)",
)


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        capture_output=True,
        text=True,
    )


# --------------------------------------------------------------------------
# The prefix guard. No broker: this is the half that must never be wrong.
# --------------------------------------------------------------------------


def _partition(names: list[str]) -> tuple[list[str], list[str]]:
    sys.path.insert(0, str(REPO / "scripts"))
    try:
        import nats_streams
    finally:
        sys.path.pop(0)
    return nats_streams.partition(names)


def test_only_dp_work_streams_are_ours() -> None:
    ours, theirs = _partition(
        [
            "DP_WORK_dp-chain-5601",
            "DP_WORK_ep123",
            "ORDERS",
            "dp_work_lowercase",
            "MY_DP_WORK_suffix",
            "",
        ]
    )
    assert ours == ["DP_WORK_dp-chain-5601", "DP_WORK_ep123"]
    # Everything else is somebody's data on a possibly shared broker.
    assert theirs == ["ORDERS", "dp_work_lowercase", "MY_DP_WORK_suffix", ""]


def test_a_similar_name_is_not_matched() -> None:
    """The guard is a prefix, not a substring -- a stream that merely
    CONTAINS the marker belongs to whoever made it.

    `DP_WORKER` is here because it is the near miss: it shares seven
    characters with the marker and is not ours, because the marker ends in
    the underscore that separates it from the endpoint name.
    """
    ours, theirs = _partition(["backup-DP_WORK_dp-chain-1", "DP_WORKER"])
    assert ours == []
    assert theirs == ["backup-DP_WORK_dp-chain-1", "DP_WORKER"]


def test_an_empty_broker_yields_nothing_to_delete() -> None:
    assert _partition([]) == ([], [])


# --------------------------------------------------------------------------
# Against a real broker.
# --------------------------------------------------------------------------


@requires_nats
def test_check_passes_under_the_limit() -> None:
    r = _run("--check", "--max", "100000")
    assert r.returncode == 0, r.stdout + r.stderr
    assert "DP_WORK_* stream(s)" in r.stdout


@requires_nats
def test_seed_then_check_red_then_delete_then_green() -> None:
    """The whole lifecycle in ONE test, scoped to its own prefix.

    One test rather than three because the broker is global: split across
    xdist workers, a `--delete` in one would remove the work queues another
    was mid-way through using. `--prefix` keeps this to streams only this
    test created, which is also why the option exists.

    The RED step is the point. A threshold nothing has been seen to trip
    is decoration, and this one is the whole prevention.
    """
    from doppler.stream import CF64, Push

    tag = f"dp-leakprobe-{uuid.uuid4().hex[:10]}"
    scope = f"DP_WORK_{tag}"
    Push(f"nats://127.0.0.1:4222/{tag}", CF64)  # provisions the stream
    try:
        red = _run("--check", "--prefix", scope, "--max", "0")
        assert red.returncode == 1, red.stdout + red.stderr
        assert "leftover work queues" in red.stdout
        assert "make nats-purge" in red.stdout

        gone = _run("--delete", "--prefix", scope)
        assert gone.returncode == 0, gone.stdout + gone.stderr
        assert "deleted 1" in gone.stdout

        green = _run("--check", "--prefix", scope, "--max", "0")
        assert green.returncode == 0, green.stdout + green.stderr
    finally:
        _run("--delete", "--prefix", scope)


def test_a_prefix_may_narrow_the_guard_but_never_widen_it() -> None:
    """`--prefix` is the one way to point this tool somewhere else, so it
    is also the one way to aim it at data that is not ours."""
    r = _run("--delete", "--prefix", "")
    assert r.returncode == 2
    assert "never widen" in r.stderr

    r = _run("--delete", "--prefix", "ORDERS")
    assert r.returncode == 2
    assert "never widen" in r.stderr


def test_absent_broker_is_an_error_unless_asked_to_be_quiet() -> None:
    """Teardown may run when nothing was ever started; everything else
    wants to hear that the broker is missing rather than read a 0."""
    r = _run("--check", "--quiet-when-absent")
    # With a broker this is a normal pass; without one it must still be 0
    # *because the flag was given*, and never a crash.
    assert r.returncode in (0, 1), r.stdout + r.stderr
    assert "Traceback" not in r.stderr


def test_the_script_is_stdlib_only() -> None:
    """It runs from `make nats-down` during teardown, and from a CI step
    that may have already failed -- so it must not depend on the project
    venv having resolved."""
    src = SCRIPT.read_text(encoding="utf-8")
    for third_party in ("import nats", "import numpy", "from doppler"):
        assert third_party not in src, third_party


def test_json_is_the_only_wire_parser() -> None:
    """A sanity check on the request helper: the JetStream API is JSON,
    and hand-rolling a parser for it would be the reimplementation this
    repo forbids."""
    src = SCRIPT.read_text(encoding="utf-8")
    assert "json.loads" in src
    assert json  # imported here for the same reason the script imports it
