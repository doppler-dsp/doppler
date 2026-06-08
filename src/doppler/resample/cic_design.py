"""CIC filter design aid — precision, alias rejection, and order selection.

All frequency arguments are normalised to the *output* sample rate
(0 < f < 0.5).  The CIC passband is 0..f_p; aliasing folds energy from
the input Nyquist zones back into 0..0.5 at the output.

The CIC frequency response evaluated at input-normalised frequency f_in is:

    H(f_in) = |sin(π R M f_in) / (R M sin(π f_in))|^N

Passband edge (input-normalised): f_in = f_p / R
Worst k=R-1 alias (input-normalised): f_in = (1 − f_p) / R

The alias is evaluated at the complex-conjugate alias: a signal at output
frequency f_p is contaminated by the input band around digital frequency
1 − f_p/R (input-normalised), which folds to −f_p at the output.  After
accounting for the CIC response, its attenuation is H((1−f_p)/R).

Examples
--------
>>> cic_precision_bits(32, 4)
42
>>> round(cic_alias_rejection(32, 4, 1, 0.1), 1)
76.9
>>> round(cic_passband_droop(32, 4, 1, 0.1), 3)
0.573
"""

from __future__ import annotations

import math
from typing import Optional

__all__ = [
    "cic_precision_bits",
    "cic_alias_rejection",
    "cic_passband_droop",
    "cic_min_order",
    "cic_design",
]


# ── internal helpers ──────────────────────────────────────────────────────────


def _response(R: int, N: int, M: int, f_in: float) -> float:
    """CIC magnitude at input-normalised frequency f_in.

    H(f_in) = |sin(π R M f_in) / (R M sin(π f_in))|^N

    Parameters
    ----------
    R : int
        Decimation ratio.
    N : int
        Number of stages.
    M : int
        Differential delay (1 or 2).
    f_in : float
        Input-normalised frequency in (0, 0.5).

    Returns
    -------
    float
        Linear magnitude ≥ 0 (not in dB).
    """
    pi = math.pi
    eps = 1e-12
    f_in = max(eps, f_in)
    num = abs(math.sin(pi * R * M * f_in))
    den = R * M * abs(math.sin(pi * f_in))
    h = (num / den) if den > eps else 0.0
    return h**N


# ── public API ────────────────────────────────────────────────────────────────


def cic_precision_bits(R: int, N: int, M: int = 1) -> int:
    """Number of valid signal bits in the uint64 CIC pipeline.

    The C implementation scales ±1.0 input to a power-of-2 integer
    (input_scale = 2^k) such that input_scale × (R·M)^N < 2^63.  k is
    the number of bits carrying signal information in the 64-bit
    accumulator; bits 0..k−1 are signal, bits k..63 are overflow headroom
    that the modular-arithmetic CIC property keeps invisible in the output.

    The formula is the same as in ``cic_core.c``::

        k = int(63.0 − N·log2(R·M) − ε)   ε = 1e-9

    The ε prevents 2^63 overflow when log2(gain) is exactly integer
    (e.g. R=32 is a power of 2, so 63 − N·log2(32) could be exactly
    integer, and truncating it would give scale × gain = 2^63 which
    overflows int64_t on the output conversion).

    Parameters
    ----------
    R : int
        Decimation ratio (≥ 1).
    N : int
        Number of stages (1–6).
    M : int, optional
        Differential delay (1 or 2).  Default 1.

    Returns
    -------
    int
        Valid signal bits (≥ 0).

    Examples
    --------
    >>> cic_precision_bits(32, 4)
    42
    >>> cic_precision_bits(8, 3)
    53
    >>> cic_precision_bits(256, 6)
    14
    """
    log2_gain = N * math.log2(R * M)
    k = int(63.0 - log2_gain - 1e-9)
    return max(0, k)


def cic_alias_rejection(R: int, N: int, M: int, f_p: float) -> float:
    """Alias rejection at output passband edge f_p (dB, positive = attenuation).

    For a CIC decimator with ratio R, the worst alias at output frequency
    f_p comes from the complex-conjugate alias zone: input digital
    frequency 1 − f_p/R (input-normalised), which folds to output −f_p.
    The k=1 alias is always worst because higher alias bands fall on deeper
    CIC nulls.

    The alias input-normalised frequency is (1 − f_p) / R; the attenuation
    is −20·log10(H((1 − f_p)/R)) where H is the normalised CIC magnitude.

    Parameters
    ----------
    R : int
        Decimation ratio.
    N : int
        Number of stages.
    M : int
        Differential delay (1 or 2).
    f_p : float
        Passband edge, normalised to the output sample rate (0 < f_p < 0.5).

    Returns
    -------
    float
        Alias attenuation in dB (positive means the alias is suppressed).
        Returns ``inf`` when the alias falls exactly on a CIC null.

    Examples
    --------
    >>> round(cic_alias_rejection(32, 4, 1, 0.1), 1)
    76.9
    >>> round(cic_alias_rejection(32, 4, 1, 0.4), 1)
    23.7
    """
    f_alias = (1.0 - f_p) / R
    h = _response(R, N, M, f_alias)
    if h <= 0.0:
        return float("inf")
    return -20.0 * math.log10(h)


def cic_passband_droop(R: int, N: int, M: int, f_p: float) -> float:
    """Passband droop (dB loss) at output passband edge f_p.

    The CIC response rolls off within the passband.  The loss at f_p
    relative to DC is −20·log10(H(f_p/R)) where f_p/R is the
    input-normalised frequency corresponding to the output passband edge,
    and H is the normalised CIC magnitude.

    Parameters
    ----------
    R : int
        Decimation ratio.
    N : int
        Number of stages.
    M : int
        Differential delay (1 or 2).
    f_p : float
        Passband edge, normalised to the output sample rate (0 < f_p < 0.5).

    Returns
    -------
    float
        Passband droop in dB (loss, positive value).

    Examples
    --------
    >>> round(cic_passband_droop(32, 4, 1, 0.1), 3)
    0.573
    >>> round(cic_passband_droop(32, 4, 1, 0.4), 3)
    9.671
    """
    h = _response(R, N, M, f_p / R)
    if h <= 0.0:
        return float("inf")
    return -20.0 * math.log10(h)


def cic_min_order(
    R: int,
    f_p: float,
    rejection_db: float,
    M: Optional[int] = None,
) -> Optional[tuple[int, int]]:
    """Minimum (N, M) meeting alias rejection requirement.

    Searches N from 1 to 16 and M in {1, 2} (or just the supplied M) and
    returns the first (N, M) pair whose alias rejection ≥ rejection_db at
    passband edge f_p.  M=1 is tried before M=2 so the result favours
    lower delay.

    Parameters
    ----------
    R : int
        Decimation ratio.
    f_p : float
        Passband edge normalised to the output sample rate (0 < f_p < 0.5).
    rejection_db : float
        Required alias attenuation in dB (positive).
    M : int or None, optional
        If given, only that M value is searched.  If None (default), both
        M=1 and M=2 are searched, preferring M=1.

    Returns
    -------
    tuple[int, int] or None
        (N, M) for the minimum-order solution, or None if no solution
        exists within N ≤ 16.

    Examples
    --------
    >>> cic_min_order(32, 0.1, 60)
    (4, 1)
    >>> cic_min_order(32, 0.4, 60)
    (11, 1)
    >>> cic_min_order(32, 0.4, 60, M=2)
    (4, 2)
    """
    m_candidates = [M] if M is not None else [1, 2]
    for m in m_candidates:
        for n in range(1, 17):
            if cic_alias_rejection(R, n, m, f_p) >= rejection_db:
                return (n, m)
    return None


def cic_design(
    R: int,
    f_p: float,
    *,
    M: Optional[int] = None,
    max_N: int = 8,
) -> None:
    """Print a design-aid table for a CIC decimator.

    Shows, for each (N, M) combination up to max_N, the available
    precision bits, alias rejection, and passband droop.  Rows where
    alias rejection ≥ 60 dB are marked with ``*``.

    Parameters
    ----------
    R : int
        Decimation ratio.
    f_p : float
        Passband edge normalised to the output sample rate (0 < f_p < 0.5).
    M : int or None, optional
        If given, only that M value is shown.  Default: both M=1 and M=2.
    max_N : int, optional
        Maximum number of stages to tabulate.  Default 8.

    Examples
    --------
    >>> cic_design(32, 0.1, max_N=5)
    CIC decimator design aid  R=32  f_p=0.100
    <BLANKLINE>
      N  M   bits   alias_rej_dB   droop_dB
    ---  -  -----  -------------  ---------
      1  1      57           19.2      0.143
      2  1      52           38.4      0.286
      3  1      47           57.7      0.430
      4  1   *  42           76.9      0.573
      5  1   *  37           96.1      0.716
      1  2      56           19.7      0.579
      2  2      50           39.3      1.158
      3  2      44           59.0      1.737
      4  2   *  38           78.6      2.316
      5  2   *  32           98.3      2.895
    """
    m_list = [M] if M is not None else [1, 2]
    target_db = 60.0

    print(f"CIC decimator design aid  R={R}  f_p={f_p:.3f}")
    print()
    print("  N  M   bits   alias_rej_dB   droop_dB")
    print("---  -  -----  -------------  ---------")

    for m in m_list:
        for n in range(1, max_N + 1):
            bits = cic_precision_bits(R, n, m)
            rej = cic_alias_rejection(R, n, m, f_p)
            drp = cic_passband_droop(R, n, m, f_p)
            flag = " *" if rej >= target_db else "  "
            print(f"{n:3d}  {m}  {flag} {bits:3d}  {rej:13.1f}  {drp:9.3f}")
