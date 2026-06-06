# wfmgen/wfmgen.pyi — type stubs for the wfmgen C extension.
from typing import Literal
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
    wtype : int, default 0
        wtype state variable.
    nsps : int, default 8
        nsps state variable.
    sym_pos : int, default 0
        sym_pos state variable.
    cur_re : float, default 1.0
        cur_re state variable.
    cur_im : float, default 0.0
        cur_im state variable.

    Examples
    --------
    Create with defaults:

    >>> from doppler.wfmgen import Synth
    >>> obj = Synth(0, 8, 0, 1.0, 0.0)
    >>> obj.get_wtype()
    0
    >>> obj.get_nsps()
    8
    >>> obj.get_sym_pos()
    0

    Reset restores defaults:

    >>> obj.set_wtype(42)
    >>> obj.reset()
    >>> obj.get_wtype()
    0

    """
    def __init__(self, wtype: int = ..., nsps: int = ..., sym_pos: int = ..., cur_re: float = ..., cur_im: float = ...) -> None: ...

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
