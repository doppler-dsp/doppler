"""Feedforward estimation: a block of samples in, one estimate out, with no loop. Symbol timing (SymbolTimingEstimator, and MatchedSymbolTimingEstimator with its own matched filter) and carrier phase, frequency and frequency rate (PolynomialPhaseEstimator).

An estimator here takes a fixed number of samples whatever the unknown
started at, and decides nothing: it returns the estimate and a figure for
how far to trust it. What tracks from there is in `doppler.track`.

Examples
--------
>>> import numpy as np
>>> from doppler.estimation import SymbolTimingEstimator
>>> n = np.arange(4 * 256)
>>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * (n / 4 - 0.25)))
>>> e = SymbolTimingEstimator(sps=4.0).estimate(x.astype(np.complex64))
>>> round(e.tau, 3)
0.25

"""

# estimation/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .estimation import (  # noqa: E402
    MatchedSymbolTimingEstimator,
    PolynomialPhaseEstimate,
    PolynomialPhaseEstimator,
    SymbolTimingEstimator,
    TimingEstimate,
)

__all__ = [
    "SymbolTimingEstimator",
    "PolynomialPhaseEstimator",
    "MatchedSymbolTimingEstimator",
    "TimingEstimate",
    "PolynomialPhaseEstimate",
]
