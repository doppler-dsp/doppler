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
    `est_snr_db` does not move, `est_rate_hz` still reports 0, and the frame
    still decodes -- so nothing in the pre-#1087 read-back surface reveals
    it. That is the difference between a pointing problem and a clean link.
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
    # ...and the scalar estimates do not separate them either.
    assert rated.est_snr_db == pytest.approx(clean.est_snr_db, abs=1.0)
    assert rated.est_rate_hz == pytest.approx(0.0, abs=1.0)
    # Only the quadrature does.
    assert qi_rated > 5.0 * qi_clean, (
        f"Q/I {qi_rated:.5f} vs clean {qi_clean:.5f} -- the quadrature is "
        "the axis that shows a phase-coherence problem"
    )
