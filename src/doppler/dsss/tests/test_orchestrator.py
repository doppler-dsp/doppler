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

from doppler.dsss.orchestrator import Acquirer, CoarseChannel

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
