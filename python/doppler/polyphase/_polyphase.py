"""
doppler.polyphase._polyphase
============================

Kaiser window design formulas for polyphase FIR filter banks.
"""

from __future__ import annotations

import numpy as np

__all__ = ["kaiser_beta", "kaiser_taps", "kaiser_prototype"]


def kaiser_beta(attenuation_db: float) -> float:
    """Return Kaiser window β for the given stopband attenuation (dB)."""
    a = float(attenuation_db)
    if a > 50.0:
        return 0.1102 * (a - 8.7)
    if a >= 21.0:
        return 0.5842 * (a - 21.0) ** 0.4 + 0.07886 * (a - 21.0)
    return 0.0


def kaiser_taps(
    attenuation: float = 60.0,
    passband: float = 0.4,
    stopband: float = 0.6,
) -> int:
    """Return prototype filter length for the given Kaiser spec."""
    return int(
        1 + (attenuation - 8)
        / 2.285
        / (2.0 * np.pi * (stopband - passband))
    )


def kaiser_prototype(
    attenuation: float = 60.0,
    passband: float = 0.4,
    stopband: float = 0.6,
    image_attenuation: float = 80.0,
) -> np.ndarray:
    """Return a Kaiser-windowed sinc prototype filter (float32)."""
    db_per_bit = 6.02
    phases = 1 << int(np.ceil(
        (20.0 * np.log10(passband) + image_attenuation) / db_per_bit
    ))
    halflen = kaiser_taps(attenuation, passband/phases, stopband/phases) // 2
    htaps = 2 * halflen + 1
    m = np.arange(0, htaps) - halflen
    window = np.kaiser(htaps, kaiser_beta(attenuation))
    wc = 2 * np.pi * (passband/phases + (stopband - passband) / (2 * phases))
    h = window * wc / np.pi * np.sinc(wc / np.pi * m)
    taps_per_phase = int(htaps / phases) + 1
    g = np.zeros(phases * taps_per_phase)
    g[:htaps] = h * phases
    polyphase = np.reshape(g.astype(np.float32), (taps_per_phase, phases)).T
    prototype = g.astype(np.float32)
    return prototype, polyphase