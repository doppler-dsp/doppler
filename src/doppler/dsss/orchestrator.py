"""Elastic multi-channel DSSS acquirer — a coarse-Doppler mixer bank.

A single :class:`~doppler.dsss.Acquisition` searches only its *native* Doppler
span, ``±chip_rate/(2*sf)`` (the slow-time FFT's unambiguous range — beyond it
the per-segment integrate-and-dump's ``sinc`` rolloff nulls the correlation at
``±2*span``).  To acquire a burst whose Doppler is uncertain over a *wider*
range, this tiles ``K`` coarse-Doppler **channels**: each down-mixes its
sub-band to baseband with a :class:`~doppler.ddc.DDC` and runs its own
``Acquisition`` there, so the bank covers ``±doppler_uncertainty``.

Each channel is an independent ``DDC → Acquisition`` pipeline over the elastic
pure kernels, so the bank fans out across a thread pool (the C kernels release
the GIL).  The same channels are shippable as ``(descriptor, state, block)`` to
separate processes / pods — that state-snapshot hook is the next increment;
this first cut is threaded, single-process.

Why ``2*span`` spacing works
----------------------------
The fast-time code correlation coherently integrates one segment
(``sf`` chips), an integrate-and-dump of duration ``T_seg = sf/chip_rate``.
Its frequency response is ``sinc(r * T_seg)``, with its first null at
``r = 1/T_seg = chip_rate/sf = 2*span``.  So a target whose residual Doppler
(after a channel's mix) exceeds that channel's ``±span`` is progressively
suppressed and *nulled* a full ``2*span`` away — the bank does not alias one
channel's target into another.  A target landing between two channel centers
appears in both at ``~ -4 dB`` (``sinc(0.5)``); :meth:`Acquirer.process`
dedups those to the single strongest detection.

Examples
--------
>>> import numpy as np
>>> from doppler.dsss.orchestrator import Acquirer
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> acq = Acquirer(
...     code,
...     doppler_uncertainty_hz=3.0e5,
...     source_rate=8.0e6,
...     spc=2,
...     chip_rate=1.0e6,
...     reps=8,
...     cn0_dbhz=20.0,
... )
>>> acq.n_channels >= 3
True
"""

from __future__ import annotations

import math
import warnings
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from typing import TYPE_CHECKING

import numpy as np

from doppler.ddc import DDC
from doppler.dsss import Acquisition

if TYPE_CHECKING:
    from numpy.typing import NDArray

__all__ = ["Acquirer", "CoarseChannel", "Detection"]


@dataclass(frozen=True)
class Detection:
    """One acquisition hit, reported in absolute (bank-level) coordinates.

    Attributes
    ----------
    doppler_hz : float
        Absolute Doppler estimate = channel center + fine (slow-time) bin.
    code_phase : int
        Code phase (samples) at the channel's acquisition rate.
    test_stat : float
        CFAR test statistic (peak / noise) reported by the channel.
    snr_est : float
        Estimated per-sample amplitude SNR of the burst.
    channel : int
        Index of the coarse channel that reported the hit.
    """

    doppler_hz: float
    code_phase: int
    test_stat: float
    snr_est: float
    channel: int


class CoarseChannel:
    """One ``DDC(mix → decimate) → Acquisition`` pipeline at center ``f_hz``.

    The DDC mixes ``f_hz`` to DC (``norm_freq = -f_hz/source_rate``) and
    decimates the input to the acquisition rate (``chip_rate*spc``); the
    Acquisition then searches the residual Doppler over its native span.
    Stateful across :meth:`process` calls (one channel per stream).
    """

    def __init__(
        self,
        f_hz: float,
        *,
        source_rate: float,
        code: NDArray[np.uint8],
        reps: int,
        spc: int,
        chip_rate: float,
        cn0_dbhz: float,
        pfa: float,
        pd: float,
        noise_mode: str = "mean",
        max_noncoh: int = 1,
    ) -> None:
        acq_fs = chip_rate * spc
        if not source_rate >= acq_fs:
            raise ValueError(
                f"source_rate ({source_rate}) must be >= chip_rate*spc "
                f"({acq_fs}); the DDC decimates down to the acq rate"
            )
        self.f_hz = float(f_hz)
        self._ddc = DDC(
            norm_freq=-f_hz / source_rate, rate=acq_fs / source_rate
        )
        # Each channel covers its own native span; the bank provides the width,
        # so the per-channel search uses the full native span (uncertainty 0).
        with warnings.catch_warnings():
            # A conservative sizing C/N0 may flag under-power; the bank (not
            # the single channel) is the operating point — caller decides.
            warnings.simplefilter("ignore", UserWarning)
            self._acq = Acquisition(
                code,
                reps=reps,
                spc=spc,
                chip_rate=chip_rate,
                cn0_dbhz=cn0_dbhz,
                doppler_uncertainty=0.0,
                pfa=pfa,
                pd=pd,
                noise_mode=noise_mode,
                max_noncoh=max_noncoh,
            )
        self._res = self._acq.doppler_res_hz
        self._nbins = self._acq.doppler_bins

    @property
    def acquisition(self) -> Acquisition:
        """The underlying per-channel :class:`~doppler.dsss.Acquisition`."""
        return self._acq

    def _abs_doppler(self, doppler_bin: int) -> float:
        """Map a folded slow-time bin to absolute Doppler (Hz)."""
        # Fold [0, nbins) -> [-nbins/2, nbins/2): a residual above +span reads
        # as a negative offset (the slow-time FFT is circular).
        signed = doppler_bin
        if doppler_bin >= (self._nbins + 1) // 2:
            signed = doppler_bin - self._nbins
        return self.f_hz + signed * self._res

    def process(
        self, block: NDArray[np.complex64], index: int
    ) -> list[Detection]:
        """Down-mix + acquire a block; return absolute-coordinate hits."""
        baseband = self._ddc.execute(block)
        hits = []
        for dop_bin, code_phase, _peak, _noise, stat, snr in self._acq.push(
            baseband
        ):
            hits.append(
                Detection(
                    self._abs_doppler(dop_bin),
                    int(code_phase),
                    float(stat),
                    float(snr),
                    index,
                )
            )
        return hits


class Acquirer:
    """Coarse-Doppler mixer bank over ``K`` ``CoarseChannel`` pipelines.

    Lays out channel centers spaced by one native span (``2*doppler_span_hz``)
    across ``±doppler_uncertainty_hz``, fans :meth:`process` across a thread
    pool, and dedups detections that the same target produced in adjacent
    (overlapping) channels.

    Parameters
    ----------
    code : ndarray[uint8]
        PN chips (0/1).
    doppler_uncertainty_hz : float
        One-sided Doppler search half-range the bank must cover (Hz).
    source_rate : float
        Input sample rate (Hz); must be >= ``chip_rate*spc``.
    spc : int
        Samples per chip at the acquisition rate.
    chip_rate : float
        Chip rate (Hz).
    reps : int, default 1
        Max coherent code repetitions per channel (the Acquisition ceiling).
    cn0_dbhz, pfa, pd, noise_mode, max_noncoh
        Per-channel :class:`~doppler.dsss.Acquisition` detection parameters.
    max_workers : int, optional
        Thread-pool size; defaults to the channel count.
    """

    def __init__(
        self,
        code: NDArray[np.uint8],
        *,
        doppler_uncertainty_hz: float,
        source_rate: float,
        spc: int,
        chip_rate: float,
        reps: int = 1,
        cn0_dbhz: float = 50.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        noise_mode: str = "mean",
        max_noncoh: int = 1,
        max_workers: int | None = None,
    ) -> None:
        if doppler_uncertainty_hz < 0.0:
            raise ValueError("doppler_uncertainty_hz must be >= 0")
        ch_kw = {
            "source_rate": source_rate,
            "code": code,
            "reps": reps,
            "spc": spc,
            "chip_rate": chip_rate,
            "cn0_dbhz": cn0_dbhz,
            "pfa": pfa,
            "pd": pd,
            "noise_mode": noise_mode,
            "max_noncoh": max_noncoh,
        }
        # Probe one channel to read the native span / resolution.
        probe = CoarseChannel(0.0, **ch_kw)
        self.span_hz = probe._acq.doppler_span_hz
        self.res_hz = probe._acq.doppler_res_hz
        step = 2.0 * self.span_hz  # each channel covers ±span

        # Odd channel count centered on 0, covering [-U, +U]; reuse the probe
        # as the center (f = 0) channel and build the rest around it.
        half = math.ceil(doppler_uncertainty_hz / step) if step > 0 else 0
        centers = [k * step for k in range(-half, half + 1)]
        self.channels = [
            probe if i == half else CoarseChannel(centers[i], **ch_kw)
            for i in range(len(centers))
        ]
        self._pool = ThreadPoolExecutor(
            max_workers=max_workers or len(self.channels)
        )

    @property
    def n_channels(self) -> int:
        """Number of coarse-Doppler channels in the bank."""
        return len(self.channels)

    @property
    def centers_hz(self) -> list[float]:
        """Coarse-Doppler channel centers (Hz)."""
        return [c.f_hz for c in self.channels]

    def process(self, block: NDArray[np.complex64]) -> list[Detection]:
        """Acquire one input block across all channels; dedup overlaps.

        Channels run concurrently on the thread pool (the C acquisition kernel
        releases the GIL).  Detections of one target in adjacent channels are
        merged to the single strongest ``(doppler, code_phase)``.
        """
        block = np.ascontiguousarray(block, dtype=np.complex64)
        futures = [
            self._pool.submit(c.process, block, i)
            for i, c in enumerate(self.channels)
        ]
        hits = [d for f in futures for d in f.result()]
        return self._dedup(hits)

    def acquire(self, block: NDArray[np.complex64]) -> Detection | None:
        """Acquire one block and return the single strongest detection (the
        acquisition decision), or ``None`` if no channel fired."""
        dets = self.process(block)
        return max(dets, key=lambda d: d.test_stat) if dets else None

    def _dedup(self, hits: list[Detection]) -> list[Detection]:
        """Collapse same-target detections (≤ res in Doppler, ≤ 1 in code
        phase) to the strongest, keeping cross-channel order by strength."""
        kept: list[Detection] = []
        for d in sorted(hits, key=lambda h: -h.test_stat):
            if any(
                abs(d.doppler_hz - k.doppler_hz) <= self.res_hz
                and abs(d.code_phase - k.code_phase) <= 1
                for k in kept
            ):
                continue
            kept.append(d)
        return kept

    def close(self) -> None:
        """Shut down the thread pool."""
        self._pool.shutdown(wait=True)

    def __enter__(self) -> Acquirer:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()
