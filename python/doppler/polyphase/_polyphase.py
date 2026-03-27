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

import dataclasses
import textwrap
from datetime import datetime, timezone
from pathlib import Path
from typing import Sequence

import numpy as np

__all__ = ["PolyphaseBank", "design_bank", "plot_response",
           "to_c_header", "to_npy"]


# ---------------------------------------------------------------------------
# Result type
# ---------------------------------------------------------------------------

@dataclasses.dataclass
class PolyphaseBank:
    """Filter bank returned by :func:`design_bank`.

    Carries the coefficient array together with the design parameters
    used to create it, so downstream functions (:func:`plot_response`,
    :func:`to_c_header`, :func:`to_npy`) need no extra arguments.

    Attributes
    ----------
    bank:           ``(num_phases, num_taps)`` float32 array.
    bands:          Band-edge frequencies used during design.
    amps:           Amplitude at each band edge.
    attenuation_db: Stopband attenuation target (dB).
    method:         Design method (``"kaiser"`` or ``"firls"``).
    """

    bank: np.ndarray
    bands: tuple[float, ...]
    amps: tuple[float, ...]
    attenuation_db: float
    method: str

    # ------------------------------------------------------------------
    # Proxy key ndarray properties so callers can treat this like an
    # array for shape-checking, indexing, etc.
    # ------------------------------------------------------------------

    @property
    def shape(self) -> tuple[int, int]:
        return self.bank.shape  # type: ignore[return-value]

    @property
    def dtype(self) -> np.dtype:
        return self.bank.dtype

    def __array__(self, dtype=None, copy=None) -> np.ndarray:
        arr = self.bank if dtype is None else self.bank.astype(dtype)
        if copy:
            return arr.copy()
        return arr

    def __getitem__(self, key):
        return self.bank[key]

    def ravel(self) -> np.ndarray:
        return self.bank.ravel()


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

def _kaiser_prototype(N: int, fc: float, beta: float) -> np.ndarray:
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
) -> PolyphaseBank:
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
        Nyquist at the baseband rate (input rate for interpolation,
        output rate for decimation).
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
    PolyphaseBank
        Carries the ``(num_phases, num_taps)`` float32 coefficient
        array together with all design parameters.  Pass directly to
        :func:`plot_response`, :func:`to_c_header`, or :func:`to_npy`.

    Examples
    --------
    >>> pb = design_bank()
    >>> pb.shape
    (4096, 19)
    >>> pb.plot_response()          # uses stored design parameters
    >>> to_c_header(pb, path="bank.h")
    """
    bands = tuple(bands)
    amps = tuple(amps)
    N = num_phases * num_taps

    if method == "kaiser":
        # Cutoff at the midpoint of the transition band, expressed in
        # prototype-rate normalised units (÷ num_phases converts from
        # baseband-normalised to prototype-rate-normalised).
        fc = (bands[1] + bands[2]) / 2.0 / num_phases
        beta = _kaiser_beta(attenuation_db)
        h = _kaiser_prototype(N, fc, beta)
    elif method == "firls":
        # Scale band edges to prototype-rate normalised units.
        bands_scaled = tuple(f / num_phases for f in bands)
        h = _firls_prototype(N, bands_scaled, amps)
    else:
        raise ValueError(
            f"Unknown method {method!r}. Choose 'kaiser' or 'firls'."
        )

    return PolyphaseBank(
        bank=h.reshape(num_phases, num_taps),
        bands=bands,
        amps=amps,
        attenuation_db=attenuation_db,
        method=method,
    )


def plot_response(
    pb: PolyphaseBank,
    *,
    title: str = "Polyphase bank — prototype frequency response",
    show: bool = True,
) -> "plt.Figure":
    """Plot the frequency response of the prototype filter.

    Design parameters (bands, attenuation) are read from *pb* — no
    need to pass them again.

    Parameters
    ----------
    pb:
        :class:`PolyphaseBank` returned by :func:`design_bank`.
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

    num_phases, num_taps = pb.shape
    h = pb.ravel().astype(np.float64)
    N = len(h)
    nfft = max(8 * N, 65536)

    H = np.fft.rfft(h, n=nfft)
    # rfftfreq is 0..0.5 of the prototype rate.
    # × 2 → 0..1 normalised to prototype Nyquist.
    # ÷ num_phases → 0..1 normalised to baseband Nyquist
    # (input rate for interpolation, output rate for decimation).
    # rfftfreq is 0..0.5; × 2 → 0..1 normalised to prototype Nyquist.
    freqs = np.fft.rfftfreq(nfft) * 2
    mag_db = 20.0 * np.log10(np.abs(H) + 1e-300)

    fig, ax = plt.subplots(figsize=(9, 4))

    # Ideal response overlay from stored band spec
    b = list(pb.bands)
    a = list(pb.amps)
    # Scale band edges to the same baseband-normalised axis
    b_scaled = [f / num_phases for f in b]
    ideal = np.interp(freqs, b_scaled, a)
    ax.fill_between(
        freqs, 20 * np.log10(ideal + 1e-300), -120,
        alpha=0.08, color="steelblue", label="Ideal",
    )

    ax.plot(freqs, mag_db, linewidth=0.8, color="steelblue",
            label=f"Prototype ({N} taps)")

    if pb.attenuation_db > 0:
        ax.axhline(
            -pb.attenuation_db, color="tomato", linewidth=0.8,
            linestyle="--",
            label=f"−{pb.attenuation_db:.0f} dB",
        )

    ax.set_xlim(0, 2.0 / num_phases)
    ax.set_ylim(-120, 5)
    ax.set_xlabel(
        "Normalised frequency  "
        "[0 = DC, 1 = Nyquist at prototype rate = "
        f"{num_phases}× baseband rate]"
    )
    ax.set_ylabel("Magnitude (dB)")
    ax.set_title(title)
    ax.legend(fontsize=8)
    ax.grid(True, linewidth=0.4, alpha=0.5)
    fig.tight_layout()

    if show:
        plt.show()

    return fig


def to_c_header(
    pb: PolyphaseBank,
    name: str = "dp_polyphase_bank",
    path: str | Path | None = None,
) -> str:
    """Render the filter bank as a C99 header string.

    Design metadata (method, attenuation) is read from *pb*.

    Parameters
    ----------
    pb:    :class:`PolyphaseBank` returned by :func:`design_bank`.
    name:  Base identifier for the C array and ``#define`` names.
    path:  If given, the header is also written to this file.

    Returns
    -------
    str — the full header text.
    """
    num_phases, num_taps = pb.shape
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    rows = []
    for row in pb.bank:
        coeffs = ", ".join(f"{v:.8e}f" for v in row)
        rows.append(f"    {{ {coeffs} }}")
    array_body = ",\n".join(rows)

    header = textwrap.dedent(f"""\
        /* Auto-generated polyphase filter bank — do not edit by hand.
         * Generated: {ts}
         * Phases:    {num_phases}
         * Taps/phase:{num_taps}
         * Method:    {pb.method}
         * Atten:     {pb.attenuation_db:.1f} dB
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


def to_npy(pb: PolyphaseBank, path: str | Path) -> None:
    """Save the filter bank as a NumPy .npy file.

    Parameters
    ----------
    pb:    :class:`PolyphaseBank` returned by :func:`design_bank`.
    path:  Destination file path (conventionally ``.npy``).
    """
    np.save(str(path), pb.bank)
