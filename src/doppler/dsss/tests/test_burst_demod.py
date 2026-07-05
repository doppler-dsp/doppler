"""Tests for the feedforward BPSK DSSS frame demodulator (burst_demod).

Build a full burst — an unmodulated 5x500 acquisition preamble followed by a
50-chip-spread frame (sync header | payload | CRC-16) — apply a carrier with
Doppler and Doppler rate, and check the demod recovers the payload bits and the
CRC, feedforward, across both regimes the one ``max_rate`` knob spans:
near-static Doppler and a severe LEO chirp.
"""

import numpy as np
import pytest

from doppler.dsss import BurstDemod

ACQ_SF, REPS, DATA_SF, SPC = 500, 5, 50, 4
PAYLOAD = 64
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
    d = BurstDemod(_DCODE, SPC, CHIP_RATE, 0.0, max_rate, PAYLOAD, 10)
    d.set_preamble(_ACODE, REPS)
    d.set_sync(SYNC)
    return d


def test_static_doppler_decodes():
    """Near-static Doppler (negligible rate, max_rate=0): full frame + CRC."""
    payload = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
    d = _make(0.0)
    d.set_prior(0.012, 0)
    bits = d.demod(_burst(payload, 0.012, 0.0))
    assert d.frame_valid == 1
    assert np.array_equal(bits, payload)
    assert abs(d.est_freq_hz - 0.012 * FS) < 100.0  # within 100 Hz


def test_leo_chirp_decodes():
    """Severe LEO chirp + an offset coarse prior: the feedforward estimate
    recovers Doppler + rate, dechirps, and the frame decodes with CRC valid."""
    payload = ((np.arange(PAYLOAD) * 5 + 1) & 1).astype(np.uint8)
    f0, mu = 0.012, 6.0e-7
    d = _make(1.0e-6)
    d.set_prior(0.0115, 0)  # coarse prior off by ~2 kHz
    bits = d.demod(_burst(payload, f0, mu))
    assert d.frame_valid == 1
    assert np.array_equal(bits, payload)
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
        if d.frame_valid and np.array_equal(bits, payload):
            oks += 1
    assert oks >= 5  # robust across seeds


def test_bad_args():
    # An empty data code -> create() returns NULL -> jm raises MemoryError.
    with pytest.raises((ValueError, TypeError, MemoryError)):
        BurstDemod(
            np.array([], np.uint8), SPC, CHIP_RATE, 0.0, 0.0, PAYLOAD, 10
        )


def test_demod_returns_independent_arrays():
    """demod() returns a fresh array per call — not a view into the
    internal buffer. Regression test for the gh-219 class of aliasing bug:
    two consecutive same-size calls used to return numpy views of the same
    reused buffer, so a later call silently mutated an earlier-returned
    array out from under the caller."""
    d = _make(0.0)
    x = np.zeros(4, dtype=np.complex64)
    a, b = d.demod(x), d.demod(x)
    assert not np.shares_memory(a, b)
