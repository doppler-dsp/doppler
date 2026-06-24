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
        One of ``"tone"``, ``"noise"``, ``"pn"``, ``"bpsk"``, ``"qpsk"``, ``"chirp"``, ``"bits"``.
    freq : float, default 0.0
    snr : float, default 100.0
    snr_mode : str, default ``"auto"``
        One of ``"auto"``, ``"fs"``, ``"ebno"``, ``"esno"``.
    seed : int, default 0
    sps : int, default 1
    pn_length : int, default 15
    pn_poly : int, default 0
    lfsr : str, default ``"galois"``
        One of ``"galois"``, ``"fibonacci"``.
    level : float, default 0.0
    f_end : float, default 0.0
    bits : bytes | None, default None
    modulation : str, default ``"bpsk"``
        One of ``"none"``, ``"bpsk"``, ``"qpsk"``.
    pulse : str, default ``"rect"``
        One of ``"rect"``, ``"rrc"``.
    rrc_beta : float, default 0.35
    rrc_span : int, default 8
    fs : float, default 1.0
    """
    def __init__(self, type: str = ..., freq: float = ..., snr: float = ..., snr_mode: str = ..., seed: int = ..., sps: int = ..., pn_length: int = ..., pn_poly: int = ..., lfsr: str = ..., level: float = ..., f_end: float = ..., bits: bytes | None = ..., modulation: str = ..., pulse: str = ..., rrc_beta: float = ..., rrc_span: int = ..., fs: float = ...) -> None: ...
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
        One of ``"tone"``, ``"noise"``, ``"pn"``, ``"bpsk"``, ``"qpsk"``, ``"chirp"``, ``"bits"``.
    freq : float, default 0.0
    snr : float, default 100.0
    snr_mode : str, default ``"auto"``
        One of ``"auto"``, ``"fs"``, ``"ebno"``, ``"esno"``.
    seed : int, default 0
    sps : int, default 1
    pn_length : int, default 15
    pn_poly : int, default 0
    lfsr : str, default ``"galois"``
        One of ``"galois"``, ``"fibonacci"``.
    level : float, default 0.0
    f_end : float, default 0.0
    bits : bytes | None, default None
    modulation : str, default ``"bpsk"``
        One of ``"none"``, ``"bpsk"``, ``"qpsk"``.
    pulse : str, default ``"rect"``
        One of ``"rect"``, ``"rrc"``.
    rrc_beta : float, default 0.35
    rrc_span : int, default 8
    fs : float, default 1.0
    num_samples : int, default 1024
    off_samples : int, default 0
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
    def __init__(self, type: str = ..., freq: float = ..., snr: float = ..., snr_mode: str = ..., seed: int = ..., sps: int = ..., pn_length: int = ..., pn_poly: int = ..., lfsr: str = ..., level: float = ..., f_end: float = ..., bits: bytes | None = ..., modulation: str = ..., pulse: str = ..., rrc_beta: float = ..., rrc_span: int = ..., fs: float = ..., num_samples: int = ..., off_samples: int = ...) -> None: ...
    @classmethod
    def sum(cls, *sources: Synth, fs: float = ..., num_samples: int = ..., off_samples: int = ...) -> Segment:
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
