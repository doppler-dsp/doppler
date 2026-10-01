"""wfmgen paces and labels a scene whose segments differ in fs by segment.

doppler#1733. ``fs`` is per segment, and a scene whose segments differ is
legal (no single rate is true of it). wfmgen used to take segment 0's rate
for the whole run: ``--realtime`` played a slow segment after a fast one
several times too fast, and every stream frame claimed segment 0's rate.
Each segment now runs for its own duration, and each frame states its own.

The C harnesses pin the parts that need neither a wall clock nor a broker:
the composer's one-rate-a-block read (test_wfm_compose.c), the clock's rate
change (test_timing_core.c), and the per-output refusals
(wfmgen_cli_test.cmake). The two claims here are a duration and a frame
header, so they are measured where those exist.
"""

from __future__ import annotations

import json
import subprocess
import tempfile
import time
from typing import TYPE_CHECKING

import pytest

from doppler.wfm import cli

if TYPE_CHECKING:
    from pathlib import Path


def test_realtime_paces_each_segment_at_its_own_rate(tmp_path: Path) -> None:
    """60000 samples at 600 kS/s (0.1 s), then 40000 at 100 kS/s (0.4 s):
    0.5 s. Paced at segment 0's rate the whole run takes 100000 / 6e5 =
    0.17 s, which the lower bound rules out; the upper bound is loose for a
    loaded runner."""
    scene = {
        "segments": [
            {
                "fs": 600000.0,
                "num_samples": 60000,
                "sum": [{"type": "tone", "freq": 1000.0}],
            },
            {
                "fs": 100000.0,
                "num_samples": 40000,
                "sum": [{"type": "tone", "freq": 1000.0}],
            },
        ]
    }
    spec = tmp_path / "mixed.json"
    spec.write_text(json.dumps(scene), encoding="utf-8")
    out = tmp_path / "mixed.cf32"
    t0 = time.monotonic()
    r = subprocess.run(
        [
            cli._runnable(),
            "--from-file",
            str(spec),
            "--realtime",
            "-o",
            str(out),
        ],
        capture_output=True,
        timeout=20,
    )
    took = time.monotonic() - t0
    assert r.returncode == 0, r.stderr.decode()
    assert out.stat().st_size == 100000 * 8  # cf32: every sample written
    assert 0.45 < took < 2.0, f"{took:.3f} s, expected ~0.5 s"


def _broker_reachable() -> bool:
    import socket

    try:
        socket.create_connection(("127.0.0.1", 4222), timeout=0.3).close()
        return True
    except OSError:
        return False


@pytest.mark.skipif(
    not _broker_reachable(), reason="no NATS broker on 127.0.0.1:4222"
)
def test_a_stream_frame_states_its_own_segments_rate() -> None:
    """Every frame header carries an fs, so the stream sink can describe a
    mixed scene honestly: no frame spans two rates, and each states the
    rate of the segment its samples came from -- 600 samples at 6 MS/s,
    then 400 at 2 MS/s."""
    import random

    from doppler.stream import Subscriber

    ep = f"nats://127.0.0.1:4222/mixed{random.randint(1, 10**9)}"
    scene = json.dumps(
        {
            "segments": [
                {
                    "fs": 6e6,
                    "num_samples": 600,
                    "sum": [{"type": "tone", "freq": 1000.0}],
                },
                {
                    "fs": 2e6,
                    "num_samples": 400,
                    "sum": [{"type": "tone", "freq": 1000.0}],
                },
            ]
        }
    )
    sub = Subscriber(ep)
    time.sleep(0.2)  # a lone subscriber: let the subscription register
    with tempfile.NamedTemporaryFile("w", suffix=".json") as f:
        f.write(scene)
        f.flush()
        r = subprocess.run(
            [cli._runnable(), "--from-file", f.name, "--output", ep],
            capture_output=True,
            timeout=20,
        )
    assert r.returncode == 0, r.stderr.decode()
    per_rate: dict[float, int] = {}
    order: list[float] = []
    got = 0
    while got < 1000:
        samples, hdr = sub.recv(timeout_ms=2000)
        if len(samples) == 0:
            break  # end of stream
        fs = float(hdr["sample_rate"])
        if not order or order[-1] != fs:
            order.append(fs)
        per_rate[fs] = per_rate.get(fs, 0) + len(samples)
        got += len(samples)
    sub.close()
    assert order == [6e6, 2e6]
    assert per_rate == {6e6: 600, 2e6: 400}
