"""
doppler.window — Kaiser window and ENBW.

Functions
---------
kaiser_window(n, beta) → float32 ndarray
    Kaiser window of length *n* with shape parameter *beta*.

kaiser_enbw(w) → float
    Equivalent noise bandwidth of a window in FFT bins.

Notes
-----
*beta* follows the NumPy / SciPy convention: it is the direct argument
to the modified Bessel function I₀, **not** scaled by π.  The Harris
(1978) table uses α = β/π.

Typical values:

+------+----------------+-------------+------------------------+
| β    | Side-lobe (dB) | ENBW (bins) | Use case               |
+------+----------------+-------------+------------------------+
| 0    | −13            | 1.00        | Rectangular            |
| 5    | −57            | 1.36        | General-purpose        |
| 6    | −69            | 1.47        | Spectrum analyser      |
| 8.6  | −90            | 1.72        | High dynamic range     |
| 13.3 | −120           | 2.11        | Very high DR           |
+------+----------------+-------------+------------------------+

Examples
--------
>>> from doppler.window import kaiser_window, kaiser_enbw
>>> w = kaiser_window(4096, 0.0)
>>> round(kaiser_enbw(w), 3)  # rectangular → 1.0 bin
1.0
>>> w6 = kaiser_window(4096, 6.0)
>>> 1.44 < kaiser_enbw(w6) < 1.50
True
"""

from ._window import kaiser_enbw, kaiser_window

__all__ = ["kaiser_window", "kaiser_enbw"]
