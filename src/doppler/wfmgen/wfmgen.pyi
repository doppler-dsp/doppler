# wfmgen/wfmgen.pyi — type stubs for the wfmgen C extension.
import numpy as np
from numpy.typing import NDArray

class PN:
    """PN component.

    Parameters
    ----------
    poly : int, default 96
        poly constructor parameter.
    seed : int, default 1
        seed constructor parameter.
    length : int, default 7
        length constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.wfmgen import PN
    >>> obj = PN(96, 1, 7)

    """
    def __init__(self, poly: int = ..., seed: int = ..., length: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def generate(self) -> NDArray[np.uint8]:
        """Generate."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "PN": ...

    def __exit__(self, *args: object) -> None: ...

class Synth:
    """Synth component.

    Parameters
    ----------
    type : int, default 0
        type constructor parameter.
    fs : float, default 1000000.0
        fs constructor parameter.
    freq_offset : float, default 0.0
        freq_offset constructor parameter.
    snr_db : float, default 100.0
        snr_db constructor parameter.
    seed : int, default 1
        seed constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.wfmgen import Synth
    >>> obj = Synth(0, 1000000.0, 0.0, 100.0, 1)

    """
    def __init__(self, type: int = ..., fs: float = ..., freq_offset: float = ..., snr_db: float = ..., seed: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self) -> complex:
        """Generate one output sample."""

    def steps(self, n: int) -> NDArray[np.complex64]:
        """Generate n output samples."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Synth": ...

    def __exit__(self, *args: object) -> None: ...

def bpsk_map(bits: NDArray[np.uint8]) -> NDArray[np.complex64]:
    """Map bits {0,1} to BPSK symbols {+1,-1} (cf32)."""

def qpsk_map(syms: NDArray[np.uint8]) -> NDArray[np.complex64]:
    """Map QPSK symbol indices {0,1,2,3} to Gray-coded symbols (cf32)."""

def wfm_awgn_amplitude(snr_db: float, signal_power: float) -> float:
    """AWGN amplitude for a target SNR (dB, over fs) given signal power."""

def wfm_ebno_to_snr_db(ebno_db: float, bits_per_symbol: int, samples_per_symbol: float) -> float:
    """Convert Eb/No (dB) to SNR (dB over fs)."""
