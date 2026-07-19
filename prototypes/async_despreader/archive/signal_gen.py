"""Shared async-DSSS signal-generation geometry and helpers.

Extracted from `validate_stress.py` (now `archive/validate_stress.py`,
alongside the historical `despreader.py` reference it was built to
stress-test) so the CURRENT design's validation scripts
(`characterize_snr.py`, `improve_low_snr.py`, `doppler_rate_test.py`,
`pullin_sweep.py`) don't need to import the old pure-Python despreader
reference just to get these constants and helpers. Content is
unchanged from the original -- a pure extraction, not a rewrite.
"""
from __future__ import annotations

import numpy as np

CHIP_RATE = 2.046e6
SF = 1023
SPS = 2
TE = SF * SPS
DATA_RATE = 1800.0
EPOCHS_PER_SYMBOL = (1.0 / DATA_RATE) / (SF / CHIP_RATE)
F0 = 1e-4  # residual carrier, cycles/sample


def code(seed=11):
    return np.random.default_rng(seed).integers(0, 2, SF).astype(np.uint8)


def signal(c, nsym, epochs_per_symbol, phi, f0, snr_db, seed):
    rng = np.random.default_rng(seed)
    csign = np.where(c & 1, -1.0, 1.0)
    tsym = TE * epochs_per_symbol
    n = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx // SPS) % SF
    rx = (data[si] * csign[cph] * np.exp(2j * np.pi * f0 * idx)).astype(
        np.complex128
    )
    if snr_db is not None:
        p = np.sqrt(np.mean(np.abs(rx) ** 2))
        std = np.sqrt(10 ** (-snr_db / 10)) * p
        rx = rx + (
            rng.normal(0, std / np.sqrt(2), n)
            + 1j * rng.normal(0, std / np.sqrt(2), n)
        )
    return rx, data, tsym
