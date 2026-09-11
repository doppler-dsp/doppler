"""Tests for the feedforward BPSK DSSS frame demodulator (burst_demod).

Build a full burst — an unmodulated 5x500 acquisition preamble followed by a
50-chip-spread frame (sync header | payload | CRC-16) — apply a carrier with
Doppler and Doppler rate, and check the demod recovers the frame's bits,
feedforward, across both regimes the one ``max_rate`` knob spans: near-static
Doppler and a severe LEO chirp.

**The demodulator hands back the FRAME**, sync word first, and makes no claim
about what the bits are for: this object stops at decisions, and undoing a
frame needs a description it deliberately does not hold (doppler#1022). So the
payload is a slice at ``PAYLOAD_OFF`` here, and the CRC is checked by this
file — which is what a caller does, and what ``wfm.Frame.deframe()`` does for
one that holds a description.
"""

import numpy as np
import pytest

from doppler.dsss import BurstDemod

ACQ_SF, REPS, DATA_SF, SPC = 500, 5, 50, 4
PAYLOAD = 64
PAYLOAD_OFF = 13  # the sync word comes first in every frame here
FRAME_SYMS = PAYLOAD_OFF + PAYLOAD + 16  # sync | payload | CRC-16
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC
# Barker-13 frame-sync word (0/1).
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8)

_ACODE = ((np.arange(ACQ_SF) * 2654435761 >> 13) & 1).astype(np.uint8)
_DCODE = ((np.arange(DATA_SF) * 40503 >> 7) & 1).astype(np.uint8)


def _csign(b):
    return np.where(np.asarray(b) & 1, -1.0, 1.0)


def _crc16(bits):
    c = 0xFFFF
    for b in bits:
        c ^= (int(b) & 1) << 15
        c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def _payload_of(frame):
    """The payload's slice out of a returned frame."""
    return np.asarray(frame)[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD]


def _frame_ok(frame):
    """The caller's half: does the frame's own trailer match its payload?

    `wfm.Frame.deframe()` is the shipped way to ask; four lines here keep
    this file testing the demodulator rather than the frame object.
    """
    frame = np.asarray(frame)
    if frame.size < PAYLOAD_OFF + PAYLOAD + 16:
        return False
    rx = 0
    for b in frame[PAYLOAD_OFF + PAYLOAD :][:16]:
        rx = (rx << 1) | (int(b) & 1)
    return rx == _crc16(_payload_of(frame))


def _burst(payload, f0, mu, *, rng=None, sigma=0.0):
    """Preamble (5x500 unmod) + frame (sync|payload|crc), carrier-modulated."""
    crc = _crc16(payload)
    crc_bits = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([SYNC, payload, crc_bits])
    chips = [np.tile(_csign(_ACODE), REPS)]  # unmodulated preamble
    chips += [_csign(b) * _csign(_DCODE) for b in frame]
    bb = np.repeat(np.concatenate(chips), SPC).astype(np.complex64)
    n = np.arange(len(bb))
    y = bb * np.exp(2j * np.pi * (f0 * n + 0.5 * mu * n * n))
    if sigma and rng is not None:
        y = y + (sigma / np.sqrt(2.0)) * (
            rng.standard_normal(len(y)) + 1j * rng.standard_normal(len(y))
        )
    return y.astype(np.complex64)


def _make(max_rate):
    d = BurstDemod(_DCODE, SPC, CHIP_RATE, 0.0, max_rate, FRAME_SYMS, 10)
    d.set_preamble(_ACODE, REPS)
    d.set_sync(SYNC)
    return d


def test_static_doppler_decodes():
    """Near-static Doppler (negligible rate, max_rate=0): full frame + CRC."""
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    d = _make(0.0)
    d.set_prior(0.012, 0)
    bits = d.demod(_burst(payload, 0.012, 0.0))
    assert _frame_ok(bits), "the frame's own trailer must check out"
    assert np.array_equal(_payload_of(bits), payload)
    assert abs(d.est_freq_hz - 0.012 * FS) < 100.0  # within 100 Hz


def test_leo_chirp_decodes():
    """Severe LEO chirp + an offset coarse prior: the feedforward estimate
    recovers Doppler + rate, dechirps, and the frame decodes with CRC valid."""
    payload = ((np.arange(PAYLOAD) * 5 + 1) & 1).astype(np.uint8)
    f0, mu = 0.012, 6.0e-7
    d = _make(1.0e-6)
    d.set_prior(0.0115, 0)  # coarse prior off by ~2 kHz
    bits = d.demod(_burst(payload, f0, mu))
    assert _frame_ok(bits), "the frame's own trailer must check out"
    assert np.array_equal(_payload_of(bits), payload)
    assert abs(d.est_freq_hz - f0 * FS) < 100.0
    assert abs(d.est_rate_hz - mu * FS * FS) / (mu * FS * FS) < 0.05  # 5%


def test_leo_decodes_under_noise():
    """The LEO frame still decodes (CRC valid) at a workable SNR."""
    payload = ((np.arange(PAYLOAD) * 3 + 2) & 1).astype(np.uint8)
    f0, mu = -0.01, -5.0e-7
    sigma = 10 ** (-6.0 / 20.0)  # ~6 dB/sample; despread gain lifts the symbol
    oks = 0
    for seed in range(6):
        rng = np.random.default_rng(seed)
        d = _make(1.0e-6)
        d.set_prior(f0 + 5e-4, 0)
        bits = d.demod(_burst(payload, f0, mu, rng=rng, sigma=sigma))
        if _frame_ok(bits) and np.array_equal(_payload_of(bits), payload):
            oks += 1
    assert oks >= 5  # robust across seeds


def test_bad_args():
    # An empty data code -> create() returns NULL -> jm raises MemoryError.
    with pytest.raises((ValueError, TypeError, MemoryError)):
        BurstDemod(
            np.array([], np.uint8), SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, 10
        )


def test_demod_out_writes_into_callers_buffer():
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    d = _make(0.0)
    d.set_prior(0.012, 0)
    x = _burst(payload, 0.012, 0.0)
    # out= validation requires max(demod_max_out(), len(x)): the kernel's
    # scratch use scales with the input burst length, not just the payload.
    out = np.zeros(max(d.demod_max_out(), len(x)), dtype=np.uint8)
    bits = d.demod(x, out=out)
    assert np.shares_memory(bits, out)
    assert _frame_ok(bits), "the frame's own trailer must check out"
    assert np.array_equal(_payload_of(bits), payload)


def test_demod_out_undersized_raises():
    d = _make(0.0)
    out = np.zeros(1, dtype=np.uint8)
    x = np.zeros(4, dtype=np.complex64)
    with pytest.raises(ValueError):
        d.demod(x, out=out)


def test_symbols_is_the_constellation_llrs_is_the_real_part_of():
    """`symbols()` and `llrs()` are one projection reported twice.

    The object built the derotated constellation either way -- the LLR
    projection and the noise estimate are both made from it -- and then freed
    it unread (doppler#1087). Same span, same normalisation, and the exact
    relation `llr = 4*Re/est_n0` so a caller can move between them.
    """
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    d = _make(0.0)
    d.set_prior(0.012, 0)
    bits = d.demod(_burst(payload, 0.012, 0.0))
    assert _frame_ok(bits)

    sym = np.asarray(d.symbols())
    llr = np.asarray(d.llrs())
    assert sym.dtype == np.complex64
    assert sym.size == llr.size == FRAME_SYMS
    assert d.symbols_max_out(1) == d.llrs_max_out(1) == FRAME_SYMS

    # The same decision, seen twice.
    assert np.array_equal((sym.real < 0).astype(np.uint8), bits[:FRAME_SYMS])
    # And the same numbers, up to the published scale.
    assert d.est_n0 > 0.0
    np.testing.assert_allclose(llr, 4.0 * sym.real / d.est_n0, rtol=1e-3)


def test_symbols_quadrature_shows_what_no_other_readback_does():
    """Q is why the constellation is worth keeping.

    After derotation the real axis carries the signal and the imaginary axis
    carries noise alone, so a phase-coherence problem lands in Q and nowhere
    else. Measured through this object, a Doppler rate the estimator is not
    configured to track raises Q/I by more than an order of magnitude while
    `est_rate_hz` still reports 0 and the frame still decodes, so neither
    says anything is wrong.

    `est_cn0_dbhz` DOES fall, by 101 dB on a noiseless input, which its
    predecessor `est_snr_db` did not: the noise is read off the quadrature,
    and a rotation puts signal there. That is an improvement -- the problem
    is no longer invisible in the read-back surface -- but it is not a
    diagnosis. A phase error and a genuine noise floor produce the SAME
    C/N0, because to that estimator they are the same energy in the same
    axis. Only Q against I says which one happened, which is why the
    constellation is kept (doppler#1087, doppler#1304).
    """
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)

    def run(mu):
        d = _make(0.0)  # max_rate 0: the rate below is NOT tracked
        d.set_prior(0.012, 0)
        bits = d.demod(_burst(payload, 0.012, mu))
        sym = np.asarray(d.symbols())
        qi = float(np.sum(sym.imag**2) / max(np.sum(sym.real**2), 1e-30))
        return d, bits, qi

    clean, clean_bits, qi_clean = run(0.0)
    rated, rated_bits, qi_rated = run(6e-10)

    # Both still decode, so the bits say nothing is wrong...
    assert _frame_ok(clean_bits) and _frame_ok(rated_bits)
    # ...and the rate the estimator was not configured to track reads zero.
    assert rated.est_rate_hz == pytest.approx(0.0, abs=1.0)
    # The C/N0 does NOT hide it, which is the change from est_snr_db.
    assert rated.est_cn0_dbhz < clean.est_cn0_dbhz - 20.0, (
        "a rotation puts signal in the quadrature, and the noise estimate "
        "reads it as noise -- the C/N0 must fall rather than sit still"
    )
    # Only the quadrature does.
    assert qi_rated > 5.0 * qi_clean, (
        f"Q/I {qi_rated:.5f} vs clean {qi_clean:.5f} -- the quadrature is "
        "the axis that shows a phase-coherence problem"
    )


# ── est_cn0_dbhz: a CHANNEL quantity, through the binding ────────────────
#
# The C test pins the estimator; these pin what a Python caller sees, which
# is where doppler#1304 was reported from. `est_snr_db` used to sit here and
# could be compared with nothing: it published the preamble estimator's
# spectral prominence, carrying the coherent processing gain, so it read
# 59 dB on a 25 dB link and swung 33 dB with the burst start alone.

SYM_RATE = CHIP_RATE / DATA_SF


def _sigma_for(es_n0_db):
    """Per-sample noise sigma for a stated Es/N0.

    Es is `DATA_SF * SPC` samples of unit power, and `_burst` adds complex
    noise of total power `sigma**2`, so the per-sample SNR is the Es/N0 less
    the samples per symbol.
    """
    return 10 ** (-(es_n0_db - 10 * np.log10(DATA_SF * SPC)) / 20.0)


def _cn0(es_n0_db, *, lead=0, seed=0):
    """One burst at a known Es/N0, started `lead` samples late."""
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    rng = np.random.default_rng(700 + seed)
    sigma = _sigma_for(es_n0_db)
    y = _burst(payload, 0.012, 0.0, rng=rng, sigma=sigma)
    if lead:
        pad = (sigma / np.sqrt(2.0)) * (
            rng.standard_normal(lead) + 1j * rng.standard_normal(lead)
        )
        y = np.concatenate([pad.astype(np.complex64), y]).astype(np.complex64)
    d = _make(0.0)
    d.set_prior(0.012, 0)
    d.demod(y)
    return d


@pytest.mark.parametrize("es_n0_db", [10.0, 20.0, 30.0])
def test_cn0_reports_the_channel(es_n0_db):
    """C/N0 = Es/N0 * Rs, and the object is asked for a channel it was given.

    Over a 20 dB span, because the defect this replaced was a value that
    saturated: a prominence stops rising once the noise no longer sets the
    spectrum's mean, so it agreed with nothing at the top of the range.
    """
    want = es_n0_db + 10 * np.log10(SYM_RATE)
    got = [_cn0(es_n0_db, seed=s).est_cn0_dbhz for s in range(6)]
    assert np.median(got) == pytest.approx(want, abs=2.0)


@pytest.mark.parametrize("lead", list(range(SPC // 2 + 1)))
def test_cn0_is_flat_across_a_sub_chip_start_error(lead):
    """A receiver's own timing error is not a property of the channel.

    Acquisition resolves a start to one SAMPLE, so a residual fraction of a
    chip is structural. It costs the despreader real amplitude -- a realized
    SNR falls 4.8 dB at half a chip -- and the object removes what it can,
    measures the rest, and takes it back out of the reported number.
    """
    want = 25.0 + 10 * np.log10(SYM_RATE)
    got = [_cn0(25.0, lead=lead, seed=s).est_cn0_dbhz for s in range(6)]
    assert np.median(got) == pytest.approx(want, abs=2.0)


@pytest.mark.parametrize("lead", [1, 2])
def test_the_start_error_is_reported_not_just_removed(lead):
    """Half the point: a reader can see what was corrected."""
    tau = [_cn0(25.0, lead=lead, seed=s).est_timing_chips for s in range(6)]
    assert np.median(tau) == pytest.approx(lead / SPC, abs=0.1)


def test_cn0_does_not_depend_on_est_segments():
    """`est_segments` shapes the preamble estimator's partials, only.

    It used to decide the answer: `est_snr_db` was read off those partials,
    and a truncating `lseg_chips` with the remainder clamped into the last
    segment made one partial run long, which cost 23.8 dB. The C/N0 is
    measured on the DATA symbols instead, so the knob cannot reach it.

    This pins that separation rather than the partial-length defect, which
    is still there and is doppler#1305 -- measured to move nothing this
    object reports. What would fail here is someone wiring C/N0 back to the
    preamble estimator. ACQ_SF is 500, which 10 divides and 32 does not.
    """
    want = 25.0 + 10 * np.log10(SYM_RATE)
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    for segs in (5, 10, 32):
        got = []
        for s in range(6):
            rng = np.random.default_rng(800 + s)
            d = BurstDemod(_DCODE, SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, segs)
            d.set_preamble(_ACODE, REPS)
            d.set_sync(SYNC)
            d.set_prior(0.012, 0)
            d.demod(
                _burst(payload, 0.012, 0.0, rng=rng, sigma=_sigma_for(25.0))
            )
            got.append(d.est_cn0_dbhz)
        assert np.median(got) == pytest.approx(want, abs=2.0), (
            f"est_segments={segs} moved the channel's C/N0"
        )


# ── The sub-sample half of the timing term ──────────────────────────────
#
# The integer part of a start error is REMOVED by shifting; the fraction of
# a sample that a shift cannot reach is measured and taken back out of the
# reported C/N0. Two things have to be true for that half to be exercised at
# all, and neither is true of the tests above:
#
#   * the signal must be BAND-LIMITED. A rectangular chip sampled on-grid is
#     invariant to a sub-sample delay -- every sample stays inside the chip
#     it was in -- so there is no residual to correct. A front end filters,
#     and then there is.
#   * the search must have ROOM BEFORE the burst. The parabola needs the
#     point either side of its peak, so with `start = 0` it never runs and
#     the reported offset is always an exact multiple of 1/spc.
#
# Both were missing from the first version of these tests, which is why they
# passed with the correction removed.

_OS = 8  # build at _OS*SPC, band-limit, decimate back to SPC by phase
_LEAD = 64


def _burst_offgrid(payload, f0, phase, *, rng, sigma):
    """A burst whose start does NOT land on the sample grid.

    `phase` in [0, _OS) delays by `phase/_OS` of a sample.
    """
    crc = _crc16(payload)
    crc_bits = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([SYNC, payload, crc_bits])
    chips = [np.tile(_csign(_ACODE), REPS)]
    chips += [_csign(b) * _csign(_DCODE) for b in frame]
    hi = np.repeat(np.concatenate(chips), SPC * _OS).astype(float)
    h = np.hanning(2 * _OS + 1)
    hi = np.convolve(hi, h / h.sum(), mode="same")
    lo = hi[phase::_OS][: len(hi) // _OS].astype(np.complex64)
    n = np.arange(len(lo))
    y = lo * np.exp(2j * np.pi * f0 * n)
    y = y + (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(len(y)) + 1j * rng.standard_normal(len(y))
    )

    # Slack at BOTH ends. Before, so the parabola has the point either side
    # of its peak; after, because the search may only reach forward as far as
    # the frame does not already need -- a window sized exactly to one burst
    # lets it walk the last symbol off the end, which is an event with no
    # bits behind it.
    def _noise(n):
        return (sigma / np.sqrt(2.0)) * (
            rng.standard_normal(n) + 1j * rng.standard_normal(n)
        )

    return np.concatenate([_noise(_LEAD), y, _noise(_LEAD)]).astype(
        np.complex64
    )


def test_cn0_holds_across_a_sub_sample_start_error():
    """The residual the shift cannot remove is corrected, and it is worth it.

    Asserted as the SIGN of the phase dependence rather than a bias bound,
    because the band-limiting costs its own ~0.8 dB of correlation and that
    loss is common to every phase -- an absolute bound would be measuring
    the filter. What the correction owns is the part that VARIES with the
    offset: the phases with the largest sub-sample error must not read lower
    than the phases with the smallest.

    Measured at Es/N0 25 dB over eight phases, eight seeds each: corrected,
    the extremes read 0.27 dB ABOVE the centre (the triangular model over-
    corrects slightly for a filtered pulse, whose autocorrelation is rounder
    than a rectangular chip's). Uncorrected they read 0.50 dB BELOW it. The
    sign is the gate, so removing the correction turns this red with a
    0.77 dB margin rather than a hair.
    """
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    sigma = _sigma_for(25.0)
    per_phase = []
    for phase in range(_OS):
        got = []
        for s in range(8):
            rng = np.random.default_rng(900 + s)
            d = _make(0.0)
            d.set_prior(0.012, _LEAD)
            d.demod(
                _burst_offgrid(payload, 0.012, phase, rng=rng, sigma=sigma)
            )
            if d.est_cn0_dbhz:
                got.append(d.est_cn0_dbhz)
        assert got, f"phase {phase}: no burst produced a frame"
        per_phase.append(float(np.median(got)))

    # Phase 0 and _OS-1 carry the largest sub-sample residual; the middle
    # two carry the smallest.
    extremes = (per_phase[0] + per_phase[-1]) / 2.0
    centre = (per_phase[_OS // 2 - 1] + per_phase[_OS // 2]) / 2.0
    assert extremes > centre - 0.1, (
        f"the largest sub-sample offsets read {centre - extremes:.2f} dB "
        f"below the smallest -- the residual timing is not being taken out "
        f"of the C/N0 (per phase: {[round(v, 2) for v in per_phase]})"
    )


def test_the_sub_sample_offset_is_measured_not_quantised():
    """`est_timing_chips` resolves BELOW one sample when it has room to.

    Without the lead-in the parabola cannot run and every answer is an exact
    multiple of 1/spc, which would make the correction above a no-op that
    still looked like it worked.
    """
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    sigma = _sigma_for(25.0)
    taus = []
    for phase in range(_OS):
        vals = []
        for s in range(8):
            rng = np.random.default_rng(950 + s)
            d = _make(0.0)
            d.set_prior(0.012, _LEAD)
            d.demod(
                _burst_offgrid(payload, 0.012, phase, rng=rng, sigma=sigma)
            )
            vals.append(d.est_timing_chips)
        taus.append(float(np.median(vals)))
    quantum = 1.0 / SPC
    off_grid = [t for t in taus if abs(t / quantum - round(t / quantum)) > 0.1]
    assert len(off_grid) >= _OS - 2, (
        f"{len(off_grid)} of {_OS} phases resolved off the 1/spc grid -- "
        "the sub-sample interpolation is not running"
    )
    assert taus == sorted(taus, reverse=True), (
        f"the measured offset must move monotonically with the delay: {taus}"
    )
