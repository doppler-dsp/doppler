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
"""

from __future__ import annotations

import os
import subprocess
import sys
import textwrap
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

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


def test_nats_down_waits_for_the_writer(tmp_path: Path) -> None:
    """The store is gone, and the recipe reports success, with a writer
    still flushing when the teardown starts."""
    store = tmp_path / "doppler-nats-store"
    # The real layout, so a reader of a failure sees the path CI printed.
    msgs = store / "jetstream" / "$G" / "streams" / "DP_WORK_test" / "msgs"
    pidfile = tmp_path / "doppler-nats.pid"

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
