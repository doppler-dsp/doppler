# wfm_compose.pyi — composer OO types (jm; gh-287).
from __future__ import annotations
from typing import Any, Iterator
import numpy as np
from numpy.typing import NDArray

class Synth:
    """Synth.

    Parameters
    ----------
    type : str, default ``"tone"``
        Waveform type.
        One of ``"tone"``, ``"noise"``, ``"pn"``, ``"bpsk"``, ``"qpsk"``, ``"chirp"``, ``"bits"``, ``"symbols"``, ``"dsss"``.
    freq : float | tuple[float, float], default 0.0
        Carrier/offset frequency in Hz (normalised cycles/sample when fs=1); for chirp it is the start frequency.
    snr : float | tuple[float, float], default 100.0
        Signal-to-noise ratio in dB, interpreted per snr_mode; >=100 is treated as clean (no AWGN).
    snr_mode : str, default ``"auto"``
        How snr is interpreted: auto picks fs for tone/pn/chirp/bits and Es/No for bpsk/qpsk.
        One of ``"auto"``, ``"fs"``, ``"ebno"``, ``"esno"``.
    seed : int, default 0
        PRNG/LFSR seed for the noise and PN streams.
    sps : int, default 1
        Samples per symbol (PSK) or per chip (PN); the oversampling factor.
    pn_length : int, default 15
        PN LFSR register length; the sequence period is 2^pn_length - 1.
    pn_poly : int, default 0
        PN generator polynomial; 0 auto-selects a maximal-length (MLS) polynomial for pn_length.
    lfsr : str, default ``"galois"``
        PN LFSR realization (galois or fibonacci); same period, different chip order.
        One of ``"galois"``, ``"fibonacci"``.
    level : float | tuple[float, float], default 0.0
        Source power in dBFS (<=0; 0 = unit power). Applies only when summed in a Segment/Composer (gain 10^(level/20)); ignored by standalone Synth.steps().
    f_end : float | tuple[float, float], default 0.0
        Chirp end frequency in Hz; ignored by non-chirp types.
    bits : bytes | None, default None
        For type=bits: the 0/1 pattern, oversampled by sps and cycled to fill the request. For type=dsss (as `payload`): the payload bits of the burst frame.
    modulation : str, default ``"bpsk"``
        For type=bits: symbol mapping of the pattern (none=0/1 amplitude, bpsk, qpsk).
        One of ``"none"``, ``"bpsk"``, ``"qpsk"``.
    pulse : str, default ``"rect"``
        Pulse shape for the symbol stream (pn/bpsk/qpsk/bits): rect sample-and-hold or rrc matched filter.
        One of ``"rect"``, ``"rrc"``.
    rrc_beta : float, default 0.35
        RRC roll-off factor in (0, 1] when pulse=rrc.
    rrc_span : int, default 8
        RRC filter span in symbols when pulse=rrc (taps = 2*span*sps + 1).
    symbols : NDArray[np.complex64] | None, default None
        For type=symbols: a complex64 constellation stream — each element is the output point itself, oversampled by sps, cycled, and RRC-shaped with pulse=rrc. Generalises any modulation (pi/4-QPSK, QAM, ...).
    acq_code : bytes | None, default None
        For type=dsss: the acquisition/preamble code (0/1 chips), repeated acq_reps times unmodulated at the head of the burst — the coherent pull-in target BurstDespreader.set_acq/BurstDemod.set_preamble lock to.
    acq_reps : int, default 1
        For type=dsss: preamble repetitions (periods of acq_code before the frame).
    data_code : bytes | None, default None
        For type=dsss: the payload spreading code (0/1 chips) — a second code, distinct from acq_code; every frame bit (sync | payload | crc) is XOR-spread across its full length, so len(data_code) is the spreading factor.
    sync : bytes | None, default None
        For type=dsss: the frame-sync word bits (e.g. Barker-13) between the preamble and the payload — what BurstDemod.set_sync correlates to resolve frame position and BPSK polarity. Optional.
    crc : str, default ``"crc16"``
        For type=dsss: the frame trailer — crc16 appends a CRC-16-CCITT over the payload bits (what BurstDemod validates as frame_valid); none omits it.
        One of ``"none"``, ``"crc16"``.
    symbol_rate : float, default 0.0
        For type=dsss: > 0 selects CONTINUOUS asynchronous mode — the spreading code repeats endlessly and data rides on it at this symbol rate (Hz), independent of the code-epoch rate (chips/symbol = fs/sps/symbol_rate, non-integer). No preamble/sync/CRC frame; data comes from the payload when supplied, else a seeded PN a receiver regenerates. Absent/0 = burst.
    dsss_code_only : int, default 0
        Continuous dsss data source: 1 = code-only (the pure spreading code, no data modulation); 0 = data-modulated (the payload when supplied, else the seeded PN). Ignored for burst dsss and non-dsss types.
    fs : float, default 1.0
        Sample rate in Hz — one per segment (all sources share it).
    """
    def __init__(self, type: str = ..., freq: float | tuple[float, float] = ..., snr: float | tuple[float, float] = ..., snr_mode: str = ..., seed: int = ..., sps: int = ..., pn_length: int = ..., pn_poly: int = ..., lfsr: str = ..., level: float | tuple[float, float] = ..., f_end: float | tuple[float, float] = ..., bits: bytes | None = ..., modulation: str = ..., pulse: str = ..., rrc_beta: float = ..., rrc_span: int = ..., symbols: NDArray[np.complex64] | None = ..., acq_code: bytes | None = ..., acq_reps: int = ..., data_code: bytes | None = ..., sync: bytes | None = ..., crc: str = ..., symbol_rate: float = ..., dsss_code_only: int = ..., fs: float = ...) -> None: ...
    def __getattr__(self, name: str) -> Any: ...
    def steps(self, n: int) -> NDArray[np.complex64]:
        """Generate *n* complex samples."""
    def step(self) -> complex:
        """Generate one complex sample."""
    def reset(self) -> None:
        """Reset to initial state."""

class Segment:
    """Segment.

    Parameters
    ----------
    type : str, default ``"tone"``
        Waveform type.
        One of ``"tone"``, ``"noise"``, ``"pn"``, ``"bpsk"``, ``"qpsk"``, ``"chirp"``, ``"bits"``, ``"symbols"``, ``"dsss"``.
    freq : float | tuple[float, float], default 0.0
        Carrier/offset frequency in Hz (normalised cycles/sample when fs=1); for chirp it is the start frequency.
    snr : float | tuple[float, float], default 100.0
        Signal-to-noise ratio in dB, interpreted per snr_mode; >=100 is treated as clean (no AWGN).
    snr_mode : str, default ``"auto"``
        How snr is interpreted: auto picks fs for tone/pn/chirp/bits and Es/No for bpsk/qpsk.
        One of ``"auto"``, ``"fs"``, ``"ebno"``, ``"esno"``.
    seed : int, default 0
        PRNG/LFSR seed for the noise and PN streams.
    sps : int, default 1
        Samples per symbol (PSK) or per chip (PN); the oversampling factor.
    pn_length : int, default 15
        PN LFSR register length; the sequence period is 2^pn_length - 1.
    pn_poly : int, default 0
        PN generator polynomial; 0 auto-selects a maximal-length (MLS) polynomial for pn_length.
    lfsr : str, default ``"galois"``
        PN LFSR realization (galois or fibonacci); same period, different chip order.
        One of ``"galois"``, ``"fibonacci"``.
    level : float | tuple[float, float], default 0.0
        Source power in dBFS (<=0; 0 = unit power). Applies only when summed in a Segment/Composer (gain 10^(level/20)); ignored by standalone Synth.steps().
    f_end : float | tuple[float, float], default 0.0
        Chirp end frequency in Hz; ignored by non-chirp types.
    bits : bytes | None, default None
        For type=bits: the 0/1 pattern, oversampled by sps and cycled to fill the request. For type=dsss (as `payload`): the payload bits of the burst frame.
    modulation : str, default ``"bpsk"``
        For type=bits: symbol mapping of the pattern (none=0/1 amplitude, bpsk, qpsk).
        One of ``"none"``, ``"bpsk"``, ``"qpsk"``.
    pulse : str, default ``"rect"``
        Pulse shape for the symbol stream (pn/bpsk/qpsk/bits): rect sample-and-hold or rrc matched filter.
        One of ``"rect"``, ``"rrc"``.
    rrc_beta : float, default 0.35
        RRC roll-off factor in (0, 1] when pulse=rrc.
    rrc_span : int, default 8
        RRC filter span in symbols when pulse=rrc (taps = 2*span*sps + 1).
    symbols : NDArray[np.complex64] | None, default None
        For type=symbols: a complex64 constellation stream — each element is the output point itself, oversampled by sps, cycled, and RRC-shaped with pulse=rrc. Generalises any modulation (pi/4-QPSK, QAM, ...).
    acq_code : bytes | None, default None
        For type=dsss: the acquisition/preamble code (0/1 chips), repeated acq_reps times unmodulated at the head of the burst — the coherent pull-in target BurstDespreader.set_acq/BurstDemod.set_preamble lock to.
    acq_reps : int, default 1
        For type=dsss: preamble repetitions (periods of acq_code before the frame).
    data_code : bytes | None, default None
        For type=dsss: the payload spreading code (0/1 chips) — a second code, distinct from acq_code; every frame bit (sync | payload | crc) is XOR-spread across its full length, so len(data_code) is the spreading factor.
    sync : bytes | None, default None
        For type=dsss: the frame-sync word bits (e.g. Barker-13) between the preamble and the payload — what BurstDemod.set_sync correlates to resolve frame position and BPSK polarity. Optional.
    crc : str, default ``"crc16"``
        For type=dsss: the frame trailer — crc16 appends a CRC-16-CCITT over the payload bits (what BurstDemod validates as frame_valid); none omits it.
        One of ``"none"``, ``"crc16"``.
    symbol_rate : float, default 0.0
        For type=dsss: > 0 selects CONTINUOUS asynchronous mode — the spreading code repeats endlessly and data rides on it at this symbol rate (Hz), independent of the code-epoch rate (chips/symbol = fs/sps/symbol_rate, non-integer). No preamble/sync/CRC frame; data comes from the payload when supplied, else a seeded PN a receiver regenerates. Absent/0 = burst.
    dsss_code_only : int, default 0
        Continuous dsss data source: 1 = code-only (the pure spreading code, no data modulation); 0 = data-modulated (the payload when supplied, else the seeded PN). Ignored for burst dsss and non-dsss types.
    fs : float, default 1.0
        Sample rate in Hz — one per segment (all sources share it).
    num_samples : int | tuple[int, int], default 1024
        Segment on-time in samples (the active span).
    off_samples : int | tuple[int, int], default 0
        Trailing off-time gap in samples (zeros) appended after the segment.
    repeats : int, default 1
        Play the segment this many times back-to-back (each instance = delay + on-time + trailing gap) before advancing. Ranged fields re-draw and the AWGN is fresh per instance; the signal (codes, payload, PN phase) stays fixed.
    delay_samples : int | tuple[int, int], default 0
        Leading gap before the on-time (samples) — the burst arrives after this delay. Ranged like off_samples and re-drawn per repeats instance, so a (lo, hi) delay is per-burst arrival jitter. Use off_samples for inter-burst spacing, delay_samples for arrival jitter.
    gap_noise : str, default ``"auto"``
        Gap policy for this segment's delay and trailing gap. auto (default): gaps carry the segment's noise floor — the sources' AWGN keeps running while the signal stops (clean scenes still get exact-zero gaps). off: gaps are hard zeros.
        One of ``"auto"``, ``"off"``.
    """
    sources: list[Synth]
    type: str
    freq: float
    snr: float
    snr_mode: str
    seed: int
    sps: int
    pn_length: int
    pn_poly: int
    lfsr: str
    level: float
    f_end: float
    bits: bytes | None
    modulation: str
    pulse: str
    rrc_beta: float
    rrc_span: int
    symbols: NDArray[np.complex64] | None
    acq_code: bytes | None
    acq_reps: int
    data_code: bytes | None
    sync: bytes | None
    crc: str
    symbol_rate: float
    dsss_code_only: int
    def __init__(self, type: str = ..., freq: float | tuple[float, float] = ..., snr: float | tuple[float, float] = ..., snr_mode: str = ..., seed: int = ..., sps: int = ..., pn_length: int = ..., pn_poly: int = ..., lfsr: str = ..., level: float | tuple[float, float] = ..., f_end: float | tuple[float, float] = ..., bits: bytes | None = ..., modulation: str = ..., pulse: str = ..., rrc_beta: float = ..., rrc_span: int = ..., symbols: NDArray[np.complex64] | None = ..., acq_code: bytes | None = ..., acq_reps: int = ..., data_code: bytes | None = ..., sync: bytes | None = ..., crc: str = ..., symbol_rate: float = ..., dsss_code_only: int = ..., fs: float = ..., num_samples: int | tuple[int, int] = ..., off_samples: int | tuple[int, int] = ..., repeats: int = ..., delay_samples: int | tuple[int, int] = ..., gap_noise: str = ...) -> None: ...
    @classmethod
    def sum(cls, *sources: Synth, fs: float = ..., num_samples: int | tuple[int, int] = ..., off_samples: int | tuple[int, int] = ..., repeats: int = ..., delay_samples: int | tuple[int, int] = ..., gap_noise: str = ...) -> Segment:
        """Combine *sources* into a single Segment."""
    def add(self, *others: Segment) -> Timeline:
        """Append segments; return a Timeline."""

class Timeline:
    """Timeline."""
    segments: list[Segment]
    def __init__(self, segments: list[Segment]) -> None: ...
    def add(self, *segments: Segment) -> Timeline:
        """Append and return self."""
    def __iter__(self): ...
    def __len__(self) -> int: ...
    def __getitem__(self, i): ...

class Composer:
    """Composer.

    Parameters
    ----------
    segments : Segment | Timeline | list[Segment] | None, default None
        Initial segment list.
    repeat : bool, default False
        Loop the sequence after the last segment.
    continuous : bool, default False
        Never finish; execute always returns the requested count.
    """
    segments: list[Segment]
    repeat: bool
    continuous: bool
    def __init__(self, segments: Segment | Timeline | list[Segment] | None = ..., *, repeat: bool = ..., continuous: bool = ..., **segment_kwargs) -> None: ...
    def execute(self, n: int) -> NDArray[np.complex64]:
        """Execute for *n* samples."""
    def compose(self, block: int = ...) -> NDArray[np.complex64]:
        """Compose the full sequence into one array."""
    def stream(self, block: int = ..., realtime: float = ...) -> Iterator[NDArray[np.complex64]]:
        """Iterate the sequence in blocks."""
    def to_dict(self) -> dict:
        """Serialise the composer state to a dict."""
    def to_sigmf(self, sample_type: str = ..., endian: str = ..., fs: float = ..., fc: float = ...) -> str:
        """Serialise as to_sigmf."""
    def close(self) -> None:
        """Release native resources."""
    def __enter__(self) -> Composer: ...
    def __exit__(self, *exc) -> None: ...
    @classmethod
    def from_json(cls, json: str) -> Composer: ...
    @classmethod
    def from_file(cls, path: str) -> Composer: ...
    def to_json(self) -> str: ...

def tone(**kw: Any) -> Synth:
    """Return a Synth configured as a *tone* source."""
def noise(**kw: Any) -> Synth:
    """Return a Synth configured as a *noise* source."""
def pn(**kw: Any) -> Synth:
    """Return a Synth configured as a *pn* source."""
def bpsk(**kw: Any) -> Synth:
    """Return a Synth configured as a *bpsk* source."""
def qpsk(**kw: Any) -> Synth:
    """Return a Synth configured as a *qpsk* source."""
def chirp(**kw: Any) -> Synth:
    """Return a Synth configured as a *chirp* source."""
def bits(**kw: Any) -> Synth:
    """Return a Synth configured as a *bits* source."""
