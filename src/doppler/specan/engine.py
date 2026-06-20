"""
doppler.specan.engine — DDC + spectral analysis engine.

This is a thin orchestration layer over :class:`doppler.analyzer.Specan`, the
C-first spectrum-analyzer object that owns the whole natural-parameter signal
chain:

    IQ in (cf32, Fs_in)
      → Specan: DDC mix center→DC + decimate to Fs_out = span·1.28
                → Kaiser window → zero-pad FFT → averaged power
                → crop to the central ±span/2 display band → dB
      → (here) dBm offset + peak detection → SpectrumFrame

The DSP — the RBW→window/beta mapping, the DDC tuner/decimator, the averaging
PSD, the display crop — all live in the C object, so the engine never
reimplements (and can never silently drift from) the C ABI.  What remains in
Python is policy the C core deliberately leaves out: the dBm/50 Ω reference
calibration and turning the returned trace into :class:`Peak` / display frames.

The :class:`Specan` is lazily built on the first :meth:`process` call so the
engine can be constructed before the source's sample rate is known.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    from doppler.specan.config import SpecanConfig

# dBm calibration: amplitude=1.0 ↔ +10 dBm into 50 Ω
# P = V² / (2·Z)   →   P_mW = 1000·V²/(2·50) = V²/0.1
# dBm = 10·log10(P_mW) = 10·log10(V²/0.1) = 20·log10(V) + 10
_DBM_OFFSET = 10.0  # 20·log10(1.0) + 10 = 10 dBm at amplitude=1


@dataclass
class Peak:
    """One detected spectral peak."""

    freq_hz: float  # absolute frequency in Hz
    db: float  # amplitude in dBm


@dataclass
class SpectrumFrame:
    """One processed FFT frame ready for display."""

    db: list[float]  # dBm values, length fft_size, DC-centred
    fft_size: int  # number of display bins (the cropped passband)
    data_size: int  # N: Kaiser window / RBW frame size
    fs_out: float  # Hz — output (display) sample rate
    center_freq: float  # Hz — display center frequency
    rbw: float  # Hz — actual RBW = enbw_bins * fs_out / N
    span: float  # Hz — display span
    peaks: list[Peak] = field(default_factory=list)  # detected peaks


class SpecanEngine:
    """
    DDC + spectral analysis engine (thin wrapper over the C ``Specan``).

    Parameters
    ----------
    cfg : SpecanConfig
        Specan configuration (center, span, rbw, level).
    """

    def __init__(self, cfg: SpecanConfig) -> None:
        self._cfg = cfg
        self._specan = None
        self._fs_in: float = 0.0
        self._center_freq: float = 0.0  # source center frequency (Hz)
        self._fs_out: float = 0.0
        self._nfft: int = 0
        self._disp_n: int = 0
        self._data_size: int = 0
        self._span: float = 0.0
        self._block_size: int = 4096

    # ------------------------------------------------------------------
    # Lazy initialisation / reconfiguration
    # ------------------------------------------------------------------

    def _init_chain(self, fs_in: float, center_freq: float) -> None:
        """Build or rebuild the C ``Specan`` for a given input rate/center."""
        from doppler.analyzer import Specan

        cfg = self._cfg
        self._fs_in = fs_in
        self._center_freq = center_freq

        # Resolve the natural parameters (auto span/rbw) the same way the
        # config has always defined them, then hand concrete values to C.
        span = cfg.effective_span(fs_in)
        rbw = cfg.effective_rbw(span)

        if self._specan is not None:
            self._specan.destroy()
        # offset_db carries the dBm calibration / ref-level offset, applied on
        # top of the core's dBFS reference (full_scale = 1.0 here: the demo and
        # IQ sources are amplitude-normalised, not ADC codes).
        self._specan = Specan(
            fs=fs_in,
            span=span,
            rbw=rbw,
            src_center=center_freq,
            center=cfg.center,
            offset_db=_DBM_OFFSET - cfg.level,
            window="kaiser",
            navg=1,
        )

        self._fs_out = self._specan.fs_out
        self._nfft = self._specan.nfft
        self._disp_n = self._specan.display_size
        self._data_size = self._specan.n
        self._span = span
        # Read enough input per call to fill a display frame (n decimated
        # samples ≈ n / rate input samples), with a sane floor.
        self._block_size = max(
            int(self._data_size * fs_in / self._fs_out), 4096
        )

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    def process(
        self, iq: np.ndarray, fs_in: float, center_freq: float
    ) -> SpectrumFrame | None:
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
        from doppler.spectral import find_peaks_f32

        if fs_in != self._fs_in or center_freq != self._center_freq:
            self._init_chain(fs_in, center_freq)

        # Mix, decimate, window, FFT, average, crop, dB — all in C.
        db = self._specan.execute(iq.astype(np.complex64))
        if db is None:
            return None

        # Detect peaks; threshold 60 dB below reference level.
        min_db = float(self._cfg.level) - 60.0
        raw_peaks = find_peaks_f32(db, 8, min_db)
        peaks = [
            Peak(freq_hz=self._cfg.center + p[0] * self._span, db=p[1])
            for p in raw_peaks
        ]

        rbw = self._specan.rbw
        return SpectrumFrame(
            db=db.tolist(),
            fft_size=len(db),
            data_size=self._data_size,
            fs_out=self._fs_out,
            center_freq=self._cfg.center,
            rbw=rbw,
            span=self._span,
            peaks=peaks,
        )

    # ------------------------------------------------------------------
    # Retune / zoom
    # ------------------------------------------------------------------

    def retune(self, center: float) -> None:
        """Shift the display center frequency (cheap C-level LO retune)."""
        self._cfg.center = center
        if self._specan is not None:
            self._specan.retune(center)

    def zoom(self, span: float) -> None:
        """Change the display span (triggers full chain rebuild)."""
        self._cfg.span = span
        self._fs_in = 0.0  # force reinit on next process()

    # ------------------------------------------------------------------
    # Cleanup
    # ------------------------------------------------------------------

    def close(self) -> None:
        if self._specan is not None:
            self._specan.destroy()
            self._specan = None

    @property
    def block_size(self) -> int:
        """Suggested number of input samples per :meth:`process` call."""
        return self._block_size
