"""Shared utilities used across the doppler modules: the numeric helpers, and
`fromto`, the path from a calling script to where it writes.

Examples
--------
>>> from doppler.util import square_clip
>>> square_clip(2 + 0j, 1.0)
(1+0j)

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
