"""
doppler_specan.engine — DDC processing engine.

The engine owns the complete signal chain:

    IQ in (cf32, Fs_in)
      → DDC: NCO mix to DC + DPMFS resample to Fs_out  (dp_ddc)
      → Kaiser window
      → FFT  (dp_fft)
      → Magnitude → dBm

All doppler primitives are lazy-initialised on the first call to
:meth:`SpecanEngine.process` so the engine can be constructed before
the source's sample rate is known.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np

# dBm calibration: amplitude=1.0 ↔ +10 dBm into 50 Ω
# P = V² / (2·Z)   →   P_mW = 1000·V²/(2·50) = V²/0.1
# dBm = 10·log10(P_mW) = 10·log10(V²/0.1) = 20·log10(V) + 10
_DBM_OFFSET = 10.0  # 20·log10(1.0) + 10 = 10 dBm at amplitude=1


@dataclass
class SpectrumFrame:
    """One processed FFT frame ready for display."""

    db: list[float]  # dBm values, length fft_size, DC-centred
    fft_size: int
    fs_out: float  # Hz — output (display) sample rate
    center_freq: float  # Hz — source center frequency
    rbw: float  # Hz — actual RBW = enbw_bins * fs_out / N
    span: float  # Hz — display span = 0.8 * fs_out


class SpecanEngine:
    """
    DDC + spectral analysis engine.

    Parameters
    ----------
    cfg : SpecanConfig
        Specan configuration (center, span, rbw, beta, level).
    """

    def __init__(self, cfg) -> None:
        self._cfg = cfg
        self._ddc = None
        self._window: Optional[np.ndarray] = None
        self._fft_size: int = 0
        self._fs_out: float = 0.0
        self._fs_in: float = 0.0
        self._center_freq: float = 0.0
        self._enbw_bins: float = 1.0
        self._block_size: int = 4096  # input block size fed to chain

    # ------------------------------------------------------------------
    # Lazy initialisation / reconfiguration
    # ------------------------------------------------------------------

    def _init_chain(self, fs_in: float, center_freq: float) -> None:
        """Build or rebuild the DDC chain for a given input rate."""
        from doppler.ddc import Ddc  # noqa: PLC0415
        from doppler.fft import setup  # noqa: PLC0415
        from doppler.window import kaiser_beta_for_enbw, kaiser_enbw, kaiser_window  # noqa: PLC0415

        cfg = self._cfg
        self._fs_in = fs_in
        self._center_freq = center_freq

        span = cfg.effective_span(fs_in)
        fs_out = cfg.fs_out(span)
        rbw = cfg.effective_rbw(span)
        n = cfg.fft_size(fs_out, rbw)

        self._fs_out = fs_out
        self._span = span
        self._fft_size = n
        self._block_size = max(n * 4, 4096)

        # DDC: mix (center - source_center) to DC, then resample to fs_out
        rate = fs_out / fs_in
        norm_freq = (cfg.center - center_freq) / fs_in
        if self._ddc is not None:
            self._ddc.__exit__(None, None, None)
        self._ddc = Ddc(norm_freq, self._block_size, rate)
        self._ddc.__enter__()

        # Kaiser window — beta is the little RBW knob, N the big knob.
        # target_enbw_bins = rbw / bin_width is always in [1.0, 2.0)
        # because N is the smallest power-of-two >= fs_out / rbw.
        bin_width = fs_out / n
        target_enbw_bins = rbw / bin_width
        beta = kaiser_beta_for_enbw(target_enbw_bins, n)
        w = kaiser_window(n, beta)
        self._enbw_bins = kaiser_enbw(w)
        # Normalise so window power = 1 (preserves dBm calibration)
        self._window = w.astype(np.float64) / float(w.sum())

        # FFT plan
        setup((n,))

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    def process(
        self, iq: np.ndarray, fs_in: float, center_freq: float
    ) -> Optional[SpectrumFrame]:
        """
        Process one block of IQ samples and return a spectrum frame.

        Parameters
        ----------
        iq : ndarray, dtype=complex64
            Input samples at ``fs_in``.
        fs_in : float
            Input sample rate in Hz.
        center_freq : float
            Source center frequency in Hz.

        Returns
        -------
        SpectrumFrame or None
            ``None`` if the block is too short to fill an FFT frame yet.
        """
        # (Re-)initialise chain if rate changed
        if fs_in != self._fs_in or center_freq != self._center_freq:
            self._init_chain(fs_in, center_freq)
            self._pending = np.empty(0, dtype=np.complex64)

        if not hasattr(self, "_pending"):
            self._pending = np.empty(0, dtype=np.complex64)

        # Mix to DC and resample to fs_out via DDC
        resampled = self._ddc.execute(iq.astype(np.complex64))

        # Accumulate until we have a full FFT frame
        self._pending = np.concatenate([self._pending, resampled])
        if len(self._pending) < self._fft_size:
            return None

        frame_iq = self._pending[: self._fft_size].copy()
        self._pending = self._pending[self._fft_size :]

        return self._compute_spectrum(frame_iq)

    def _compute_spectrum(self, iq: np.ndarray) -> SpectrumFrame:
        """Apply window → FFT → magnitude → dBm."""
        from doppler.fft import execute1d  # noqa: PLC0415

        n = self._fft_size
        # Apply Kaiser window and convert to complex128 for FFTW
        windowed = iq.astype(np.complex128) * self._window.astype(np.complex128)
        spectrum = execute1d(windowed)

        # Magnitude in linear → dBm.
        # Window was pre-normalised by sum(w) so coherent gain = 1:
        # FFT[k_tone] = A directly.  No /N needed.
        mag = np.abs(spectrum)
        mag = np.maximum(mag, 1e-12)  # floor at −240 dBm

        # dBm: 20·log10(mag) + _DBM_OFFSET − level_offset
        db = (20.0 * np.log10(mag) + _DBM_OFFSET - self._cfg.level).tolist()

        # FFT-shift so DC is centred
        db = np.fft.fftshift(db).tolist()

        rbw = self._enbw_bins * self._fs_out / n

        return SpectrumFrame(
            db=db,
            fft_size=n,
            fs_out=self._fs_out,
            center_freq=self._center_freq,
            rbw=rbw,
            span=self._span,
        )

    # ------------------------------------------------------------------
    # Retune
    # ------------------------------------------------------------------

    def retune(self, center: float) -> None:
        """Shift the display center frequency."""
        self._cfg.center = center
        if self._fs_in > 0 and self._ddc is not None:
            norm_freq = (center - self._center_freq) / self._fs_in
            self._ddc.set_freq(norm_freq)

    def zoom(self, span: float) -> None:
        """Change the display span (triggers full chain rebuild)."""
        self._cfg.span = span
        self._fs_in = 0.0  # force reinit on next process()

    # ------------------------------------------------------------------
    # Cleanup
    # ------------------------------------------------------------------

    def close(self) -> None:
        if self._ddc is not None:
            self._ddc.__exit__(None, None, None)
            self._ddc = None

    @property
    def block_size(self) -> int:
        """Suggested number of input samples per :meth:`process` call."""
        return self._block_size
