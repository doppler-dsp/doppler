"""
doppler_specan.source — IQ sample sources.

All sources produce blocks of complex64 (cf32) samples and report the
current sample rate and center frequency.  The engine consumes these
blocks without knowing which source produced them.

Sources
-------
DemoSource
    Synthetic signal: calibrated complex tone + AWGN.  Default source.
    Produces -20 dBm tone + configurable noise floor.

FileSource
    Raw interleaved cf32 IQ file.  Sample rate and center frequency
    must be supplied via config (no metadata in raw files).

SocketSource
    ZMQ PUB/SUB subscriber.  Rate and center frequency are
    auto-discovered from the ``dp_header_t`` carried in each packet.
"""

from __future__ import annotations

import math
from abc import ABC, abstractmethod

import numpy as np


# ------------------------------------------------------------------
# Abstract base
# ------------------------------------------------------------------


class Source(ABC):
    """Abstract IQ source."""

    @abstractmethod
    def read(self, n: int) -> tuple[np.ndarray, float, float]:
        """
        Read *n* complex64 samples.

        Returns
        -------
        iq : ndarray, dtype=complex64, shape=(m,)
            IQ samples.  May be fewer than *n* at end-of-file.
        sample_rate : float
            Sample rate in Hz.
        center_freq : float
            Center frequency in Hz.
        """
        ...

    def set_fft_size(self, n: int) -> None:
        """Notify the source of the current FFT size (optional hint)."""

    def close(self) -> None:
        """Release any held resources."""


# ------------------------------------------------------------------
# Demo source
# ------------------------------------------------------------------

_DBM_REF = 1.0 / (2.0 * 50.0 * 0.001)  # 1/(2*Z*P_ref) for dBm into 50Ω


def _dbm_to_amplitude(dbm: float) -> float:
    """Convert dBm (into 50 Ω) to linear amplitude (V peak)."""
    # P = V²/(2Z)  →  V = sqrt(P * 2Z)
    # P (watts) = 10^(dBm/10) * 1e-3
    p_watts = 10.0 ** (dbm / 10.0) * 1e-3
    return math.sqrt(p_watts * 2.0 * 50.0)


class DemoSource(Source):
    """
    Synthetic IQ source: calibrated tone + AWGN.

    Generates a complex tone at ``tone_freq`` offset from DC using
    doppler's NCO, plus Gaussian noise at ``noise_floor`` dBm.
    Signal levels are calibrated: amplitude=1 ↔ +10 dBm into 50 Ω.

    Parameters
    ----------
    sample_rate : float
        Output sample rate in Hz.
    center_freq : float
        Nominal center frequency in Hz (metadata only, no RF tuning).
    tone_freq : float
        Tone offset from DC in Hz.
    tone_power : float
        Tone power in dBm.
    noise_floor : float
        AWGN floor in dBm.
    """

    def __init__(
        self,
        sample_rate: float = 2.048e6,
        center_freq: float = 0.0,
        tone_freq: float = 100e3,
        tone_power: float = -20.0,
        noise_floor: float = -90.0,
    ) -> None:
        self._fs = float(sample_rate)
        self._cf = float(center_freq)
        self._tone_fn = float(tone_freq) / self._fs  # normalised [0,1)
        self._tone_amp = _dbm_to_amplitude(tone_power)
        self._noise_amp = _dbm_to_amplitude(noise_floor)
        self._nco = None  # created lazily
        self._fft_size = 512  # updated by engine via set_fft_size()
        # Store dBm values for external readback
        self._tone_power_dbm = float(tone_power)
        self._noise_floor_dbm = float(noise_floor)

    def set_fft_size(self, n: int) -> None:
        self._fft_size = n

    def set_tone_freq(self, fn: float) -> None:
        """Set tone frequency (normalised [0, 1))."""
        self._tone_fn = float(fn) % 1.0
        if self._nco is not None:
            self._nco.__exit__(None, None, None)
            self._nco = None  # recreated lazily on next read

    def set_tone_power(self, dbm: float) -> None:
        """Set tone power in dBm."""
        self._tone_power_dbm = float(dbm)
        self._tone_amp = _dbm_to_amplitude(dbm)

    def set_noise_floor(self, dbm: float) -> None:
        """Set noise floor in dBm."""
        self._noise_floor_dbm = float(dbm)
        self._noise_amp = _dbm_to_amplitude(dbm)

    def _get_nco(self):
        if self._nco is None:
            from doppler import Nco  # noqa: PLC0415

            self._nco = Nco(self._tone_fn)
        return self._nco

    def read(self, n: int) -> tuple[np.ndarray, float, float]:
        nco = self._get_nco()
        # Tone: NCO at normalised frequency
        sig = nco.execute_cf32(n).astype(np.complex64) * self._tone_amp

        # AWGN: scale by sqrt(fft_size) so the displayed noise floor
        # matches noise_floor_dbm at the current FFT size.  Uses the
        # FFT size (not the block read size) so the level stays stable
        # when block_size changes on zoom.
        noise_scale = self._noise_amp * math.sqrt(self._fft_size)
        rng = np.random.standard_normal((n, 2)).astype(np.float32)
        sig += (
            (rng[:, 0] + 1j * rng[:, 1]).astype(np.complex64)
            * noise_scale
            / math.sqrt(2)
        )

        return sig, self._fs, self._cf

    def close(self) -> None:
        if self._nco is not None:
            self._nco.__exit__(None, None, None)
            self._nco = None


# ------------------------------------------------------------------
# File source
# ------------------------------------------------------------------


class FileSource(Source):
    """
    Raw interleaved cf32 IQ file.

    Reads 32-bit float I/Q pairs from a binary file.  Loops back to
    the beginning when the file is exhausted (continuous playback).

    Parameters
    ----------
    path : str or Path
        Path to a raw cf32 file (interleaved float32 I, Q pairs).
    sample_rate : float
        Sample rate in Hz (must be supplied; not embedded in file).
    center_freq : float
        Center frequency in Hz (metadata; not embedded in file).
    """

    def __init__(
        self,
        path,
        sample_rate: float,
        center_freq: float = 0.0,
    ) -> None:
        from pathlib import Path  # noqa: PLC0415

        self._path = Path(path)
        self._fs = float(sample_rate)
        self._cf = float(center_freq)
        self._fh = open(self._path, "rb")  # noqa: SIM115

    def read(self, n: int) -> tuple[np.ndarray, float, float]:
        raw = np.frombuffer(self._fh.read(n * 8), dtype=np.float32)
        if len(raw) < n * 2:
            # EOF — loop
            self._fh.seek(0)
            remainder = np.frombuffer(
                self._fh.read((n - len(raw) // 2) * 8), dtype=np.float32
            )
            raw = np.concatenate([raw, remainder])

        iq = raw.view(np.complex64)
        return iq[:n].copy(), self._fs, self._cf

    def close(self) -> None:
        if not self._fh.closed:
            self._fh.close()


# ------------------------------------------------------------------
# Socket source
# ------------------------------------------------------------------


class SocketSource(Source):
    """
    ZMQ PUB/SUB subscriber.

    Connects to a doppler streaming publisher and auto-configures
    sample rate and center frequency from the ``dp_header_t`` carried
    in each packet — no ``--fs`` flag required.

    Parameters
    ----------
    address : str
        ZMQ endpoint, e.g. ``"tcp://localhost:5555"``.
    timeout_ms : int
        Receive timeout in milliseconds.
    """

    def __init__(
        self,
        address: str,
        timeout_ms: int = 2000,
    ) -> None:
        self._address = address
        self._timeout_ms = timeout_ms
        self._sub = None
        self._fs: float = 0.0
        self._cf: float = 0.0
        self._buf = np.empty(0, dtype=np.complex64)

    def _get_sub(self):
        if self._sub is None:
            from doppler import Subscriber  # noqa: PLC0415

            self._sub = Subscriber(self._address)
            self._sub.__enter__()
        return self._sub

    def read(self, n: int) -> tuple[np.ndarray, float, float]:
        sub = self._get_sub()

        # Drain packets until the buffer holds >= n samples
        while len(self._buf) < n:
            data, hdr = sub.recv(timeout_ms=self._timeout_ms)
            if data is None:
                # Timeout — return what we have (may be empty)
                break
            # Convert to complex64 if needed
            if data.dtype != np.complex64:
                data = data.astype(np.complex64)
            self._fs = float(hdr.sample_rate)
            self._cf = float(hdr.center_freq)
            self._buf = np.concatenate([self._buf, data.ravel()])

        out = self._buf[:n].copy()
        self._buf = self._buf[n:]
        return out, self._fs, self._cf

    def close(self) -> None:
        if self._sub is not None:
            self._sub.__exit__(None, None, None)
            self._sub = None


# ------------------------------------------------------------------
# Factory
# ------------------------------------------------------------------


def make_source(cfg) -> Source:
    """
    Instantiate the source described by *cfg*.

    Parameters
    ----------
    cfg : SpEcanConfig

    Returns
    -------
    Source
    """
    s = cfg.source.lower()
    if s == "demo":
        return DemoSource(
            sample_rate=cfg.fs if cfg.fs > 0 else 2.048e6,
            center_freq=cfg.center,
            tone_freq=cfg.demo.tone_freq,
            tone_power=cfg.demo.tone_power,
            noise_floor=cfg.demo.noise_floor,
        )
    if s == "file":
        if not cfg.address:
            raise ValueError("source=file requires an address (file path)")
        if cfg.fs <= 0:
            raise ValueError("source=file requires --fs <sample_rate>")
        return FileSource(cfg.address, cfg.fs, cfg.center)
    if s == "socket":
        if not cfg.address:
            raise ValueError("source=socket requires an address (ZMQ endpoint)")
        return SocketSource(cfg.address)
    raise ValueError(f"Unknown source: {cfg.source!r}")
