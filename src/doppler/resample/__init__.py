"""resample — Polyphase resampler and halfband decimator types."""

import math
import os as _os
import sys as _sys

import numpy as np

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .resample import HalfbandDecimator, HalfbandDecimatorDp, HalfbandDecimatorR2C, Resampler, CIC, ciccompmf, kaiser_beta, kaiser_num_taps, RateConverter  # noqa: E402
from .cic_design import (  # noqa: E402
    cic_precision_bits,
    cic_alias_rejection,
    cic_passband_droop,
    cic_min_order,
    cic_design,
)


def kaiser_beta(atten: float) -> float:
    """Kaiser window beta from stopband attenuation in dB."""
    if atten > 50.0:
        return 0.1102 * (atten - 8.7)
    if atten >= 21.0:
        return 0.5842 * (atten - 21.0) ** 0.4 + 0.07886 * (atten - 21.0)
    return 0.0


def kaiser_num_taps(num_phases: int, atten: float, pb: float, sb: float) -> int:
    """Taps-per-phase from Kaiser design spec."""
    pb_ph = pb / num_phases
    sb_ph = sb / num_phases
    proto = int(1 + (atten - 8.0) / 2.285 / (2.0 * math.pi * (sb_ph - pb_ph)))
    halflen = proto // 2
    htaps = 2 * halflen + 1
    return htaps // num_phases + 1


def _build_bank(
    num_phases: int,
    num_taps: int,
    atten: float,
    pb: float,
    sb: float,
) -> np.ndarray:
    """Return float32 array of shape ``(num_phases, num_taps)``."""
    beta = kaiser_beta(atten)
    pb_ph = pb / num_phases
    sb_ph = sb / num_phases
    wc = 2.0 * math.pi * (pb_ph + (sb_ph - pb_ph) * 0.5)
    proto = num_phases * num_taps
    if proto % 2 == 0:
        proto += 1
    halflen = proto // 2
    i0_beta = float(np.i0(beta))
    g = np.zeros(proto, dtype=np.float64)
    for i in range(proto):
        m = float(i) - halflen
        mid = float(proto - 1) * 0.5
        u = 2.0 * (float(i) - mid) / float(proto - 1)
        w = float(np.i0(beta * math.sqrt(max(0.0, 1.0 - u * u)))) / i0_beta
        s = 1.0 if m == 0.0 else math.sin(wc * m) / (wc * m)
        g[i] = w * wc / math.pi * s * num_phases
    bank = np.zeros((num_phases, num_taps), dtype=np.float32)
    for p in range(num_phases):
        for t in range(num_taps):
            idx = t * num_phases + p
            if idx < proto:
                bank[p, t] = float(g[idx])
    return bank


def _halfband_bank(
    atten: float = 60.0,
    pb: float = 0.4,
    sb: float = 0.6,
) -> np.ndarray:
    """Return a ``(2, N)`` float32 halfband polyphase bank."""
    N = kaiser_num_taps(2, atten, pb, sb)
    beta = kaiser_beta(atten)
    pb_ph = pb / 2.0
    sb_ph = sb / 2.0
    wc = 2.0 * math.pi * (pb_ph + (sb_ph - pb_ph) * 0.5)
    proto = 2 * (N - 1) + 1
    halflen = proto // 2
    i0_beta = float(np.i0(beta))
    g = np.zeros(proto, dtype=np.float64)
    for i in range(proto):
        m = float(i) - halflen
        mid = float(proto - 1) * 0.5
        u = 2.0 * (float(i) - mid) / float(proto - 1)
        w = float(np.i0(beta * math.sqrt(max(0.0, 1.0 - u * u)))) / i0_beta
        s = 1.0 if m == 0.0 else math.sin(wc * m) / (wc * m)
        g[i] = w * wc / math.pi * s * 2.0
    bank = np.zeros((2, N), dtype=np.float32)
    for p in range(2):
        for t in range(N):
            idx = t * 2 + p
            if idx < proto:
                bank[p, t] = float(g[idx])
    return bank


def _num_phases_for_rejection(rejection: float) -> int:
    """Minimum power-of-two num_phases to achieve ``rejection`` dB."""
    min_phases = 10.0 ** (rejection / 20.0)
    p = 64
    while p < min_phases:
        p <<= 1
    return min(p, 65536)


# underscore alias kept for test/internal compatibility
_kaiser_num_taps = kaiser_num_taps


def rate_convert(x, rate, rc=None):
    """Convert samples to a new sample rate.

    Parameters
    ----------
    x : array_like, complex64
        Input samples.
    rate : float
        Output-to-input sample rate ratio.
    rc : RateConverter, optional
        Existing converter to reuse; a new one is created if None.

    Returns
    -------
    out : ndarray, complex64
        Converted samples.
    rc : RateConverter
        The converter used (pass back in to maintain state across calls).

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.resample import rate_convert
    >>> x = np.ones(256, dtype=np.complex64)
    >>> y, rc = rate_convert(x, 0.5)
    >>> len(y) == 128
    True
    """
    if rc is None:
        rc = RateConverter(rate)
    return rc.execute(x), rc


__all__ = ["HalfbandDecimator", "HalfbandDecimatorDp", "HalfbandDecimatorR2C", "Resampler", "CIC", "ciccompmf", "kaiser_beta", "kaiser_num_taps", "RateConverter"]
