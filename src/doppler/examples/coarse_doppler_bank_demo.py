"""coarse_doppler_bank_demo.py -- find a burst outside one search's span.

One burst search integrates coherently only within its native Doppler span,
``+/-chip_rate / (2 * sf)``. ``Acquirer`` covers a wider uncertainty with a
bank of ``CoarseChannel`` searches, each mixed to its own center. Here a
burst sits well outside the center channel; the bank finds it in an
off-center channel, at the right absolute Doppler, and that channel alone,
built the same way, reports the same hit.
"""

import math

import numpy as np

from doppler.dsss import Acquirer, CoarseChannel, Detection

CODE = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)  # length-7 PN
CHIP_RATE = 1.0e6
SOURCE_RATE = 8.0e6  # 8 samples per chip at the source
REPS = 8  # code periods integrated per search
SEARCH = {
    "source_rate": SOURCE_RATE,
    "spc": 2,
    "chip_rate": CHIP_RATE,
    "reps": REPS,
    "cn0_dbhz": 35.0,
    "pfa": 1e-3,
}


def burst(f_hz: float, periods: int, snr_amp: float, seed: int) -> np.ndarray:
    """A noisy DSSS burst at absolute Doppler ``f_hz``, code phase 0."""
    rng = np.random.default_rng(seed)
    chips = np.where(CODE & 1, -1.0, 1.0)
    sps = int(SOURCE_RATE / CHIP_RATE)
    clean = np.tile(np.repeat(chips, sps), periods)
    n = np.arange(clean.size)
    sigma = 1.0 / snr_amp / math.sqrt(2.0)
    noise = sigma * (
        rng.standard_normal(n.size) + 1j * rng.standard_normal(n.size)
    )
    carrier = np.exp(2j * np.pi * f_hz / SOURCE_RATE * n)
    return (clean * carrier + noise).astype(np.complex64)


with Acquirer(CODE, doppler_uncertainty_hz=3.0e5, **SEARCH) as bank:
    centers = bank.centers_hz
    dc = bank.n_channels // 2
    print(
        f"{bank.n_channels} channels, {bank.span_hz / 1e3:.1f} kHz native "
        f"span each, centers {centers[0] / 1e3:+.0f} .. "
        f"{centers[-1] / 1e3:+.0f} kHz"
    )

    f_true = 2.0 * bank.span_hz + 1.0e4  # beyond the center channel's span
    rx = burst(f_true, REPS * 4, snr_amp=0.3, seed=11)
    hit = bank.acquire(rx)

    assert isinstance(hit, Detection)
    print(
        f"true {f_true / 1e3:+.1f} kHz -> channel {hit.channel} "
        f"(center {centers[hit.channel] / 1e3:+.0f} kHz), "
        f"{hit.doppler_hz / 1e3:+.1f} kHz, code phase {hit.code_phase}, "
        f"C/N0 {hit.cn0_dbhz_est:.1f} dB-Hz"
    )
    assert hit.channel != dc, "the center channel cannot see this burst"
    assert abs(hit.doppler_hz - f_true) <= bank.res_hz
    assert hit.code_phase == 0
    nearest = min(
        range(bank.n_channels), key=lambda k: abs(centers[k] - f_true)
    )
    assert hit.channel == nearest

    # The bank is its channels: one CoarseChannel at that center, fed the
    # same samples, reports the same absolute Doppler.
    ch = CoarseChannel(centers[hit.channel], code=CODE, pd=0.9, **SEARCH)
    alone = ch.process(rx, hit.channel)
    assert alone, "the channel on its own found nothing"
    assert abs(alone[0].doppler_hz - hit.doppler_hz) <= bank.res_hz

print("coarse_doppler_bank_demo: OK")
