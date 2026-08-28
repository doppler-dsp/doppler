"""A ``--record`` capture must replay the run it recorded, byte for byte.

That is the contract `wfm_compose.h` states over the JSON spec — *"a recorded
run reproduces byte-for-byte when fed back via ``--from-file``"* — and what
`write_record()` promises: *"the fully-resolved run, as the JSON that
``--from-file`` reads back"*.

It was false for ``seed_advance`` (doppler#978). The key was parsed onto the
composer and `wfm_spec_to_json()` had **no parameter to write it from**, so
a recorded run replayed with the mode silently reset to ``none`` — every loop
after the first came out identical when the original's did not. Nothing warned,
on either stream.

Two things make that defect easy to miss, and both shape the tests below:

- **It is only observable on the repeat/continuous LOOP axis.** wfmgen has two
  repeat counters: ``epoch`` (the whole spec looping) is what ``seed_advance``
  drives, while a segment's own ``repeats: N`` is ``instance``, which reseeds
  the AWGN *unconditionally* by design. A ``repeats``-based probe shows fresh
  noise under every ``seed_advance`` value and proves nothing.
- **A loop has no natural end**, so the byte comparison below reads a fixed
  prefix off the pipe rather than waiting for a file.

The key is emitted only when non-default, the way ``headroom`` already is, so
an ordinary run's record is unchanged — asserted here too, because "the fix
churned every existing capture" is the other way this goes wrong.
"""

from __future__ import annotations

import json
import subprocess
from typing import TYPE_CHECKING

import numpy as np
import pytest

from doppler.wfm import Composer, cli

if TYPE_CHECKING:
    from pathlib import Path

#: One PN period at sps=1, in samples — the loop length of the scene below.
PERIOD = 127

#: cf32 on the wire.
BYTES_PER_SAMPLE = 8

#: How many loops of the scene to compare. Two is the minimum that can see a
#: per-epoch seed at all: epoch 0 is unchanged under every mode.
LOOPS = 3

#: A looping scene whose only variable is the seed-advance mode.
SCENE = {
    "version": 1,
    "repeat": True,
    "segments": [
        {
            "type": "pn",
            "fs": 1000000,
            "sps": 1,
            "pn_length": 7,
            "snr": 10.0,
            "seed": 1,
            "num_samples": PERIOD,
        }
    ],
}


def _bin() -> str:
    return cli._runnable()


def _prefix(*args: str) -> bytes:
    """Run wfmgen to stdout and take the first ``LOOPS`` loops, then stop.

    A ``repeat`` run never ends on its own, so the read is what bounds it.
    """
    want = LOOPS * PERIOD * BYTES_PER_SAMPLE
    p = subprocess.Popen(
        [_bin(), *args, "--output", "-"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        buf = b""
        while len(buf) < want:
            chunk = p.stdout.read(want - len(buf))
            if not chunk:  # the process ended early — let the caller see it
                break
            buf += chunk
        return buf
    finally:
        p.terminate()
        try:
            p.wait(timeout=20)
        except subprocess.TimeoutExpired:  # pragma: no cover - safety net
            p.kill()
            p.wait(timeout=20)


def _samples(raw: bytes) -> np.ndarray:
    return np.frombuffer(raw, dtype="<c8")


def _scene_file(tmp_path: Path, mode: str | None) -> Path:
    scene = dict(SCENE)
    if mode is not None:
        scene["seed_advance"] = mode
    path = tmp_path / "scene.json"
    path.write_text(json.dumps(scene))
    return path


@pytest.mark.parametrize("mode", ["noise", "all"])
def test_record_replays_the_run_byte_for_byte(tmp_path, mode):
    """The headline contract, at the outcome layer: the samples, not the key.

    Before the fix this failed at the first sample of loop 1 — the exact point
    where the mode first acts — with the replay's loops all identical.
    """
    scene = _scene_file(tmp_path, mode)
    record = tmp_path / "record.json"
    run = _prefix("--from-file", str(scene), "--record", str(record))
    replay = _prefix("--from-file", str(record))

    assert len(run) == LOOPS * PERIOD * BYTES_PER_SAMPLE
    assert replay == run, (
        f"a --record capture of seed_advance={mode!r} did not replay the run"
    )


@pytest.mark.parametrize("mode", ["noise", "all"])
def test_the_recorded_run_actually_varied(tmp_path, mode):
    """Guard the guard: a replay that matches a run of identical loops proves
    nothing. This is the check that makes the test above non-vacuous."""
    run = _samples(
        _prefix("--from-file", str(_scene_file(tmp_path, mode)))
    ).reshape(LOOPS, PERIOD)
    assert not np.array_equal(run[0], run[1]), (
        f"seed_advance={mode!r} produced byte-identical loops, so the "
        "round-trip test above cannot fail"
    )


def test_none_replays_and_its_loops_are_identical(tmp_path):
    """The default's own two claims, which the fix must not disturb."""
    scene = _scene_file(tmp_path, None)
    record = tmp_path / "record.json"
    run = _prefix("--from-file", str(scene), "--record", str(record))
    assert _prefix("--from-file", str(record)) == run
    loops = _samples(run).reshape(LOOPS, PERIOD)
    assert np.array_equal(loops[0], loops[1])


@pytest.mark.parametrize("mode", ["noise", "all"])
def test_the_flag_path_records_it_too(tmp_path, mode):
    """``--seed-advance`` is a flag as well as a scene key, and the flag path
    builds its composer somewhere else entirely — so it gets its own case."""
    record = tmp_path / "record.json"
    subprocess.run(
        [
            _bin(),
            "--type",
            "pn",
            "--fs",
            "1e6",
            "--sps",
            "1",
            "--pn-length",
            "7",
            "--snr",
            "10",
            "--count",
            str(PERIOD),
            "--seed",
            "1",
            "--seed-advance",
            mode,
            "--output",
            "/dev/null",
            "--record",
            str(record),
        ],
        capture_output=True,
        check=True,
    )
    assert json.loads(record.read_text())["seed_advance"] == mode


def test_a_default_run_records_no_seed_advance_key(tmp_path):
    """Emitted only when non-default, exactly as ``headroom`` is.

    The 1-source inline form's field order is frozen for byte-identity, so a
    key that appeared unconditionally would churn every capture ever recorded
    to say `none`.
    """
    record = tmp_path / "record.json"
    subprocess.run(
        [
            _bin(),
            "--type",
            "pn",
            "--fs",
            "1e6",
            "--count",
            str(PERIOD),
            "--output",
            "/dev/null",
            "--record",
            str(record),
        ],
        capture_output=True,
        check=True,
    )
    assert "seed_advance" not in json.loads(record.read_text())


@pytest.mark.parametrize("mode", ["noise", "all"])
def test_python_to_json_round_trips_it(tmp_path, mode):
    """The same contract on the Python face, whose module docstring makes it:
    *"the composer's resolved spec round-trips through JSON … so a capture is
    fully reproducible"*.

    ``Composer`` reaches the same serializer through jm's generated
    ``to_json``, so this is a second call site of one emitter — not a second
    emitter — and it is the face a caller reading that docstring uses.
    """
    scene = json.dumps({**SCENE, "seed_advance": mode})
    n = LOOPS * PERIOD
    direct = Composer.from_json(scene).execute(n)
    replay = Composer.from_json(Composer.from_json(scene).to_json()).execute(n)
    assert np.array_equal(direct, replay)
    # non-vacuous for the same reason as above
    assert not np.array_equal(direct[:PERIOD], direct[PERIOD : 2 * PERIOD])


# ── the same defect, on a second flag ────────────────────────────────────
#
# `--interleave` was dropped by the record on the flag's first release
# (doppler#1031), exactly as `seed_advance` had been. The replay came back
# the same LENGTH with different bytes and no error — a capture that looks
# like the one you recorded and is a different waveform.
#
# It is a finite run, so unlike the loop tests above it can be compared as
# whole files rather than a prefix off a pipe.


def _run_to(tmp_path: Path, name: str, *args: str) -> bytes:
    out = tmp_path / name
    subprocess.run(
        [_bin(), *args, "--output", str(out)],
        check=True,
        capture_output=True,
        timeout=60,
    )
    return out.read_bytes()


@pytest.mark.parametrize(
    ("depth", "unit", "payload"),
    [
        (4, 1, "1011001011010010"),  # bit interleaving, the bare form
        (5, 8, None),  # octet units: 24 payload bits + 16 CRC = 5 x 8
    ],
)
def test_interleave_survives_a_record_round_trip(
    tmp_path, depth, unit, payload
):
    """The flag has to reach the writer AND the reader, not just the kernel."""
    bits = ["--bits", payload] if payload else ["--bits-hex", "b25a0f"]
    record = tmp_path / "record.json"
    args = [
        "--type",
        "bits",
        "--modulation",
        "bpsk",
        *bits,
        "--sync",
        "11110011",
        "--sps",
        "1",
        "--count",
        "4096",
        "--interleave",
        str(depth),
        "--interleave-unit",
        str(unit),
    ]
    first = _run_to(tmp_path, "a.iq", *args, "--record", str(record))
    again = _run_to(tmp_path, "b.iq", "--from-file", str(record))

    spec = json.loads(record.read_text())["segments"][0]
    assert spec["interleave"] == depth
    assert spec["interleave_unit"] == unit
    assert again == first, (
        f"--interleave {depth} --interleave-unit {unit} did not survive "
        f"--record; the replay is {len(again)} bytes against {len(first)}"
    )


def test_an_interleaved_run_differs_from_an_uninterleaved_one(tmp_path):
    """Guard the guard: if the flag changed nothing, the test above is
    vacuous and would pass against a writer that dropped it."""
    args = [
        "--type",
        "bits",
        "--modulation",
        "bpsk",
        "--bits",
        "1011001011010010",
        "--sync",
        "11110011",
        "--sps",
        "1",
        "--count",
        "4096",
    ]
    plain = _run_to(tmp_path, "plain.iq", *args)
    woven = _run_to(tmp_path, "woven.iq", *args, "--interleave", "4")
    assert len(plain) == len(woven), "the interleaver is length-preserving"
    assert plain != woven, "--interleave changed nothing to record"
