"""`make nats-down` must outlive the broker's shutdown flush.

The recipe stops the JetStream broker and removes its store. `kill` returns
when the signal is QUEUED, not when the process is gone, and a JetStream
shutdown spends the interval that follows writing stream state to disk —
measured against a real nats-server, it stayed alive for ~900 further
`kill -0` polls after `kill` returned. An `rm -rf` racing that empties a
stream's `msgs/` directory and then fails its `rmdir` on a file the broker
re-created in between:

    rm: cannot remove '.../DP_WORK_ep752264292/msgs': Directory not empty

which fails the cleanup step of a job whose tests had passed. It runs under
`if: always()`, so it is also the step that reports after a failure — turning
"the tests failed" into "the cleanup failed too".

The fix is ordering: stop every writer, WAIT for it to be gone, then remove.
This gates that, and it gates it the way the pipefail test gates its scanner
— by driving the real recipe over seeded state that a racing teardown cannot
survive. Deleting the wait loop from the `nats-down` recipe turns this red.

**The writer here is a stand-in, deliberately, and nats-server is not
required.** What is under test is the recipe's ordering, not the broker's
shutdown timing, and a test that needed a broker installed would skip on
every box that lacks one — which for a race that only shows up on a loaded CI
runner is the worst of both worlds. So the fixture is a process that traps
SIGTERM and keeps writing into the store afterwards, which is the one
property of nats-server the recipe has to survive.

**The recipe's first step is aimed away from the suite's broker.** It
purges every `DP_WORK_*` stream (`scripts/nats_streams.py --delete`), and
run against the broker the other xdist workers share, that deleted their
work queues mid-test: a Pull read elsewhere stopped short at whatever frame
it had reached, five times in 200 CI runs (#2319). So the purge is pointed
at a port that refuses connections, `docker` (the recipe removes a
container named `nats`, which on a docker dev box IS the shared broker) is
a stand-in on PATH, and with a broker up, a sentinel work queue on the
shared broker has to survive the recipe.
"""

from __future__ import annotations

import os
import socket
import subprocess
import sys
import textwrap
import uuid
from typing import TYPE_CHECKING

import pytest

from doppler.tests._nats import (
    HOST,
    PORT,
    delete_stream_if_present,
    nats_available,
)
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path

    from doppler.stream import Push

REPO = repo_root(__file__)

#: Enough files that `rm -rf` takes long enough for the writer to land inside
#: its walk. Tuned down from 30k, which reproduced comfortably but cost more
#: than the gate is worth; below ~10k the removal finishes before the writer
#: is scheduled and the test passes for the wrong reason.
SEED_FILES = 12000

#: How long the stand-in keeps writing after SIGTERM. The recipe waits up to
#: 15s (10s graceful, then SIGKILL), so this must be comfortably under that
#: or the gate would be measuring the escalation rather than the wait.
FLUSH_SECONDS = 3.0

WRITER = textwrap.dedent(
    """
    import os, signal, sys, time

    msgs = sys.argv[1]
    os.makedirs(msgs, exist_ok=True)
    for i in range(int(sys.argv[2])):
        open(os.path.join(msgs, "pre%d.blk" % i), "w").close()

    def flush(signum, frame):
        # What nats-server does on SIGTERM: keep writing for a while, then go.
        end = time.time() + float(sys.argv[3])
        n = 0
        while time.time() < end:
            try:
                os.makedirs(msgs, exist_ok=True)
                open(os.path.join(msgs, "flush%d.blk" % n), "w").close()
            except OSError:
                pass
            n += 1
        sys.exit(0)

    signal.signal(signal.SIGTERM, flush)
    print("ready", flush=True)
    while True:
        time.sleep(0.05)
    """
)


@pytest.fixture
def refused_port() -> Iterator[int]:
    """A loopback port that refuses connections, for as long as the test.

    Bound and never listened on, so nothing else can take it and a connect
    is refused at once -- which `--quiet-when-absent` reads as "no broker,
    nothing to do".
    """
    with socket.socket() as s:
        s.bind((HOST, 0))
        yield s.getsockname()[1]


@pytest.fixture
def shared_queue() -> Iterator[Push | None]:
    """A work queue on the suite's shared broker, or None without one.

    It stands for every other worker's queue in flight while this test runs.
    """
    if not nats_available():
        yield None
        return
    from doppler.stream import CF32, Push

    tag = f"dp-teardown-sentinel-{uuid.uuid4().hex[:10]}"
    push = Push(f"nats://{HOST}:{PORT}/{tag}", CF32)  # provisions it
    try:
        yield push
    finally:
        delete_stream_if_present(push)
        push.close()


def test_nats_down_waits_for_the_writer(
    tmp_path: Path, refused_port: int, shared_queue: Push | None
) -> None:
    """The store is gone, and the recipe reports success, with a writer
    still flushing when the teardown starts."""
    store = tmp_path / "doppler-nats-store"
    # The real layout, so a reader of a failure sees the path CI printed.
    msgs = store / "jetstream" / "$G" / "streams" / "DP_WORK_test" / "msgs"
    pidfile = tmp_path / "doppler-nats.pid"
    bindir = tmp_path / "bin"
    bindir.mkdir()
    docker = bindir / "docker"
    docker.write_text('#!/bin/sh\necho "$@" >> "$0.calls"\n', "utf-8")
    docker.chmod(0o755)

    writer = subprocess.Popen(
        [
            sys.executable,
            "-c",
            WRITER,
            str(msgs),
            str(SEED_FILES),
            str(FLUSH_SECONDS),
        ],
        stdout=subprocess.PIPE,
        text=True,
    )
    try:
        assert writer.stdout is not None
        assert writer.stdout.readline().strip() == "ready"
        pidfile.write_text(str(writer.pid), encoding="utf-8")

        env = {
            **os.environ,
            "TMPDIR": str(tmp_path),
            "NATS_PIDFILE": str(pidfile),
            "NATS_STREAMS_PORT": str(refused_port),
            "PATH": f"{bindir}{os.pathsep}{os.environ['PATH']}",
        }
        proc = subprocess.run(
            ["make", "nats-down"],
            cwd=REPO,
            env=env,
            capture_output=True,
            text=True,
            timeout=120,
        )
    finally:
        if writer.poll() is None:
            writer.kill()
        writer.wait(timeout=30)

    assert proc.returncode == 0, (
        "nats-down must not fail a job that is only cleaning up:\n"
        f"{proc.stdout}\n{proc.stderr}"
    )
    assert not store.exists(), (
        "the store survived nats-down, so the removal raced a writer that "
        "was still flushing — the wait for the broker to exit is missing "
        f"or too short:\n{proc.stdout}\n{proc.stderr}"
    )
    assert not pidfile.exists(), "nats-down must clear the pidfile"

    # Where the purge went. Without a broker on the box this is the only
    # evidence: a purge sent to the shared port finds nothing there either,
    # and exits 0 just the same.
    assert f"no broker on {HOST}:{refused_port}" in proc.stdout, (
        "the recipe's purge was not aimed at the refused port, so it went "
        f"to the broker the suite shares (#2319):\n{proc.stdout}"
    )
    calls = docker.with_name("docker.calls")
    assert calls.exists() and "rm -f nats" in calls.read_text("utf-8"), (
        "the recipe's container removal did not reach the stand-in, so it "
        "ran the real docker against the shared broker's container"
    )
    if shared_queue is not None:
        # Strict on purpose: deleting a stream that is gone raises "Not
        # Found", so this succeeds only if the recipe left the queue alone.
        try:
            shared_queue.delete_stream()
        except RuntimeError as exc:
            raise AssertionError(
                "nats-down deleted a work queue on the shared broker: every "
                "other xdist worker's queue in flight went with it (#2319)"
            ) from exc
