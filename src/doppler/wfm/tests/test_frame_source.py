"""An UNSPREAD frame: the same descriptor, across all three wfmgen
interfaces -- Python, the C CLI, and the scene JSON.

``sync`` / ``acq_code`` / ``acq_reps`` / ``crc`` describe a frame's bit
layout, and until this change they reached the samples only through
``type="dsss"``. On every unspread interface they were accepted, stored, and
readable back — and applied nowhere. ``wfmgen --type bpsk --sync … --crc
crc16`` exited 0 and produced a waveform byte-identical to the unframed run;
``Synth(type="bits", sync=…)`` did the same and handed the sync word back
from ``s.sync`` as if it had been used.

What made that survivable is that no test asserted a frame kwarg CHANGES
anything. The DSSS path was covered; the unspread path had nothing to be
wrong about, because nothing looked. So these tests are behavioural
throughout — never "the flag was accepted", always "the samples moved, and
moved to the descriptor's own bits".

The three interfaces are the point. They converge on one construction path
(``wfm_compose_build_synth``), so a frame honoured on one and dropped on
another is the failure this file exists to catch:

- Python kwargs through ``Segment`` / ``Composer``;
- the C CLI's own flags;
- a ``--record`` JSON round-trip through ``--from-file``.

``Synth``-level assertions live in ``test_wfm_synth.py``, which owns that
type. See ``docs/design/rx-test.md`` §7 for the descriptor.
"""

import json
import subprocess

import numpy as np
import pytest

from doppler.wfm import Composer, FrameDesc, Segment, cli, crc16

SPS = 4
FS = 1e6
SYNC = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)  # Barker13
ACQ = np.array([1, 0] * 4, np.uint8)
REPS = 4
PAYLOAD = np.array([0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0], np.uint8)
NBITS = len(ACQ) * REPS + len(SYNC) + len(PAYLOAD) + 16


def _bits(a) -> str:
    return "".join(str(int(b)) for b in a)


def _seg_kwargs(framed: bool, num_samples: int) -> dict:
    kw = {
        "type": "bits",
        "fs": FS,
        "sps": SPS,
        "bits": PAYLOAD.tobytes(),
        "modulation": "bpsk",
        "num_samples": num_samples,
    }
    if framed:
        kw |= {
            "acq_code": ACQ.tobytes(),
            "acq_reps": REPS,
            "sync": SYNC.tobytes(),
            "crc": "crc16",
        }
    return kw


def _compose(framed: bool, num_samples: int) -> np.ndarray:
    return np.asarray(
        Composer([Segment(**_seg_kwargs(framed, num_samples))]).compose()
    )


def _cli(args, tmp_path, name="out.dat"):
    out = tmp_path / name
    p = subprocess.run(
        [cli._runnable(), *args, "--output", str(out)],
        capture_output=True,
        text=True,
        timeout=120,
    )
    return p, out


def _cli_frame_args(framed: bool, count: int) -> list[str]:
    args = [
        "--type",
        "bits",
        "--modulation",
        "bpsk",
        "--bits",
        _bits(PAYLOAD),
        "--fs",
        str(FS),
        "--sps",
        str(SPS),
        "--count",
        str(count),
    ]
    if framed:
        args += [
            "--acq-code",
            _bits(ACQ),
            "--acq-reps",
            str(REPS),
            "--sync",
            _bits(SYNC),
            "--crc",
            "crc16",
        ]
    return args


# ── the Python interface ─────────────────────────────────────────────────────


def test_a_frame_changes_the_composed_waveform():
    """The assertion whose absence was the defect, at the documented API."""
    n = NBITS * SPS
    assert not np.array_equal(_compose(False, n), _compose(True, n))


def test_the_composed_frame_is_the_descriptors_bits():
    """`[preamble x reps | sync | payload | CRC-16]`, in that order.

    Checked at one sample per symbol so the mapping is direct (BPSK sends
    bit 0 as +1 and bit 1 as -1), and against a CRC computed by the library's
    own ``crc16`` — the same kernel the frame assembler uses, so a wrong
    trailer position or bit order fails here rather than at a receiver.
    """
    kw = _seg_kwargs(True, NBITS) | {"sps": 1, "fs": 1.0}
    y = np.asarray(Composer([Segment(**kw)]).compose()).real

    crc = crc16(PAYLOAD)
    trailer = np.array([(crc >> (15 - i)) & 1 for i in range(16)], np.uint8)
    want = np.concatenate([np.tile(ACQ, REPS), SYNC, PAYLOAD, trailer])

    assert len(want) == NBITS
    np.testing.assert_allclose(y[:NBITS], 1.0 - 2.0 * want, atol=1e-6)


def test_the_frame_cycles_to_fill_the_segment():
    """One descriptor, a multi-frame record — no repeat count in the frame.

    This is what ``native/validation/rx_frame_fer.c`` relies on to score many
    frames from a one-frame description, and it is why a framed unspread
    source cycles rather than deriving its length the way a DSSS burst does.
    """
    kw = _seg_kwargs(True, 3 * NBITS) | {"sps": 1, "fs": 1.0}
    y = np.asarray(Composer([Segment(**kw)]).compose())

    np.testing.assert_allclose(y[:NBITS], y[NBITS : 2 * NBITS], atol=1e-6)
    np.testing.assert_allclose(y[:NBITS], y[2 * NBITS :], atol=1e-6)


@pytest.mark.parametrize("wtype", ["bpsk", "qpsk", "pn"])
def test_a_frame_the_type_cannot_carry_is_refused(wtype):
    """Refused at construction, with nothing built.

    These source their symbols from the PN LFSR, so there is no length to
    bound a payload. The composer used to accept them and drop the frame; a
    build-time failure would have become a silent gap, so the check runs in
    ``wfm_compose_create`` before anything exists.
    """
    kw = _seg_kwargs(True, 256) | {"type": wtype}
    kw.pop("bits")
    kw.pop("modulation")
    with pytest.raises((ValueError, RuntimeError)):
        Composer([Segment(**kw)]).compose()


# ── the C CLI ────────────────────────────────────────────────────────────────


def test_the_cli_frame_flags_reach_the_samples(tmp_path):
    n = NBITS * SPS
    pf, framed = _cli(_cli_frame_args(True, n), tmp_path, "f.dat")
    pu, plain = _cli(_cli_frame_args(False, n), tmp_path, "u.dat")

    assert pf.returncode == 0, pf.stderr
    assert pu.returncode == 0, pu.stderr
    assert framed.read_bytes() != plain.read_bytes()


def test_the_cli_and_the_composer_agree(tmp_path):
    """Same description, same bytes. Two interfaces that disagree are worse
    than one that is wrong, because nothing says which to believe."""
    n = NBITS * SPS
    p, out = _cli(_cli_frame_args(True, n), tmp_path)
    assert p.returncode == 0, p.stderr

    from_cli = np.frombuffer(out.read_bytes(), np.complex64)
    np.testing.assert_allclose(from_cli, _compose(True, n), atol=1e-6)


def test_the_cli_refuses_with_the_reason(tmp_path):
    """Exit 2 and a message naming the replacement — not exit 0 and silence,
    and not the generic 'could not build the waveform spec' either.

    A framed `--type bpsk` is no longer refused for its TYPE: gh-762's
    `--payload-len` gave the PN-sourced waveforms the payload bound they
    were missing, so what is left to refuse is a frame with no payload at
    all — which is the same thing `--type bits` was always refused for.
    """
    p, _ = _cli(
        ["--type", "bpsk", "--sync", _bits(SYNC), "--count", "256"], tmp_path
    )
    assert p.returncode == 2
    assert "payload" in p.stderr
    assert "--payload-len" in p.stderr, (
        "the refusal must name the flag that removes it"
    )

    # And with that bound supplied, the same command BUILDS.
    ok, _ = _cli(
        [
            "--type",
            "bpsk",
            "--sync",
            _bits(SYNC),
            "--payload-len",
            "64",
            "--pn-length",
            "7",
            "--count",
            "256",
        ],
        tmp_path,
    )
    assert ok.returncode == 0, ok.stderr

    # A type with no bit stream at all is still refused, payload or not.
    ch, _ = _cli(
        [
            "--type",
            "chirp",
            "--sync",
            _bits(SYNC),
            "--payload-len",
            "64",
            "--count",
            "256",
        ],
        tmp_path,
    )
    assert ch.returncode == 2
    assert "no bit stream" in ch.stderr

    p2, _ = _cli(
        [
            "--type",
            "bits",
            "--modulation",
            "bpsk",
            "--sync",
            _bits(SYNC),
            "--count",
            "256",
        ],
        tmp_path,
    )
    assert p2.returncode == 2
    assert "payload" in p2.stderr


# ── the record round-trip ────────────────────────────────────────────────────


def test_the_record_carries_the_frame_and_rebuilds_it(tmp_path):
    """A record that omits the frame rebuilds a different waveform.

    ``add_dsss_fields`` was type-gated, so a framed ``bits`` record wrote no
    ``acq_code``/``sync``/``crc`` at all — and ``--from-file`` then produced
    an unframed stream from a file that looked complete. Both halves of
    ``wfm_json.c`` are checked here, because either one alone is silent.
    """
    n = NBITS * SPS
    rec = tmp_path / "rec.json"
    p, out = _cli(
        [*_cli_frame_args(True, n), "--record", str(rec)], tmp_path, "a.dat"
    )
    assert p.returncode == 0, p.stderr

    seg = json.loads(rec.read_text())["segments"][0]
    assert seg["acq_code"] == _bits(ACQ)
    assert seg["acq_reps"] == REPS
    assert seg["sync"] == _bits(SYNC)
    assert seg["crc"] == "crc16"

    p2, out2 = _cli(["--from-file", str(rec)], tmp_path, "b.dat")
    assert p2.returncode == 0, p2.stderr
    assert out2.read_bytes() == out.read_bytes()


def test_an_unframed_record_stays_unframed(tmp_path):
    """`crc` defaults to crc16 on every source, so a record that emitted it
    unconditionally would frame every unframed pattern on the way back in."""
    n = NBITS * SPS
    rec = tmp_path / "rec.json"
    p, out = _cli(
        [*_cli_frame_args(False, n), "--record", str(rec)], tmp_path, "a.dat"
    )
    assert p.returncode == 0, p.stderr

    seg = json.loads(rec.read_text())["segments"][0]
    assert "acq_code" not in seg and "sync" not in seg and "crc" not in seg

    p2, out2 = _cli(["--from-file", str(rec)], tmp_path, "b.dat")
    assert p2.returncode == 0, p2.stderr
    assert out2.read_bytes() == out.read_bytes()


# ── a frame the CALLER built, reaching Python through the scene JSON ─────────


def _carried(with_frame: bool) -> np.ndarray:
    """The same scene, differing only in whether it carries a description."""
    seg = {
        "type": "bits",
        "fs": FS,
        "sps": SPS,
        "modulation": "bpsk",
        "pattern": _bits(PAYLOAD),
        "num_samples": (len(SYNC) + len(PAYLOAD)) * SPS,
    }
    if with_frame:
        seg["frame"] = {
            "fields": [
                {"name": "sync", "lit": _bits(SYNC)},
                {"name": "payload", "lit": _bits(PAYLOAD)},
            ]
        }
    return np.asarray(
        Composer.from_json(json.dumps({"segments": [seg]})).compose()
    )


def test_python_reaches_a_carried_frame_with_no_new_binding():
    """The whole point of the ``frame`` key, demonstrated end to end.

    ``Segment`` is jm-generated and jm has no field type that accepts another
    extension object, so ``Segment(frame=FrameDesc(...))`` cannot be declared.
    It does not need to be: ``Composer.from_json`` is the same C code
    ``wfmgen --from-file`` runs, so a description crosses into Python through
    the scene JSON and needs no binding of its own.

    Behavioural, per this file's own rule — not "the key was accepted" but
    "the samples moved". A carried frame puts the sync word on the wire ahead
    of the payload, which an unframed scene does not.
    """
    assert not np.array_equal(_carried(False), _carried(True))


def test_a_carried_frame_survives_the_python_round_trip():
    """It goes back OUT again, so a scene read and re-written keeps its frame.

    The reader and the writer are separate functions in ``wfm_json.c``; a
    description that parsed but did not re-emit would make ``from_json`` ->
    ``to_json`` quietly drop the frame, and the next ``--from-file`` of that
    output would rebuild the derived one.
    """
    seg = {
        "type": "bits",
        "fs": FS,
        "sps": SPS,
        "modulation": "bpsk",
        "pattern": _bits(PAYLOAD),
        "num_samples": (len(SYNC) + len(PAYLOAD)) * SPS,
        "frame": {"fields": [{"name": "sync", "lit": _bits(SYNC)}]},
    }
    once = Composer.from_json(json.dumps({"segments": [seg]})).to_json()
    got = json.loads(once)["segments"][0]["frame"]
    assert got["fields"][0]["name"] == "sync"
    assert got["fields"][0]["lit"] == _bits(SYNC)
    # And again, so the second pass is a fixed point rather than a decay.
    twice = Composer.from_json(once).to_json()
    assert json.loads(twice)["segments"][0]["frame"] == got


# ── waiting on just-makeit#1224 ─────────────────────────────────────────────


@pytest.mark.xfail(
    strict=True,
    reason=(
        "just-makeit#1224: an init_param/field cannot take another generated "
        "object, so `frame=` cannot be declared on Segment. Strict on "
        "purpose -- when jm ships it AND objects/… declares the field, this "
        "XPASSes and fails CI until the marker is removed, which is how the "
        "adoption announces itself instead of waiting to be remembered."
    ),
)
def test_segment_takes_a_framedesc_directly():
    """The ergonomic form, and the exact bar its adoption has to clear.

    `FrameDesc` is already generated and `Composer.from_json` already carries
    a description into C, so nothing here is a new capability -- this is
    sugar over the path `test_python_reaches_a_carried_frame_with_no_new_
    binding` pins. That is why the assertion is not "it does not raise" but
    "it renders the SAME bytes as the JSON route": sugar that produces a
    different waveform from the interchange it wraps is worse than no sugar.

    Do not delete the JSON tests when this passes. The scene JSON is the
    interchange -- it is what `wfmgen --from-file` eats and what a recorded
    capture replays through -- and this only spares a caller from spelling
    it. Deleting them would retire the thing being wrapped.
    """
    desc = FrameDesc(np.zeros(0, np.uint8), SYNC, PAYLOAD, crc="none")
    seg = Segment(
        type="bits",
        fs=FS,
        sps=SPS,
        modulation="bpsk",
        bits=PAYLOAD.tobytes(),
        num_samples=(len(SYNC) + len(PAYLOAD)) * SPS,
        frame=desc,
    )
    direct = np.asarray(Composer([seg]).compose())
    assert np.array_equal(direct, _carried(True))
