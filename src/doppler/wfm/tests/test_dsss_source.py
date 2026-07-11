"""The ``type="dsss"`` source: two-code bursts as a first-class waveform.

One dsss segment is a complete burst — an unmodulated repeated preamble
(``acq_code`` x ``acq_reps``) followed by the frame ``sync | payload |
CRC-16``, every frame bit spread by the distinct ``data_code`` — with
``snr_mode="esno"`` meaning the Es/N0 of the outer *data* symbol
(``len(data_code) * sps`` samples). These tests pin the three properties
that make it trustworthy:

- the burst renders byte-identically through all three wfmgen faces
  (Python kwargs, JSON ``from_json``, the C CLI via ``--from-file`` and
  bare ``--type dsss`` flags);
- the Es/N0 calibration is real: measured noise power matches the
  ``esno - 10*log10(sf*sps)`` conversion, and a data-aided estimate over
  despread symbols recovers the target;
- the TX frame honours the RX contract: ``BurstDemod`` seeded with the
  same codes decodes every burst of a 5-burst capture, CRC-valid, payload
  bit-exact.
"""

from __future__ import annotations

import json
import math
import subprocess

import numpy as np
import pytest

from doppler.dsss import BurstDemod
from doppler.snr import snr_data_aided_db
from doppler.wfm import Composer, Segment, Synth, cli

ACQ_SF, REPS, DATA_SF, SPC = 128, 4, 25, 4
FS = 1e6 * SPC
PAYLOAD = 200
SYNC = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)  # Barker13
FRAME = len(SYNC) + PAYLOAD + 16  # sync + payload + CRC-16
BURST_CHIPS = ACQ_SF * REPS + FRAME * DATA_SF
BURST_LEN = BURST_CHIPS * SPC


def _codes():
    rng = np.random.default_rng(7)
    acq = rng.integers(0, 2, ACQ_SF, dtype=np.uint8)
    dat = rng.integers(0, 2, DATA_SF, dtype=np.uint8)
    pay = rng.integers(0, 2, PAYLOAD, dtype=np.uint8)
    return acq, dat, pay


def _seg_kwargs(seed: int, off: int, acq, dat, pay) -> dict:
    return {
        "type": "dsss",
        "fs": FS,
        "sps": SPC,
        "seed": seed,
        "snr": 10.0,
        "snr_mode": "esno",
        "acq_code": acq.tobytes(),
        "acq_reps": REPS,
        "data_code": dat.tobytes(),
        "sync": SYNC.tobytes(),
        "payload": pay.tobytes(),
        "off_samples": off,
    }


def _scene_json(kwargs_list) -> dict:
    segments = []
    for kw in kwargs_list:
        d = dict(kw)
        for key in ("acq_code", "data_code", "sync", "payload"):
            d[key] = "".join(str(b) for b in d[key])
        segments.append(d)
    return {
        "version": 1,
        "repeat": False,
        "continuous": False,
        "segments": segments,
    }


def test_intrinsic_on_time():
    """A dsss segment's on-time is one burst — num_samples is derived (and
    any caller-supplied value ignored), so to_json records the real span."""
    acq, dat, pay = _codes()
    seg = Segment(**_seg_kwargs(1, 500, acq, dat, pay), num_samples=17)
    comp = Composer([seg])
    x = comp.compose()
    assert len(x) == BURST_LEN + 500
    spec = json.loads(comp.to_json())
    assert spec["segments"][0]["num_samples"] == BURST_LEN


def test_three_faces_byte_identical(tmp_path):
    """kwargs Composer == from_json == CLI --from-file, bit for bit."""
    acq, dat, pay = _codes()
    kwargs = [
        _seg_kwargs(k + 1, 3000 + 1000 * k, acq, dat, pay) for k in range(2)
    ]

    x_obj = Composer([Segment(**kw) for kw in kwargs]).compose()

    scene = json.dumps(_scene_json(kwargs))
    x_json = Composer.from_json(scene).compose()
    assert np.array_equal(x_obj, x_json)

    scene_path = tmp_path / "scene.json"
    scene_path.write_text(scene)
    out = tmp_path / "cli.cf32"
    p = subprocess.run(
        [
            cli._runnable(),
            "--from-file",
            str(scene_path),
            "--output",
            str(out),
        ],
        capture_output=True,
    )
    assert p.returncode == 0, p.stderr.decode()
    x_cli = np.fromfile(out, np.complex64)
    assert np.array_equal(x_obj, x_cli)


def test_cli_bare_flags_match_kwargs(tmp_path):
    """A single burst from bare --type dsss flags == the kwargs face."""
    acq, dat, pay = _codes()
    kw = _seg_kwargs(3, 0, acq, dat, pay)
    x_obj = Composer([Segment(**kw)]).compose()

    out = tmp_path / "cli.cf32"
    p = subprocess.run(
        [
            cli._runnable(),
            "--type",
            "dsss",
            "--fs",
            str(FS),
            "--sps",
            str(SPC),
            "--seed",
            "3",
            "--snr",
            "10",
            "--snr-mode",
            "esno",
            "--acq-code",
            "".join(map(str, acq)),
            "--acq-reps",
            str(REPS),
            "--data-code",
            "".join(map(str, dat)),
            "--sync",
            "".join(map(str, SYNC)),
            "--bits",
            "".join(map(str, pay)),
            "--output",
            str(out),
        ],
        capture_output=True,
    )
    assert p.returncode == 0, p.stderr.decode()
    x_cli = np.fromfile(out, np.complex64)
    assert np.array_equal(x_obj, x_cli)


def test_esno_calibration():
    """snr_mode="esno" means the DATA symbol: noise power over fs matches
    esno - 10*log10(sf*sps), and a data-aided estimate over despread
    symbols recovers the target Es/N0."""
    acq, dat, pay = _codes()
    kw = _seg_kwargs(11, 0, acq, dat, pay)
    kw["crc"] = "none"
    kw.pop("sync")
    noisy = Composer([Segment(**kw)]).compose()
    clean = Composer([Segment(**{**kw, "snr": 100.0})]).compose()
    noise_power = float(np.mean(np.abs(noisy - clean) ** 2))
    expected = 10 ** (-(10.0 - 10 * math.log10(DATA_SF * SPC)) / 10)
    assert noise_power == pytest.approx(expected, rel=0.05)

    pre = ACQ_SF * REPS * SPC
    chips = noisy[pre : pre + PAYLOAD * DATA_SF * SPC]
    chips = chips.reshape(PAYLOAD, DATA_SF, SPC)
    signs = np.where(dat[None, :, None] == 1, -1.0, 1.0)
    soft = (chips * signs).mean(axis=(1, 2)) * math.sqrt(DATA_SF * SPC)
    est = snr_data_aided_db(soft.astype(np.complex64), pay)
    assert est == pytest.approx(10.0, abs=0.75)


def test_five_bursts_decode_through_burst_demod():
    """The canonical scenario: 5 bursts, engine-drawn random gaps with a
    minimum, each decoded CRC-valid and payload-exact by BurstDemod from
    the ground-truth starts."""
    acq, dat, pay = _codes()
    min_gap, max_gap = 4000, 12000
    segs = [
        Segment(
            **{
                **_seg_kwargs(k + 1, 0, acq, dat, pay),
                "off_samples": (min_gap, max_gap),
            }
        )
        for k in range(5)
    ]
    comp = Composer(segs)
    x = comp.compose()

    # recover each burst's drawn gap from the resolved spec is not possible
    # (ranges record the span), so walk the capture: bursts are BURST_LEN
    # long; gaps are the zero runs between them.
    starts, pos = [], 0
    for _ in range(5):
        starts.append(pos)
        pos += BURST_LEN
        # skip the trailing zero gap (>= min_gap by construction)
        nz = np.flatnonzero(x[pos:] != 0)
        pos += int(nz[0]) if len(nz) else len(x) - pos
    assert len(x) >= 5 * (BURST_LEN + min_gap)

    n_valid = 0
    for _k, s in enumerate(starts):
        bd = BurstDemod(dat, spc=SPC, chip_rate=FS / SPC, payload_len=PAYLOAD)
        bd.set_preamble(acq, REPS)
        bd.set_sync(SYNC)
        bd.set_prior(0.0, 0)
        bits = bd.demod(x[s : s + BURST_LEN])
        if bd.frame_valid and np.array_equal(bits, pay):
            n_valid += 1
    assert n_valid == 5


def test_standalone_synth_face():
    """The standalone Synth (bridge face) renders the same burst as a
    one-segment Composer with no gap."""
    acq, dat, pay = _codes()
    kw = _seg_kwargs(2, 0, acq, dat, pay)
    kw.pop("off_samples")
    x_comp = Composer([Segment(**kw)]).compose()
    fs = kw.pop("fs")
    s = Synth(**kw, fs=fs)
    x_syn = s.steps(BURST_LEN)
    assert np.array_equal(x_comp, x_syn)


def test_invalid_geometry_raises_or_degrades():
    """Payload with no data code is invalid: the standalone Synth raises at
    first generation."""
    acq, dat, pay = _codes()
    kw = _seg_kwargs(1, 0, acq, dat, pay)
    kw.pop("data_code")
    kw.pop("off_samples")
    s = Synth(**kw)
    with pytest.raises(RuntimeError):
        s.steps(64)
