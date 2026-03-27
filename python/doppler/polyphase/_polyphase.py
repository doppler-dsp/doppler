"""
doppler.polyphase._polyphase
============================

Kaiser window design formulas for polyphase FIR filter banks.
"""

from __future__ import annotations

import numpy as np

__all__ = [
    "kaiser_beta",
    "kaiser_taps",
    "kaiser_prototype",
    "plot_group_delay",
]


def kaiser_beta(attenuation_db: float) -> float:
    """Return Kaiser window β for the given stopband attenuation (dB)."""
    a = float(attenuation_db)
    if a > 50.0:
        return 0.1102 * (a - 8.7)
    if a >= 21.0:
        return 0.5842 * (a - 21.0) ** 0.4 + 0.07886 * (a - 21.0)
    return 0.0


def kaiser_taps(
    attenuation: float = 60.0,
    passband: float = 0.4,
    stopband: float = 0.6,
) -> int:
    """Return prototype filter length for the given Kaiser spec."""
    return int(1 + (attenuation - 8) / 2.285 / (2.0 * np.pi * (stopband - passband)))


def kaiser_prototype(
    attenuation: float = 60.0,
    passband: float = 0.4,
    stopband: float = 0.6,
    image_attenuation: float = 80.0,
) -> np.ndarray:
    """Return a Kaiser-windowed sinc prototype filter (float32)."""
    db_per_bit = 6.02
    phases = 1 << int(
        np.ceil((20.0 * np.log10(passband) + image_attenuation) / db_per_bit)
    )
    halflen = kaiser_taps(attenuation, passband / phases, stopband / phases) // 2
    htaps = 2 * halflen + 1
    m = np.arange(0, htaps) - halflen
    window = np.kaiser(htaps, kaiser_beta(attenuation))
    wc = 2 * np.pi * (passband / phases + (stopband - passband) / (2 * phases))
    h = window * wc / np.pi * np.sinc(wc / np.pi * m)
    taps_per_phase = int(htaps / phases) + 1
    g = np.zeros(phases * taps_per_phase)
    g[:htaps] = h * phases
    polyphase = np.reshape(g.astype(np.float32), (taps_per_phase, phases)).T
    prototype = g.astype(np.float32)
    return prototype, polyphase


def plot_group_delay(
    polyphase: np.ndarray,
    passband: float = 0.4,
    *,
    num_freqs: int = 4096,
    max_phases: int = 64,
    ax=None,
):
    """Plot the group delay of each polyphase branch.

    Each curve is one polyphase phase; colour encodes phase index
    (0 = purple, L-1 = yellow, plasma colormap).  The y-axis is
    zoomed to the passband region to reveal the flat group delay and
    the fractional-sample staircase between branches.

    Parameters
    ----------
    polyphase : ndarray, shape (L, N)
        Polyphase bank as returned by :func:`kaiser_prototype`.
    passband : float
        Normalised passband edge (cycles/sample).  Used to shade
        the passband and anchor the ideal-delay reference line.
    num_freqs : int
        FFT size for group delay evaluation (default 4096).
    max_phases : int
        Maximum number of phase curves drawn; phases are evenly
        subsampled when L > max_phases (default 64).
    ax : matplotlib.axes.Axes, optional
        Axes to draw on; a new figure is created if *None*.

    Returns
    -------
    fig : matplotlib.figure.Figure
    ax  : matplotlib.axes.Axes

    Examples
    --------
    >>> import doppler.polyphase as dp  # doctest: +SKIP
    >>> _, bank = dp.kaiser_prototype(passband=0.4, stopband=0.6)
    >>> fig, ax = dp.plot_group_delay(bank, passband=0.4)  # doctest: +SKIP
    """
    import matplotlib.pyplot as plt
    import matplotlib.cm as cm

    bank = np.asarray(polyphase, dtype=float)
    L, N = bank.shape
    n = np.arange(N, dtype=float)
    freqs = np.fft.rfftfreq(num_freqs)

    step = max(1, L // max_phases)
    indices = np.arange(0, L, step)
    colors = cm.plasma(np.linspace(0.1, 0.9, len(indices)))

    if ax is None:
        fig, ax = plt.subplots(figsize=(9, 5))
    else:
        fig = ax.get_figure()

    ideal_gd = (N - 1) / 2.0

    for color, p in zip(colors, indices):
        h = bank[p]
        H = np.fft.rfft(h, num_freqs)
        nH = np.fft.rfft(n * h, num_freqs)
        gd = np.real(nH * np.conj(H)) / (np.abs(H) ** 2 + 1e-30)
        ax.plot(freqs, gd, color=color, linewidth=0.7, alpha=0.75)

    ax.axvspan(
        0,
        passband,
        alpha=0.10,
        color="steelblue",
        zorder=0,
        label="passband",
    )
    ax.axhline(
        ideal_gd,
        color="0.5",
        linewidth=1.0,
        linestyle="--",
        label=f"ideal = {ideal_gd:.1f} samples",
    )

    # Zoom y-axis: ideal ± 1 sample spread + padding
    pad = 0.4
    ax.set_ylim(ideal_gd - 1.0 - pad, ideal_gd + 1.0 + pad)
    ax.set_xlim(0.0, 0.5)

    sm = plt.cm.ScalarMappable(
        cmap="plasma",
        norm=plt.Normalize(vmin=0, vmax=L - 1),
    )
    sm.set_array([])
    cbar = fig.colorbar(sm, ax=ax, pad=0.02)
    cbar.set_label("phase index  p", labelpad=8)

    ax.set_xlabel("normalised frequency  (cycles/sample)")
    ax.set_ylabel("group delay  (samples)")
    ax.set_title(f"polyphase bank group delay — L={L} phases × N={N} taps/phase")
    ax.legend(loc="upper right", fontsize=9)

    return fig, ax
