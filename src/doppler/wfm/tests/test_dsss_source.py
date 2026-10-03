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
from doppler.wfm import (
    STAGE_CRC16,
    Composer,
    FrameDesc,
    Segment,
    Synth,
    cli,
)

ACQ_SF, REPS, DATA_SF, SPC = 128, 4, 25, 4
FS = 1e6 * SPC
PAYLOAD = 200
SYNC = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)  # Barker13
FRAME = len(SYNC) + PAYLOAD + 16  # sync + payload + CRC-16
BURST_CHIPS = ACQ_SF * REPS + FRAME * DATA_SF
BURST_LEN = BURST_CHIPS * SPC


def _frame_ok(frame, payload):
    """Did this frame arrive intact? The receiver no longer says.

    `BurstDemod` stops at decisions (doppler#1022): it hands back the
    frame's bits, and the trailer is checked by whoever holds the frame —
    `wfm.Frame.deframe()` in the shipped form, four lines here.
    """
    from doppler.wfm import crc16

    frame = np.asarray(frame)
    if frame.size < len(SYNC) + len(payload) + 16:
        return False
    got = frame[len(SYNC) : len(SYNC) + len(payload)]
    rx = 0
    for b in frame[len(SYNC) + len(payload) :][:16]:
        rx = (rx << 1) | (int(b) & 1)
    return bool(np.array_equal(got, payload)) and rx == int(crc16(got))


def _codes():
    rng = np.random.default_rng(7)
    acq = rng.integers(0, 2, ACQ_SF, dtype=np.uint8)
    dat = rng.integers(0, 2, DATA_SF, dtype=np.uint8)
    pay = rng.integers(0, 2, PAYLOAD, dtype=np.uint8)
    return acq, dat, pay


def _rx_frame(pay):
    """The description the receiver is handed: ``[sync | payload | CRC-16]``.

    The receiver reads its sync word (field 0) and the frame's length from
    it; the payload bits only fill the layout, so the transmitter's own are
    reused and both ends hold one description.
    """
    from doppler.wfm import Frame

    frame = Frame(sync=SYNC, payload=pay, crc="crc16")
    assert frame.nbits == FRAME
    return frame


def _data_desc() -> FrameDesc:
    """``[sync | data:PAYLOAD | CRC-16]``: the frame a burst spreads. The
    preamble stays on the source -- it is sent unspread, outside this."""
    d = FrameDesc()
    d.add_field("sync", SYNC)
    d.add_data("payload", PAYLOAD)
    d.add_derived("crc", 16)
    d.add_stage_over(STAGE_CRC16, "payload", "crc")
    return d


#: The same description as a scene's "frame" key.
_FRAME_JSON = {
    "fields": [
        {"name": "sync", "spec": "".join(map(str, SYNC))},
        {"name": "payload", "spec": f"data:{PAYLOAD}"},
        {"name": "crc", "bits": 16, "derived_by": 1},
    ],
    "stages": [{"kind": "crc16", "first_field": 1, "n_fields": 2}],
}


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
        "frame": _data_desc(),  # sync | data | CRC-16, over `data` below
        "data": pay.tobytes(),  # one burst: the data source, whole
        "off_samples": off,
    }


def _scene_json(kwargs_list) -> dict:
    segments = []
    for kw in kwargs_list:
        d = dict(kw)
        for key in ("acq_code", "data_code", "data"):
            d[key] = "".join(str(b) for b in d[key])
        d["frame"] = _FRAME_JSON
        # A scene carries the preamble's repetitions in its Field, *REPS.
        d["acq_code"] += f"*{d.pop('acq_reps')}"
        segments.append(d)
    return {
        "version": 1,
        "repeat": False,
        "continuous": False,
        "segments": segments,
    }


def test_intrinsic_on_time():
    """A dsss segment's on-time is its bursts, one per frame of its data --
    here one -- so num_samples is derived, and a count given beside it is
    refused rather than dropped (doppler#1729). The record leaves it out: a
    replay derives it again from the data."""
    acq, dat, pay = _codes()
    with pytest.raises(ValueError, match="num_samples is derived"):
        Composer(
            [Segment(**_seg_kwargs(1, 500, acq, dat, pay), num_samples=17)]
        )
    seg = Segment(**_seg_kwargs(1, 500, acq, dat, pay))
    assert seg.num_samples == 0  # the default: derive it
    comp = Composer([seg])
    x = comp.compose()
    assert len(x) == BURST_LEN + 500
    spec = json.loads(comp.to_json())
    assert "num_samples" not in spec["segments"][0]
    assert len(Composer.from_json(comp.to_json()).compose()) == len(x)


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
            "".join(map(str, acq)) + f"*{REPS}",
            "--data-code",
            "".join(map(str, dat)),
            "--sync",
            "".join(map(str, SYNC)),
            "--crc",
            "crc16",  # the CLI's flags build the same description
            "--data",
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
    del kw["frame"]  # the payload alone: no sync, no CRC
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
                "gap_noise": "off",  # zero-run walk needs silent gaps
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
        bd = BurstDemod(dat, _rx_frame(pay), spc=SPC, chip_rate=FS / SPC)
        bd.set_preamble(acq, REPS)
        bd.set_prior(0.0, 0)
        bits = bd.demod(x[s : s + BURST_LEN])
        if _frame_ok(bits, pay):
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


# ── the frame's STAGES reach a SPREAD burst ─────────────────────────────
#
# `test_frame_source.py` is the unspread twin of this section, written
# because the frame kwargs were "accepted, stored, and applied nowhere" on
# every unspread face. The spread path had the same hole one layer in: a
# dsss burst assembled its frame through a private four-field builder that
# had never heard of a stage, so `--conv` on a DSSS source produced a
# BYTE-IDENTICAL waveform to no `--conv` at all. Nothing failed, because
# nothing compared the two (doppler#1017).
#
# So every assertion here is behavioural: the samples moved, they moved by
# the amount the description implies, and the preamble did not move at all.


def _burst(acq, dat, pay, stage=None):
    """A clean burst over `[sync | payload | CRC-16]`, coded by `stage`.

    A coded frame is a DESCRIPTION: `FrameDesc` states the fields and the
    span each stage covers, and the source spreads its bits as an otherwise
    unframed payload -- the preamble stays on the source, unspread. With no
    stage the same description is the common frame, so the two bursts
    differ by the stage and nothing else.
    """
    from doppler.wfm import (
        STAGE_CONV,
        STAGE_CRC16,
        STAGE_RANDOMISE,
        FrameDesc,
        field_bits,
    )

    d = FrameDesc()
    if stage == "asm":
        d.add_field("asm", np.asarray(field_bits("0x1ACFFC1D")))
    d.add_field("sync", SYNC)
    d.add_field("payload", pay)
    d.add_derived("crc", 16)
    d.add_stage_over(STAGE_CRC16, "payload", "crc", 0, 0)
    if stage == "randomise":
        d.add_stage_over(STAGE_RANDOMISE, "payload", "crc", 1, 0)
    if stage == "conv":
        d.add_stage(
            STAGE_CONV, first_field=0, n_fields=3, emit_num=2, emit_den=1
        )
    d.build()

    kw = _seg_kwargs(1, 0, acq, dat, pay)
    kw["snr"] = 99.0  # the stage is the only thing that may move a sample
    kw["data"] = np.asarray(d.bits()).tobytes()
    del kw["frame"]  # the stages' own description rides in `data`
    return np.asarray(Composer([Segment(**kw)]).compose())


#: (stage, extra frame BITS it adds). The inner code doubles the frame it
#: covers; the marker adds its 32 bits; the randomiser is XOR in place.
STAGE_BITS = [("conv", FRAME), ("asm", 32), ("randomise", 0)]


@pytest.mark.parametrize(("stage", "extra_bits"), STAGE_BITS)
def test_stage_reaches_the_spread_frame(stage, extra_bits):
    """It runs, it costs what the description says, and it spares the
    preamble."""
    acq, dat, pay = _codes()
    plain = _burst(acq, dat, pay)
    coded = _burst(acq, dat, pay, stage)

    pre = ACQ_SF * REPS * SPC  # unmodulated preamble, in SAMPLES
    assert coded.size == plain.size + extra_bits * DATA_SF * SPC, (
        f"{stage} did not lengthen the burst by its own bits, spread"
    )
    # The preamble is the coherent pull-in target: it is transmitted
    # unmodulated and UNSPREAD, so it is outside every stage's cover. A
    # stage that touched it would break acquisition for every receiver.
    assert np.array_equal(coded[:pre], plain[:pre]), (
        f"{stage} moved the preamble, which is not part of the frame"
    )
    # ...and it must have changed what it does cover, or the stage was
    # declared and dropped -- the exact failure this section exists to catch.
    n = min(coded.size, plain.size) - pre
    assert not np.array_equal(coded[pre : pre + n], plain[pre : pre + n]), (
        f"{stage} left the spread frame byte-identical: the stage did not run"
    )


def test_a_record_carries_the_stages_and_replays_them():
    """A coded burst's own record rebuilds it, stage for stage.

    This is what a record is FOR. A coded DSSS capture once recorded its
    codes, its preamble and its CRC, dropped its coding stages, and replayed
    as a perfectly plausible UNCODED waveform. A coded frame is now a
    description -- a scene's "frame" key, which is also how Python composes
    one -- and the record carries it whole: every stage, and no
    common-frame `crc` key beside it to say something else.
    """
    acq, dat, pay = _codes()
    frame = {
        "fields": [
            {"name": "asm", "spec": "0x1ACFFC1D"},
            {"name": "sync", "spec": "".join(map(str, SYNC))},
            {"name": "payload", "spec": "".join(map(str, pay))},
            {"name": "crc", "bits": 16, "derived_by": 1},
        ],
        "stages": [
            {"kind": "crc16", "first_field": 2, "n_fields": 2},
            {
                "kind": "conv",
                "first_field": 0,
                "n_fields": 4,
                "emit_num": 2,
                "emit_den": 1,
            },
        ],
    }
    seg = _scene_json([_seg_kwargs(1, 0, acq, dat, pay)])["segments"][0]
    seg["snr"] = 99.0
    del seg["data"]  # the frame carries it
    seg["frame"] = frame

    c = Composer.from_json(json.dumps({**_scene_json([]), "segments": [seg]}))
    x_obj = np.asarray(c.compose())
    rec = json.loads(c.to_json())["segments"][0]
    kinds = [st["kind"] for st in rec["frame"]["stages"]]
    assert kinds == ["crc16", "conv"], "the record dropped a stage"
    assert "crc" not in rec, "a carried frame's record has no second CRC"

    x_replay = np.asarray(Composer.from_json(c.to_json()).compose())
    assert np.array_equal(x_obj, x_replay), "the record does not replay"


@pytest.mark.parametrize(
    "key", ["rs_depth", "randomise", "attach_asm", "convolutional"]
)
def test_the_coding_kwargs_are_gone(key):
    """A coded frame is a description (docs/design/frame-description.md R).

    The four source kwargs that used to spell stages are refused rather
    than accepted and ignored: a coded capture that silently came out
    uncoded is the failure the stages section exists to prevent.
    """
    acq, dat, pay = _codes()
    with pytest.raises(TypeError):
        Segment(**_seg_kwargs(1, 0, acq, dat, pay), **{key: 1})


def test_invalid_geometry_raises_or_degrades():
    """Payload with no data code is invalid: the standalone Synth raises at
    first generation, with dp_wfm_source_error()'s reason."""
    acq, dat, pay = _codes()
    kw = _seg_kwargs(1, 0, acq, dat, pay)
    kw.pop("data_code")
    kw.pop("off_samples")
    s = Synth(**kw)
    with pytest.raises(ValueError, match="give data_code"):
        s.steps(64)


def test_repeats_burst_train_decodes():
    """One declaration is the whole burst train: ``repeats=5`` with a
    ranged gap renders five bursts whose gaps are all >= the range's lo
    bound and are drawn independently per instance, the first instance is
    byte-identical to a repeats-less segment (back-compat), and every
    burst decodes through ``BurstDemod`` CRC-valid with the exact
    payload."""
    acq, dat, pay = _codes()
    min_gap, max_gap = 4000, 12000
    kw = _seg_kwargs(1, 0, acq, dat, pay)
    kw["off_samples"] = (min_gap, max_gap)
    kw["repeats"] = 5
    kw["gap_noise"] = "off"  # zero-run walk needs silent gaps
    x = Composer([Segment(**kw)]).compose()
    assert len(x) >= 5 * (BURST_LEN + min_gap)

    one = dict(kw)
    one.pop("repeats")
    x1 = Composer([Segment(**one)]).compose()
    assert np.array_equal(x[: len(x1)], x1)  # instance 0 back-compat

    # bursts are BURST_LEN long; gaps are the zero runs between them
    starts, gaps, pos = [], [], 0
    for _ in range(5):
        starts.append(pos)
        pos += BURST_LEN
        nz = np.flatnonzero(x[pos:] != 0)
        gap = int(nz[0]) if len(nz) else len(x) - pos
        gaps.append(gap)
        pos += gap
    assert all(g >= min_gap for g in gaps)
    assert len(set(gaps)) > 1  # per-instance gap draws are distinct

    n_valid = 0
    for s in starts:
        bd = BurstDemod(dat, _rx_frame(pay), spc=SPC, chip_rate=FS / SPC)
        bd.set_preamble(acq, REPS)
        bd.set_prior(0.0, 0)
        bits = bd.demod(x[s : s + BURST_LEN])
        if _frame_ok(bits, pay):
            n_valid += 1
    assert n_valid == 5


def test_gap_noise_default_floor_and_decode():
    """By default (gh-409) the inter-burst gaps carry the segment's noise
    floor — the same power the in-burst AWGN has over fs — instead of
    digital silence, and the SigMF sidecar's per-instance annotations give
    the exact burst positions, so every burst still decodes from the
    noisy-gap capture. The gap draws are identical in both gap_noise
    modes (same hash), pinning the two renders to the same timeline."""
    import json

    acq, dat, pay = _codes()
    kw = _seg_kwargs(1, 0, acq, dat, pay)
    kw["off_samples"] = (4000, 12000)
    kw["repeats"] = 5
    comp = Composer([Segment(**kw)])
    x = comp.compose()
    off = Composer([Segment(**{**kw, "gap_noise": "off"})]).compose()
    assert len(x) == len(off)  # same draws → same timeline in both modes

    anns = json.loads(comp.to_sigmf(sample_type="cf32", fs=FS))["annotations"]
    assert len(anns) == 5
    starts = [int(a["core:sample_start"]) for a in anns]
    assert all(int(a["core:sample_count"]) == BURST_LEN for a in anns)

    # gap noise power == the esno-resolved floor (10 dB over sf*sps -> -10
    # dB fs -> power 10); use the tail gap after the last burst.
    gap = x[starts[-1] + BURST_LEN :]
    floor = 10 ** (-(10.0 - 10 * math.log10(DATA_SF * SPC)) / 10)
    assert len(gap) >= 4000 and np.all(gap != 0)
    power = float(np.mean(np.abs(gap) ** 2))
    assert power == pytest.approx(floor, rel=0.15)
    # ... while the burst region carries signal + that same floor
    burst_p = float(np.mean(np.abs(x[starts[0] : starts[0] + BURST_LEN]) ** 2))
    assert burst_p == pytest.approx(floor + 1.0, rel=0.15)

    n_valid = 0
    for s in starts:
        bd = BurstDemod(dat, _rx_frame(pay), spc=SPC, chip_rate=FS / SPC)
        bd.set_preamble(acq, REPS)
        bd.set_prior(0.0, 0)
        bits = bd.demod(x[s : s + BURST_LEN])
        if _frame_ok(bits, pay):
            n_valid += 1
    assert n_valid == 5


def test_delay_samples_arrival_jitter():
    """delay_samples places the burst after a leading gap: the delayed
    render is the delay-less render shifted (clean case), a ranged delay
    re-draws per instance, and the CLI records/replays it byte-exactly."""
    acq, dat, pay = _codes()
    kw = _seg_kwargs(3, 0, acq, dat, pay)
    kw.pop("off_samples")
    kw["gap_noise"] = "off"
    x0 = Composer([Segment(**kw)]).compose()
    xd = Composer([Segment(**{**kw, "delay_samples": 500})]).compose()
    assert len(xd) == len(x0) + 500
    assert np.all(xd[:500] == 0)
    assert np.array_equal(xd[500:], x0)
