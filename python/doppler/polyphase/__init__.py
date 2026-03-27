"""
doppler.polyphase
=================

Kaiser window design formulas for polyphase FIR filter banks,
and DPMFS polynomial coefficient fitting.
"""

from ._polyphase import (
    kaiser_beta,
    kaiser_taps,
    kaiser_prototype,
    plot_group_delay,
)
from ._dpmfs import DPMFSCoeffs, fit_dpmfs
from .matlab_optimization import optimize_dpmfs, optimize_pbf

__all__ = [
    "kaiser_beta",
    "kaiser_taps",
    "kaiser_prototype",
    "plot_group_delay",
    "DPMFSCoeffs",
    "fit_dpmfs",
    "optimize_dpmfs",
    "optimize_pbf",
]
