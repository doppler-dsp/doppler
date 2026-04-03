"""
doppler.window — Kaiser window, ENBW, and beta solver.

Functions
---------
kaiser_window(n, beta) → float32 ndarray
    Kaiser window of length *n* with shape parameter *beta*.

kaiser_enbw(w) → float
    Equivalent noise bandwidth of a window in FFT bins.

kaiser_beta_for_enbw(target_enbw, n=1024) → float
    Find the Kaiser beta that produces a given ENBW (in bins).
    Used by the spectrum analyser to hit an exact RBW target:
    beta is the *little knob* (fine), N is the *big knob* (coarse).

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
>>> from doppler.window import kaiser_window, kaiser_enbw, kaiser_beta_for_enbw
>>> w = kaiser_window(4096, 0.0)
>>> round(kaiser_enbw(w), 3)  # rectangular → 1.0 bin
1.0
>>> w6 = kaiser_window(4096, 6.0)
>>> 1.44 < kaiser_enbw(w6) < 1.50
True
>>> beta = kaiser_beta_for_enbw(1.47)
>>> abs(beta - 6.0) < 0.05
True
"""

from ._window import kaiser_enbw, kaiser_window


def kaiser_beta_for_enbw(target_enbw: float, n: int = 1024) -> float:
    """Find the Kaiser beta whose ENBW equals *target_enbw* (in bins).

    Uses bisection on the actual window so the result is exact for the
    given *n*.  Converges in ~50 iterations; trivially fast at init time.

    Parameters
    ----------
    target_enbw : float
        Desired ENBW in FFT bins.  Must be >= 1.0 (rectangular window).
        Values > ~2.1 require very large beta and are unusual in practice.
    n : int
        Window length used for the bisection.  Larger *n* gives a more
        accurate result; 1024 is sufficient for any practical FFT size.

    Returns
    -------
    float
        Kaiser beta >= 0 that achieves *target_enbw*.
    """
    if target_enbw <= 1.0:
        return 0.0
    lo, hi = 0.0, 60.0  # beta=60 gives ENBW >> 2.1
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if kaiser_enbw(kaiser_window(n, mid)) < target_enbw:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2.0


__all__ = ["kaiser_window", "kaiser_enbw", "kaiser_beta_for_enbw"]
