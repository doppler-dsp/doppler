"""Shared utilities used across the doppler modules: the numeric helpers, and
`fromto`, the path from a calling script to where it writes.

`square_clip` is the per-component hard limiter the AGC and other
saturating paths use; `saturate` is the total range guard a loop puts on
its own state, NaN included; `ema_step` and `ema_alpha_decim` are the
library's one exponential moving average and the coefficient that advances
it a whole chunk at a time, with `complement_power` the kernel behind that
coefficient; `sinc` and `mean_sinc` are the straddle loss of a correlator
or DFT bin; and `simpson_weights`, `midpoint_nodes` and `gauss_hermite`
are quadrature rules that fill arrays in place, so the same rule runs in C
and Python with no callback across the boundary.

Examples
--------
>>> from doppler.util import square_clip
>>> square_clip(2 + 0j, 1.0)
(1+0j)

Where a NaN lands is a parameter, because which end is safe is domain
knowledge: an unknown level reads loud, an unknown lock statistic reads
unlocked.

>>> from doppler.util import saturate, ema_step
>>> saturate(float("nan"), 0.0, 1.0, 1.0), saturate(float("nan"), 0.0, 1.0, 0.0)
(1.0, 0.0)
>>> ema_step(1.0, 7.0, 1.0), ema_step(1.0, 7.0, 0.0)   # alpha 1 passes, 0 freezes
(7.0, 1.0)

"""

# util/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .paths import fromto  # noqa: E402
from .util import (  # noqa: E402
    complement_power,
    ema_alpha_decim,
    ema_step,
    gauss_hermite,
    mean_sinc,
    midpoint_nodes,
    next_pow_two,
    saturate,
    simpson_weights,
    sinc,
    square_clip,
)

__all__ = [
    "square_clip",
    "saturate",
    "ema_step",
    "ema_alpha_decim",
    "next_pow_two",
    "complement_power",
    "sinc",
    "mean_sinc",
    "simpson_weights",
    "midpoint_nodes",
    "gauss_hermite",
    "fromto",
]
