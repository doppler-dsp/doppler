"""
Signal generator for the spectrum analyzer.

Produces a single complex tone using doppler's NCO plus
configurable AWGN, applies a Hann window, and returns the
FFT magnitude spectrum in dBFS.
"""

from __future__ import annotations

import math
import numpy as np

# ---------------------------------------------------------------------------
# Lazy imports — the .so extensions may not be present during docs builds
# ---------------------------------------------------------------------------
_nco = None
_fft_setup_done: dict[int, bool] = {}


def _get_nco():
    global _nco
    if _nco is None:
        from doppler import Nco  # noqa: PLC0415

        _nco = Nco
    return _nco


def _ensure_fft(n: int) -> None:
    if n not in _fft_setup_done:
        from doppler.fft import setup  # noqa: PLC0415

        setup((n,))
        _fft_setup_done[n] = True


# ---------------------------------------------------------------------------
# Hann window cache
# ---------------------------------------------------------------------------
_windows: dict[int, np.ndarray] = {}


def _hann(n: int) -> np.ndarray:
    if n not in _windows:
        _windows[n] = np.hanning(n).astype(np.float32)
    return _windows[n]


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------


class SpecanState:
    """Mutable tuning state, shared between WebSocket handler and generator."""

    def __init__(
        self,
        sample_rate: float = 2.048e6,
        center_freq: float = 0.0,
        fft_size: int = 2048,
    ) -> None:
        self.sample_rate = sample_rate
        self.center_freq = center_freq  # Hz offset from DC
        self.fft_size = fft_size
        # Single tone: normalised frequency in [-0.5, 0.5)
        self.tone_freq: float = 0.1  # fraction of sample_rate
        self.tone_amp_db: float = -6.0  # dBFS
        # AWGN floor in dBFS (e.g. -40 = visible noise, -80 = very clean)
        self.noise_floor_db: float = -40.0


def generate_frame(state: SpecanState) -> list[float]:
    """
    Generate one FFT frame and return the magnitude spectrum in dBFS.

    Returns a list of `fft_size` floats (dBFS, -120 floor).
    """
    n = state.fft_size
    _ensure_fft(n)

    Nco = _get_nco()
    from doppler.fft import execute1d  # noqa: PLC0415

    # Single complex tone via NCO
    # Shift by center_freq so tuning moves the tone across the display
    f = state.tone_freq - state.center_freq / state.sample_rate
    f = f % 1.0  # wrap to [0, 1)
    amp_lin = 10.0 ** (state.tone_amp_db / 20.0)

    with Nco(f) as nco:
        sig = nco.execute_cf32(n).astype(np.complex64) * amp_lin

    # AWGN — scale by sqrt(N) so the displayed noise floor matches the
    # slider value regardless of FFT size (FFT processing gain spreads
    # noise power across N bins, each seeing ~10·log10(N) dB less).
    noise_lin = 10.0 ** (state.noise_floor_db / 20.0) * math.sqrt(n)
    sig += (
        noise_lin
        * (
            np.random.standard_normal(n).astype(np.float32)
            + 1j * np.random.standard_normal(n).astype(np.float32)
        )
        / math.sqrt(2)
    )

    # Hann window → complex128 for FFTW
    windowed = (sig * _hann(n)).astype(np.complex128)
    spectrum = execute1d(windowed)

    # Magnitude in dBFS (referenced to window power)
    mag = np.abs(spectrum)
    # Normalise by FFT size and window coherent gain (Hann ≈ 0.5)
    mag /= n * 0.5
    mag = np.maximum(mag, 1e-6)  # -120 dBFS floor
    db = 20.0 * np.log10(mag)

    # FFT-shift so DC is in the centre
    db = np.fft.fftshift(db)

    return db.tolist()
