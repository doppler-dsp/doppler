"""Tests for the elastic coarse-Doppler acquirer (``dsss.orchestrator``).

A single Acquisition only searches its native span (``±chip_rate/(2*sf)``); the
bank tiles coarse-Doppler channels to cover a wider uncertainty.  These drive
the bank with a noisy DSSS burst at an absolute Doppler that lands *outside*
the center channel and check it is acquired in the right channel at the right
absolute Doppler — and that the threaded fan-out matches a serial run.
"""

import math

import numpy as np
import pytest

from doppler.dsss import (
    BurstCapture,
    PersistentBurstCapture,
    bin_to_signed,
)
from doppler.dsss.orchestrator import Acquirer, CoarseChannel, Detection
from doppler.dsss.tests.characterization.burst_capture import (
    characterize as _bc,
)

CODE = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
SF = 7
CHIP_RATE = 1.0e6
SPC = 2
SOURCE_RATE = 8.0e6
SPC_SRC = int(SOURCE_RATE / CHIP_RATE)  # 8 samples/chip at source rate
REPS = 8
U_HZ = 3.0e5  # Doppler uncertainty the bank must cover


def _burst(f_true, n_seg, snr_amp, seed):
    """A noisy DSSS burst at absolute Doppler ``f_true`` (source rate, cp0)."""
    rng = np.random.default_rng(seed)
    chips = np.where(CODE & 1, -1.0, 1.0)
    seg = np.tile(np.repeat(chips, SPC_SRC), n_seg)
    car = np.exp(2j * np.pi * (f_true / SOURCE_RATE) * np.arange(len(seg)))
    sigma = 1.0 / snr_amp
    noise = (sigma / math.sqrt(2.0)) * (
        rng.standard_normal(len(seg)) + 1j * rng.standard_normal(len(seg))
    )
    return (seg * car + noise).astype(np.complex64)


def _bank(**kw):
    base = {
        "doppler_uncertainty_hz": U_HZ,
        "source_rate": SOURCE_RATE,
        "spc": SPC,
        "chip_rate": CHIP_RATE,
        "reps": REPS,
        "cn0_dbhz": 35.0,
        "pfa": 1e-3,
    }
    base.update(kw)
    return Acquirer(CODE, **base)


# ── bank geometry ────────────────────────────────────────────────────────────


def test_bank_covers_uncertainty():
    """Odd channel count, centered on 0, spanning ±uncertainty by ±span."""
    with _bank() as acq:
        centers = acq.centers_hz
        assert acq.n_channels % 2 == 1  # symmetric about DC
        assert centers[acq.n_channels // 2] == 0.0
        # spacing == one native span (2*span_hz) and coverage reaches ±U
        step = centers[1] - centers[0]
        assert step == pytest.approx(2.0 * acq.span_hz)
        assert max(centers) >= U_HZ - acq.span_hz


def test_channel_rejects_oversampled_source():
    """A source slower than the acq rate is a construction error."""
    with pytest.raises(ValueError):
        CoarseChannel(
            0.0,
            source_rate=1.0e5,  # < chip_rate*spc = 2e6
            code=CODE,
            reps=REPS,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=35.0,
            pfa=1e-3,
            pd=0.9,
        )


# ── acquisition across the bank ──────────────────────────────────────────────


@pytest.mark.parametrize("f_true", [0.0, 1.5e5, -2.6e5])
def test_acquires_offcenter_target(f_true):
    """A target anywhere in ±U is acquired at the right absolute Doppler."""
    sig = _burst(f_true, REPS * 4, snr_amp=0.3, seed=7)
    with _bank() as acq:
        best = acq.acquire(sig)
        assert best is not None
        # within one Doppler bin of the truth, at the injected code phase
        assert abs(best.doppler_hz - f_true) <= acq.res_hz
        assert best.code_phase == 0
        # the owning channel's center is the nearest one to f_true
        nearest = min(acq.centers_hz, key=lambda c: abs(c - f_true))
        assert acq.centers_hz[best.channel] == nearest


def test_offcenter_uses_noncenter_channel():
    """Coverage beyond the native span genuinely uses a non-center channel."""
    with _bank() as acq:
        f_true = 2.0 * acq.span_hz + 1.0e4  # well outside the center channel
        best = acq.acquire(_burst(f_true, REPS * 4, snr_amp=0.3, seed=11))
        assert best is not None
        assert best.channel != acq.n_channels // 2  # not the DC channel
        assert abs(best.doppler_hz - f_true) <= acq.res_hz


def test_threaded_matches_serial():
    """The thread-pool fan-out yields exactly the serial per-channel result."""
    sig = _burst(1.5e5, REPS * 4, snr_amp=0.3, seed=3)
    with _bank(max_workers=1) as serial, _bank(max_workers=8) as threaded:
        a = serial.process(sig)
        b = threaded.process(sig)
    key = lambda ds: sorted(  # noqa: E731
        (round(d.doppler_hz, 3), d.code_phase, d.channel) for d in ds
    )
    assert key(a) == key(b)


def test_no_target_no_acquire():
    """Pure noise stays under the per-channel CFAR budget (no acquire)."""
    rng = np.random.default_rng(123)
    n = REPS * 4 * SF * SPC_SRC
    noise = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )
    with _bank() as acq:
        # A handful of channels x a low pfa: expect no detection on noise-only.
        assert acq.acquire(noise) is None


# ── elastic state hand-off (the pod-migration payoff) ────────────────────────

_CH_KW = {
    "source_rate": SOURCE_RATE,
    "code": CODE,
    "reps": REPS,
    "spc": SPC,
    "chip_rate": CHIP_RATE,
    "cn0_dbhz": 35.0,
    "pfa": 1e-3,
    "pd": 0.9,
}


def test_the_doppler_fold_is_the_librarys_and_not_a_copy():
    """`_abs_doppler` must agree with `bin_to_signed` at EVERY bin.

    The fold used to be spelled out here. It agreed with the canonical form
    everywhere — checked exhaustively for `n < 40` before it was removed —
    and that is precisely why it was worth removing rather than leaving: a
    copy that agrees is a copy that can stop agreeing, and this particular
    arithmetic has already cost a receiver reporting `tracking == 1` while
    decoding noise (`clib_common.h`, the wideband search against its own
    hand-off).

    The check walks the whole grid rather than sampling it, because two
    spellings differ at ONE index — the Nyquist bin of an even grid, where
    `+n/2` and `-n/2` name the same frequency and a consumer seeded on the
    wrong side of it is off by the full search span. A test that checked
    three bins in the middle would pass against a broken fold.

    **What this catches, and what it does not.** Proven by sabotage both ways:
    re-inlining the *old* copy leaves it GREEN, because that copy agreed;
    changing the threshold to `> n // 2` — the Nyquist-side spelling, which
    is the one that cost a receiver `tracking == 1` on noise — turns it RED.
    So this gates the DIVERGENCE, not the DUPLICATION. A future copy that
    happens to agree will pass here and will still be a copy. Nothing
    textual was added to catch that: a regex for the shape would be brittle
    and would be suppressed rather than obeyed, which is worse than an
    honest gap (doppler#1168).
    """
    ch = CoarseChannel(
        0.0,
        source_rate=SOURCE_RATE,
        code=CODE,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=35.0,
        pfa=1e-3,
        pd=0.9,
    )
    n = ch._nbins
    res = ch._res
    for b in range(n):
        assert ch._abs_doppler(b) == pytest.approx(
            ch.f_hz + bin_to_signed(b, n) * res
        ), f"bin {b} of {n} disagrees with the library's fold"

    # ...and the offset really does move the answer, so the loop above is not
    # comparing two expressions that are both anchored at zero.
    off = CoarseChannel(
        1234.0,
        source_rate=SOURCE_RATE,
        code=CODE,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=35.0,
        pfa=1e-3,
        pd=0.9,
    )
    assert off._abs_doppler(0) == pytest.approx(1234.0)


def test_channel_state_roundtrip_and_reject():
    """A CoarseChannel serializes its DDC + Acquisition state and restores it
    idempotently into a fresh identical channel; a corrupt blob is rejected."""
    a = CoarseChannel(0.0, **_CH_KW)
    a.process(_burst(0.0, REPS, snr_amp=0.5, seed=1), 0)
    blob = a.get_state()

    b = CoarseChannel(0.0, **_CH_KW)  # same descriptor
    b.set_state(blob)
    # Behavioral resume: from the restored state, the next block produces the
    # exact same detections as continuing the original channel.
    nxt = _burst(0.0, REPS, snr_amp=0.5, seed=2)
    assert b.process(nxt, 0) == a.process(nxt, 0)

    with pytest.raises(ValueError):  # truncated
        b.set_state(blob[:-1])
    with pytest.raises(ValueError):  # wrong magic / garbage of right length
        b.set_state(b"\x00" * len(blob))
    with pytest.raises(TypeError):  # not bytes
        b.set_state(42)


def test_bank_pod_handoff_resumes_bit_exact():
    """The headline: checkpoint a bank mid-stream, rebuild it from its
    descriptor on a fresh "pod", restore the blob — and the continuation is
    bit-for-bit identical to an uninterrupted run, target still acquired."""
    f_true = 1.5e5  # off-center, lands in a non-DC channel
    sig = _burst(f_true, REPS * 4, snr_amp=0.3, seed=7)
    cut = len(sig) // 8  # short warm-up, then hand off mid-stream

    # Uninterrupted reference.
    ref = _bank()
    r1 = ref.process(sig[:cut])
    r2 = ref.process(sig[cut:])

    # Checkpoint after the warm-up; hand the blob to a fresh identical bank.
    a = _bank()
    a.process(sig[:cut])
    blob = a.get_state()

    b = _bank()  # rebuilt from the same descriptor — the "other pod"
    b.set_state(blob)
    r2b = b.process(sig[cut:])

    # The resumed continuation is detection-for-detection identical to the
    # uninterrupted run — and the target survived the migration. (The blobs
    # themselves aren't byte-compared: the Acquisition ring leaves its unused
    # tail uninitialized, so behavioral equality is the meaningful invariant.)
    assert r2b == r2
    assert any(abs(d.doppler_hz - f_true) <= ref.res_hz for d in r1 + r2)

    for bank in (ref, a, b):
        bank.close()


# ── Capturing channels (doppler#1174) ───────────────────────────────────────
#
# The stimulus is the burst_capture characterization's scene, so the channel
# is built on THAT geometry -- chip rate, spc, reps -- and the source rate is
# the scene's own sample rate. The first version of these tests declared the
# module's detector geometry (spc 2, reps 8) against a scene made at spc 4,
# reps 4, and found bursts anyway at this SNR: a search matched to the wrong
# chip length, measuring nothing this section claims.

_CAP_KW = {
    "source_rate": _bc.CHIP_RATE * _bc.SPC,
    "reps": _bc.REPS,
    "spc": _bc.SPC,
    "chip_rate": _bc.CHIP_RATE,
    "cn0_dbhz": 55.0,
    "pfa": 1e-3,
    "pd": 0.9,
}


def _burst_scene():
    """Two bursts in noise, at known positions in the scene's own stream."""
    return (
        _bc.acq_code(),
        _bc.scene([9000, 60_000], 200_000, seed=11),
        _bc.BURST_LEN,
    )


def _cap_channel(burst_len, code, **kw):
    return CoarseChannel(0.0, code=code, burst_len=burst_len, **_CAP_KW, **kw)


def _cap_bank(burst_len, code, **kw):
    """Three channels: the scene's burst at DC lands in the centre one, and
    the neighbours sit a full `2*span` away, where the fast-time sinc nulls
    it."""
    return Acquirer(
        code,
        doppler_uncertainty_hz=2.0e4,
        burst_len=burst_len,
        **_CAP_KW,
        **kw,
    )


def test_a_detector_channel_is_unchanged():
    """No `burst_len` means the object that was always there.

    The migration is additive: a caller who wants detections gets exactly the
    channel they had, built on `BurstAcquisition`, and `bursts()` is empty
    rather than an error.
    """
    code, x, _ = _burst_scene()
    ch = CoarseChannel(0.0, code=code, **_CAP_KW)
    assert ch.burst_len == 0
    assert ch.process(x, 0)
    assert ch.bursts()[0].size == 0


def test_a_ring_without_a_burst_length_is_refused():
    """A ring is the look-back a CAPTURE reaches into; a detector has none.

    Refused at construction rather than ignored, so a caller who asked for a
    file-backed channel cannot silently get a detector with no file.
    """
    code, _, _ = _burst_scene()
    with pytest.raises(ValueError, match="burst_len"):
        CoarseChannel(0.0, code=code, ring_path="/nonexistent", **_CAP_KW)
    with pytest.raises(ValueError, match="burst_len"):
        Acquirer(
            code,
            doppler_uncertainty_hz=0.0,
            ring_dir="/nonexistent",
            **_CAP_KW,
        )


def test_a_capturing_channel_reports_both_faces():
    """One object, both outputs — which is why the capture publishes the
    detections it made.

    Without `detections()` a bank wanting both would push every sample
    through two acquisition engines. The detections are what the SEARCH
    found, so there are at least as many as there are bursts.
    """
    code, x, burst_len = _burst_scene()
    ch = _cap_channel(burst_len, code)
    assert isinstance(ch._acq, BurstCapture)
    dets = ch.process(x, 0)
    win, ev = ch.bursts()

    assert dets, "a capturing channel still reports detections"
    assert all(isinstance(d, Detection) for d in dets)
    assert win.size % burst_len == 0 and win.size > 0
    assert len(ev) == win.size // burst_len
    assert len(dets) >= len(ev), (
        "the search cannot find fewer hits than it turned into bursts"
    )


def test_a_burst_position_indexes_the_stream_the_capture_saw():
    """`preamble_start` means an index into the DDC's OUTPUT.

    That is the stream the capture searched, the stream its ring holds, and
    the stream the window is a slice of — so the assertion is exact rather
    than approximate: the window handed back must equal
    `ddc_output[start : start+burst_len]`, sample for sample.

    Written this way after the first version compared the starts against the
    SOURCE positions and read the difference as an error to be corrected. It
    is not one: the DDC's group delay ahead of the capture is not something
    these positions have to undo, because nothing downstream of them works in
    source coordinates.
    """
    code, x, burst_len = _burst_scene()
    ch = _cap_channel(burst_len, code)
    ch.process(x, 0)
    win, ev = ch.bursts()
    assert len(ev), "the scene has bursts in it"

    # The same DDC configuration, fed the same block: what the capture saw.
    probe = _cap_channel(burst_len, code)
    seen = probe._ddc.execute(x)

    for i, start in enumerate(int(v) for v in ev["preamble_start"]):
        window = win[i * burst_len : (i + 1) * burst_len]
        assert np.array_equal(window, seen[start : start + burst_len]), (
            f"burst {i} at {start} is not a slice of the stream searched"
        )


def test_a_file_backed_channel_shrinks_the_checkpoint(tmp_path):
    """The bank is the pod-shippable object, so its blob is the thing to
    shrink: the look-back is nearly all of an in-RAM one."""
    code, x, burst_len = _burst_scene()
    ram = _cap_channel(burst_len, code)
    dsk = _cap_channel(burst_len, code, ring_path=str(tmp_path / "ch0.cf32"))
    assert isinstance(dsk._acq, PersistentBurstCapture)

    ram.process(x, 0)
    dsk.process(x, 0)
    assert len(dsk.get_state()) < len(ram.get_state())
    # Same answer either way: where the ring's pages live is not a DSP
    # parameter.
    assert len(ram.bursts()[1]) == len(dsk.bursts()[1])
    assert (tmp_path / "ch0.cf32").stat().st_size > 0


# ── The bank fans bursts out as it does detections ──────────────────────────


def test_a_detector_bank_has_no_bursts():
    """`bursts()` on the bank that always existed: one empty entry per
    channel, not an error."""
    code, x, _ = _burst_scene()
    with Acquirer(code, doppler_uncertainty_hz=2.0e4, **_CAP_KW) as bank:
        assert bank.burst_len == 0
        assert bank.process(x)
        got = bank.bursts()
        assert len(got) == bank.n_channels
        assert all(w.size == 0 and len(e) == 0 for w, e in got)


def test_a_capturing_bank_reports_both_faces_per_channel():
    """One `process()` fills both: the dedup'd detections it returns and the
    windows each channel completed. A detection's `channel` is the index
    into `bursts()` where its burst is -- and here the burst is at DC, so
    that is the centre channel, whose neighbours a full `2*span` away are
    nulled by the fast-time sinc."""
    code, x, burst_len = _burst_scene()
    with _cap_bank(burst_len, code) as bank:
        assert bank.n_channels == 3
        assert all(isinstance(c._acq, BurstCapture) for c in bank.channels)
        dets = bank.process(x)
        got = bank.bursts()

    assert dets
    assert len(got) == bank.n_channels
    centre = bank.n_channels // 2
    strongest = sorted(dets, key=lambda d: -d.test_stat)[:2]
    assert all(d.channel == centre and d.doppler_hz == 0.0 for d in strongest)
    win, ev = got[centre]
    # At least the two transmitted bursts; a spurious window is EXPECTED at
    # pfa=1e-3 over a scene this long (the object's validation report, F2),
    # and the two real ones are the two the search rated highest.
    assert len(ev) >= 2 and win.size == len(ev) * burst_len
    real = sorted(ev, key=lambda e: -float(e["cn0_dbhz_est"]))[:2]
    starts = sorted(int(e["preamble_start"]) for e in real)
    assert starts[1] - starts[0] == 60_000 - 9000, (
        "the transmitted spacing, so these are the transmitted bursts"
    )
    # ...and the fan-out is the serial answer, window for window.
    ch = _cap_channel(burst_len, code)
    ch.process(x, 0)
    ref_win, ref_ev = ch.bursts()
    assert np.array_equal(win, ref_win)
    assert np.array_equal(ev, ref_ev)


def test_a_target_on_a_channel_boundary_is_acquired():
    """A target at exactly one span -- midway between two channel centres --
    is acquired by the bank, at the right absolute Doppler.

    This was the coverage hole of doppler#1179: at an even coherent depth the
    engine never searched its outermost native bin, so a target on the
    boundary was seen by NEITHER neighbour (test_stat 31 -> 23 -> 12 -> 6 ->
    0 from DC to one span). The hole was the engine's (doppler#1183), and
    this is the bank-side pin that it stays closed. The boundary is the
    Nyquist bin, sign-folded, so the estimate is checked to within one bin
    of |f| rather than of f: the channel above the target sees it with the
    right sign, the channel below may not, and the dedup keeps the stronger.
    """
    code, x, burst_len = _burst_scene()
    with _cap_bank(burst_len, code) as bank:
        f = bank.span_hz
        n = np.arange(x.size)
        shifted = (
            x * np.exp(2j * np.pi * f / _CAP_KW["source_rate"] * n)
        ).astype(np.complex64)
        dets = bank.process(shifted)
        got = bank.bursts()
    assert dets, "the boundary is searched"
    best = max(dets, key=lambda d: d.test_stat)
    assert abs(abs(best.doppler_hz) - f) <= bank.res_hz
    assert len(got[best.channel][1]) >= 1, "and its window is captured"


def test_an_offcentre_burst_is_read_from_the_channel_the_dedup_kept():
    """A target at half a span off DC is still the centre channel's, and the
    dedup'd detection's `channel` is the index into `bursts()` where its
    window is -- the claim `bursts()` makes about how to read it.

    Its `preamble_start` is the SAME as the unshifted burst's: every channel
    decimates by the same factor, so a position is on one sample grid across
    the bank and does not move with the mix.

    Half a span, so the burst is unambiguously the centre channel's. The
    boundary itself is the previous test's (doppler#1179, #1183).
    """
    code, x, burst_len = _burst_scene()
    with _cap_bank(burst_len, code) as bank:
        f = 0.5 * bank.span_hz
        n = np.arange(x.size)
        shifted = (
            x * np.exp(2j * np.pi * f / _CAP_KW["source_rate"] * n)
        ).astype(np.complex64)
        dets = bank.process(shifted)
        got = bank.bursts()
        centre = bank.n_channels // 2
        ref = bank.channels[centre]

    assert dets
    best = max(dets, key=lambda d: d.test_stat)
    assert best.channel == centre
    assert abs(best.doppler_hz - f) <= bank.res_hz
    _w, ev = got[best.channel]
    assert len(ev) >= 2, "the channel the dedup kept holds the windows"

    unshifted = _cap_channel(burst_len, code)
    unshifted.process(x, 0)
    grid = sorted(int(v) for v in unshifted.bursts()[1]["preamble_start"])
    real = sorted(ev, key=lambda e: -float(e["cn0_dbhz_est"]))[:2]
    assert sorted(int(e["preamble_start"]) for e in real) == grid[:2], (
        "a position does not move with the mix"
    )
    assert ref.f_hz == 0.0


def test_a_file_backed_bank_names_a_ring_per_centre(tmp_path):
    """One ring file per channel, named by the channel's CENTRE.

    Named by centre rather than index so that widening the bank -- which
    adds channels at the edges -- keeps every existing ring pointing at the
    sub-band it holds the history of. And the bank's blob shrinks by every
    channel's look-back, which is the number the pod hand-off pays."""
    code, x, burst_len = _burst_scene()
    rings = tmp_path / "rings"
    with (
        _cap_bank(burst_len, code) as ram,
        _cap_bank(burst_len, code, ring_dir=rings) as dsk,
    ):
        assert all(
            isinstance(c._acq, PersistentBurstCapture) for c in dsk.channels
        )
        names = sorted(p.name for p in rings.iterdir())
        assert names == sorted(f"ch{c:+.0f}Hz.cf32" for c in dsk.centers_hz)
        assert all((rings / n).stat().st_size > 0 for n in names)

        ram.process(x)
        dsk.process(x)
        assert len(dsk.get_state()) < len(ram.get_state())
        assert [len(e) for _w, e in ram.bursts()] == [
            len(e) for _w, e in dsk.bursts()
        ]

        # Widening the bank keeps the centre channels' rings, and adds two.
        wide = Acquirer(
            code,
            doppler_uncertainty_hz=2.0 * 2.0e4,
            burst_len=burst_len,
            ring_dir=rings,
            **_CAP_KW,
        )
        wide.close()
        assert set(names) < {p.name for p in rings.iterdir()}


# ── The pod hand-off, for a bank that captures ──────────────────────────────
#
# The reason the bank exists is that it ships: checkpoint here, rebuild from
# the descriptor on another pod, restore, continue. A capturing bank carries
# each channel's look-back in that blob (or in its ring file), so the burst
# that STRADDLES the hand-off is the one that proves it -- a detector bank
# only had to reproduce the next block's detections.


def _cut_mid_burst():
    """Where to split the scene: inside the second burst's preamble."""
    return 60_000 + 2 * _bc.ACQ_SF * _bc.SPC


def test_a_capturing_bank_resumes_bit_exact_across_a_pod_handoff():
    """Checkpoint mid-burst, restore into a fresh bank, and the continuation
    is window-for-window the uninterrupted run's -- including the burst that
    began before the cut, which only a travelling look-back can complete."""
    code, x, burst_len = _burst_scene()
    cut = _cut_mid_burst()

    with _cap_bank(burst_len, code) as ref:
        ref.process(x[:cut])
        r2 = ref.process(x[cut:])
        ref_bursts = ref.bursts()

    with _cap_bank(burst_len, code) as a:
        a.process(x[:cut])
        blob = a.get_state()

    with _cap_bank(burst_len, code) as b:  # the other pod
        b.set_state(blob)
        r2b = b.process(x[cut:])
        got = b.bursts()

    assert r2b == r2
    for (w, e), (rw, re_) in zip(got, ref_bursts):
        assert np.array_equal(w, rw)
        assert np.array_equal(e, re_)
    centre = ref.n_channels // 2
    starts = [int(v) for v in got[centre][1]["preamble_start"]]
    assert any(s < cut < s + burst_len for s in starts), (
        "the burst straddling the hand-off was completed after it"
    )


def test_a_file_backed_bank_resumes_across_a_restart(tmp_path):
    """The HPA case: the pod dies, a new one mounts the same volume.

    The rings ARE the history, so the new bank built over the same
    `ring_dir` adopts them and the blob restores into it -- and finishes
    the burst the old pod was in the middle of. Against a ring directory
    that is EMPTY the same blob is refused: it claims retained history that
    is not there, and resuming into silence would lose the burst quietly.
    """
    code, x, burst_len = _burst_scene()
    cut = _cut_mid_burst()
    rings = tmp_path / "volume"

    with _cap_bank(burst_len, code) as ref:
        ref.process(x[:cut])
        r2 = ref.process(x[cut:])
        ref_bursts = ref.bursts()

    with _cap_bank(burst_len, code, ring_dir=rings) as old_pod:
        old_pod.process(x[:cut])
        blob = old_pod.get_state()
    # The old pod is gone; only the volume and the blob survive.

    with _cap_bank(burst_len, code, ring_dir=rings) as new_pod:
        new_pod.set_state(blob)
        assert new_pod.process(x[cut:]) == r2
        for (w, e), (rw, re_) in zip(new_pod.bursts(), ref_bursts):
            assert np.array_equal(w, rw)
            assert np.array_equal(e, re_)

    with (
        _cap_bank(burst_len, code, ring_dir=tmp_path / "empty") as other,
        pytest.raises(ValueError),
    ):
        other.set_state(blob)
