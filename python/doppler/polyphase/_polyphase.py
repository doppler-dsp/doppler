"""
doppler.polyphase._polyphase
============================

Polyphase FIR filter-bank designer.

A polyphase resampler needs one long prototype lowpass FIR that is then
sliced into ``num_phases`` sub-filters of ``num_taps`` taps each.  The
caller selects a sub-filter at run time by shifting the NCO phase into the
range ``[0, num_phases)``.

Design methods
--------------
``"kaiser"``
    Kaiser-windowed sinc.  Pure-NumPy, no extra dependency.
    Single cutoff at the midpoint of the transition band.  β is
    derived from *attenuation_db* using the standard formula.

``"firls"``
    Weighted least-squares (scipy.signal.firls).  Requires SciPy.
    Takes the full band / amplitude specification directly; gives
    equiripple-like behaviour across all bands.

Prototype length
----------------
The prototype has exactly ``num_phases * num_taps`` samples so it folds
cleanly into the ``(num_phases, num_taps)`` bank matrix.  When the
least-squares method requires an odd length the prototype is one sample
shorter and the last sample is zero-padded before reshaping.
"""

from __future__ import annotations

import textwrap
from datetime import datetime, timezone
from pathlib import Path
from typing import Sequence

import numpy as np

__all__ = ["design_bank", "plot_response", "to_c_header", "to_npy"]

# ---------------------------------------------------------------------------
# Kaiser β formula (Harris 1978)
# ---------------------------------------------------------------------------

def _kaiser_beta(attenuation_db: float) -> float:
    """Return Kaiser window β for the given stopband attenuation (dB)."""
    a = float(attenuation_db)
    if a > 50.0:
        return 0.1102 * (a - 8.7)
    if a >= 21.0:
        return 0.5842 * (a - 21.0) ** 0.4 + 0.07886 * (a - 21.0)
    return 0.0


# ---------------------------------------------------------------------------
# Private helpers
# ---------------------------------------------------------------------------

def _kaiser_prototype(
    N: int,
    fc: float,
    beta: float,
) -> np.ndarray:
    """Kaiser-windowed sinc lowpass prototype of length N.

    Parameters
    ----------
    N:     Filter length.
    fc:    Cutoff frequency, normalized to [0, 1] where 1 = Nyquist.
    beta:  Kaiser window shape parameter.

    Returns
    -------
    float32 array of length N.
    """
    n = np.arange(N, dtype=np.float64)
    mid = (N - 1) / 2.0
    h = fc * np.sinc(fc * (n - mid))
    h *= np.kaiser(N, beta)
    return h.astype(np.float32)


def _firls_prototype(
    N: int,
    bands: Sequence[float],
    amps: Sequence[float],
) -> np.ndarray:
    """Least-squares lowpass prototype of length N (zero-padded if needed).

    scipy.signal.firls requires an odd number of taps (Type I).  If N is
    even the prototype is designed with N-1 taps and a trailing zero is
    appended before the caller reshapes.

    Parameters
    ----------
    N:      Target length (num_phases * num_taps).
    bands:  Band-edge frequencies normalised to [0, 1] (1 = Nyquist).
    amps:   Desired amplitude at each band edge.

    Returns
    -------
    float32 array of length N.
    """
    try:
        from scipy.signal import firls
    except ImportError as exc:
        raise ImportError(
            "method='firls' requires SciPy. "
            "Install it with: uv add scipy"
        ) from exc

    N_ls = N if N % 2 == 1 else N - 1
    h = firls(N_ls, list(bands), list(amps))
    if len(h) < N:
        h = np.concatenate([h, np.zeros(N - len(h))])
    return h.astype(np.float32)


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def design_bank(
    num_phases: int = 4096,
    num_taps: int = 19,
    bands: Sequence[float] = (0.0, 0.4, 0.6, 1.0),
    amps: Sequence[float] = (1.0, 1.0, 0.0, 0.0),
    attenuation_db: float = 60.0,
    method: str = "kaiser",
) -> np.ndarray:
    """Design a polyphase FIR filter bank.

    Parameters
    ----------
    num_phases:
        Number of polyphase branches (power of two recommended; default
        4096 gives <80 dBc spectral images with nearest-neighbour
        phase selection).
    num_taps:
        Number of taps per branch.  Total prototype length is
        ``num_phases * num_taps``.
    bands:
        Band-edge frequencies in normalised units [0, 1] where 1 =
        Nyquist.  Must have an even number of entries that alternate
        between the start and end of each band.
    amps:
        Desired amplitude at each band edge.  Same length as *bands*.
    attenuation_db:
        Target stopband attenuation in dB.  Used to derive the Kaiser
        β parameter when ``method="kaiser"``.
    method:
        ``"kaiser"`` (default, pure NumPy) or ``"firls"`` (SciPy
        least-squares, uses the full band specification).

    Returns
    -------
    ndarray, shape ``(num_phases, num_taps)``, dtype float32.
        Row ``k`` is the sub-filter for phase index ``k``.  Select the
        row at run time via ``phase >> (32 - log2(num_phases))`` from a
        32-bit NCO accumulator.

    Examples
    --------
    >>> bank = design_bank()
    >>> bank.shape
    (4096, 19)

    >>> bank_ls = design_bank(method="firls")
    >>> bank_ls.shape
    (4096, 19)
    """
    bands = list(bands)
    amps = list(amps)
    N = num_phases * num_taps

    if method == "kaiser":
        # Cutoff at the midpoint of the first transition band.
        fc = (bands[1] + bands[2]) / 2.0
        beta = _kaiser_beta(attenuation_db)
        h = _kaiser_prototype(N, fc, beta)
    elif method == "firls":
        h = _firls_prototype(N, bands, amps)
    else:
        raise ValueError(
            f"Unknown method {method!r}. Choose 'kaiser' or 'firls'."
        )

    return h.reshape(num_phases, num_taps)


def plot_response(
    bank: np.ndarray,
    *,
    attenuation_db: float = 0.0,
    bands: Sequence[float] | None = None,
    amps: Sequence[float] | None = None,
    title: str = "Polyphase bank — prototype frequency response",
    show: bool = True,
) -> "plt.Figure":
    """Plot the frequency response of the prototype filter.

    The prototype is the full flattened bank (``bank.ravel()``).  The
    magnitude response is plotted in dB on a linear frequency axis
    normalised to [0, 1] (1 = Nyquist).

    Parameters
    ----------
    bank:
        Filter bank array of shape ``(num_phases, num_taps)``.
    attenuation_db:
        If non-zero, draws a horizontal reference line at -attenuation_db.
    bands / amps:
        If given, overlays the design band specification as a shaded
        ideal response for visual comparison.
    title:
        Plot title.
    show:
        Call ``plt.show()`` after drawing (default True).

    Returns
    -------
    matplotlib.figure.Figure
    """
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise ImportError(
            "plot_response() requires matplotlib. "
            "Install it with: uv add matplotlib"
        ) from exc

    h = bank.ravel().astype(np.float64)
    N = len(h)
    nfft = max(8 * N, 65536)

    H = np.fft.rfft(h, n=nfft)
    # rfftfreq is 0..0.5; normalise to 0..1 (1 = Nyquist)
    freqs = np.fft.rfftfreq(nfft) * 2
    mag_db = 20.0 * np.log10(np.abs(H) + 1e-300)

    fig, ax = plt.subplots(figsize=(9, 4))

    # Ideal response overlay
    if bands is not None and amps is not None:
        b, a = list(bands), list(amps)
        ideal = np.interp(freqs, b, a)
        ax.fill_between(
            freqs, 20 * np.log10(ideal + 1e-300), -120,
            alpha=0.08, color="steelblue", label="Ideal",
        )

    ax.plot(freqs, mag_db, linewidth=0.8, color="steelblue",
            label=f"Prototype ({N} taps)")

    if attenuation_db > 0:
        ax.axhline(
            -attenuation_db, color="tomato", linewidth=0.8,
            linestyle="--", label=f"−{attenuation_db:.0f} dB",
        )

    ax.set_xlim(0, 1)
    ax.set_ylim(-120, 5)
    ax.set_xlabel("Normalised frequency  [0 = DC, 1 = Nyquist]")
    ax.set_ylabel("Magnitude (dB)")
    ax.set_title(title)
    ax.legend(fontsize=8)
    ax.grid(True, linewidth=0.4, alpha=0.5)
    fig.tight_layout()

    if show:
        plt.show()

    return fig


def to_c_header(
    bank: np.ndarray,
    name: str = "dp_polyphase_bank",
    path: str | Path | None = None,
    *,
    method: str = "",
    attenuation_db: float = 0.0,
) -> str:
    """Render the filter bank as a C99 header string.

    Parameters
    ----------
    bank:
        Array of shape ``(num_phases, num_taps)`` as returned by
        :func:`design_bank`.
    name:
        Base identifier used for the C array and ``#define`` names.
    path:
        If given, the header is written to this file path in addition
        to being returned as a string.
    method:
        Design method string embedded in the auto-generated comment.
    attenuation_db:
        Attenuation embedded in the auto-generated comment.

    Returns
    -------
    str — the full header text.
    """
    num_phases, num_taps = bank.shape
    guard = name.upper().replace(" ", "_") + "_H"
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    rows = []
    for row in bank:
        coeffs = ", ".join(f"{v:.8e}f" for v in row)
        rows.append(f"    {{ {coeffs} }}")
    array_body = ",\n".join(rows)

    header = textwrap.dedent(f"""\
        /* Auto-generated polyphase filter bank — do not edit by hand.
         * Generated: {ts}
         * Phases:    {num_phases}
         * Taps/phase:{num_taps}
         * Method:    {method or 'unknown'}
         * Atten:     {attenuation_db:.1f} dB
         *
         * Usage:
         *   uint32_t phase = nco_u32 >> (32 - {num_phases.bit_length() - 1});
         *   const float *h = {name}[phase];
         *   // MAC h[0..{num_taps-1}] against delay line
         */
        #pragma once
        #include <stddef.h>

        #define {name.upper()}_NUM_PHASES {num_phases}
        #define {name.upper()}_NUM_TAPS   {num_taps}

        static const float {name}[{num_phases}][{num_taps}] = {{
        {array_body}
        }};
        """)

    if path is not None:
        Path(path).write_text(header, encoding="utf-8")

    return header


def to_npy(bank: np.ndarray, path: str | Path) -> None:
    """Save the filter bank as a NumPy .npy file.

    Parameters
    ----------
    bank:  Array of shape ``(num_phases, num_taps)``.
    path:  Destination file path (conventionally ``.npy``).
    """
    np.save(str(path), bank)
