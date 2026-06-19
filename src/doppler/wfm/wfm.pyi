# wfm/wfm.pyi — type stubs for the wfm C extension.
from typing import Literal
import os
import numpy as np
from numpy.typing import NDArray

class PN:
    """Allocate and initialise a maximal-length-sequence LFSR. The register is seeded from ``seed`` and will produce a pseudo-random binary sequence with period 2^length - 1 for any primitive ``poly``. Both Galois and Fibonacci realizations share the same primitive polynomial and therefore the same period; they differ only in chip ordering/phase.

    Parameters
    ----------
    poly : int, default 96
        poly constructor parameter.
    seed : int, default 1
        seed constructor parameter.
    length : int, default 7
        length constructor parameter.
    lfsr : Literal["galois", "fibonacci"], default "galois"
        lfsr constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.wfm import PN
    >>> obj = PN(poly=96, seed=1, length=7, lfsr="galois")

    """
    def __init__(self, poly: int = ..., seed: int = ..., length: int = ..., lfsr: Literal["galois", "fibonacci"] = "galois") -> None: ...

    def reset(self) -> None:
        """Reset PN to its post-create state. Reloads the LFSR register from the original seed so the sequence restarts from chip 0.  Useful for reproducible captures without re-allocating.

        Examples
        --------
        >>> from doppler.wfm import PN
        >>> import numpy as np
        >>> p = PN(poly=96, seed=1, length=7)
        >>> a = p.generate(8).copy()
        >>> p.reset()
        >>> np.array_equal(a, p.generate(8))
        True

        """

    def generate(self) -> NDArray[np.uint8]:
        """Generate ``n`` chips into ``out`` and advance the LFSR by ``n`` positions.  Each element of ``out`` is 0 or 1.  Requesting more than one MLS period is valid — the sequence simply wraps around.  The Python binding returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy the result before calling generate again if you need a snapshot.

        Returns
        -------
        NDArray[np.uint8]
            ``n`` (the number of chips written; always equal to the request).

        Examples
        --------
        >>> from doppler.wfm import PN
        >>> import numpy as np
        >>> p = PN(poly=96, seed=1, length=7)
        >>> chips = p.generate(127)
        >>> chips[:8].tolist()
        [1, 0, 0, 0, 0, 0, 1, 1]
        >>> int(chips.sum())   # 64 ones per MLS period
        64

        """

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "PN": ...

    def __exit__(self, *args: object) -> None: ...

class _SynthEngine:
    """Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source.  One call to wfm_synth_step() or wfm_synth_steps() advances all sub-components in lock-step. SNR >= WFM_SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead.  When ``snr_mode`` is "auto" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN.

    Parameters
    ----------
    type : Literal["tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits"], default "tone"
        type constructor parameter.
    fs : float, default 1000000.0
        fs constructor parameter.
    freq : float, default 0.0
        freq constructor parameter.
    snr : float, default 100.0
        snr constructor parameter.
    snr_mode : Literal["auto", "fs", "ebno", "esno"], default "auto"
        snr_mode constructor parameter.
    seed : int, default 1
        seed constructor parameter.
    sps : int, default 8
        sps constructor parameter.
    pn_length : int, default 7
        pn_length constructor parameter.
    pn_poly : int, default 0
        pn_poly constructor parameter.
    lfsr : Literal["galois", "fibonacci"], default "galois"
        lfsr constructor parameter.
    f_end : float, default 0.0
        f_end constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.wfm import _SynthEngine
    >>> obj = _SynthEngine(type="tone", fs=1000000.0, freq=0.0, snr=100.0, snr_mode="auto", seed=1, sps=8, pn_length=7, pn_poly=0, lfsr="galois", f_end=0.0)
    >>> obj.get_wtype()
    0
    >>> obj.get_nsps()
    8
    >>> obj.get_sym_pos()
    0

    """
    def __init__(self, wtype: int = ..., nsps: int = ..., sym_pos: int = ..., cur_re: float = ..., cur_im: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset Synth to its post-create state. Resets the LO phase accumulator, AWGN internal state, and PN LFSR register to their initial values so the output sequence is perfectly reproducible from sample 0.

        Examples
        --------
        >>> from doppler.wfm import _SynthEngine
        >>> import numpy as np
        >>> s = _SynthEngine(type="qpsk", sps=4, seed=1, snr=100.0)
        >>> a = s.steps(16).copy()
        >>> s.reset()
        >>> np.array_equal(a, s.steps(16))
        True

        """

    def step(self) -> complex:
        """Generate one output sample."""

    def steps(self, n: int) -> NDArray[np.complex64]:
        """Generate a block of output samples. Calls wfm_synth_step() in a tight loop, writing each cf32 sample into ``output``.  The Python binding returns a freshly allocated NumPy complex64 array; ownership is transferred to the caller.

        Parameters
        ----------
        n : int
            Number of samples to generate.

        Returns
        -------
        NDArray[np.complex64]
            Output.

        Examples
        --------
        >>> from doppler.wfm import _SynthEngine
        >>> import numpy as np
        >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
        >>> x = s.steps(4)
        >>> x.shape, x.dtype
        ((4,), dtype('complex64'))
        >>> x.tolist()
        [(1+0j), (1+0j), (1+0j), (1+0j)]

        """

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "_SynthEngine": ...

    def __exit__(self, *args: object) -> None: ...

def bpsk_map(bits: NDArray[np.uint8]) -> NDArray[np.complex64]:
    """Map bits {0,1} to BPSK symbols {+1,-1} (cf32)."""

def qpsk_map(syms: NDArray[np.uint8]) -> NDArray[np.complex64]:
    """Map QPSK symbol indices {0,1,2,3} to Gray-coded symbols (cf32)."""

def wfm_awgn_amplitude(snr_db: float, signal_power: float) -> float:
    """AWGN amplitude for a target SNR (dB, over fs) given signal power."""

def wfm_ebno_to_snr_db(ebno_db: float, bits_per_symbol: int, samples_per_symbol: float) -> float:
    """Convert Eb/No (dB) to SNR (dB over fs)."""

def mls_poly(n: int) -> int:
    """Maximal-length-sequence primitive polynomial for an LFSR of length n."""

def rrc_taps(beta: float, sps: int, span: int) -> NDArray[np.float32]:
    """Root-raised-cosine pulse-shaping taps (2*span*sps+1 unit-energy cf32 taps)."""

def dsss_spread(syms: NDArray[np.complex64], code: NDArray[np.uint8], sf: int) -> NDArray[np.complex64]:
    """Direct-sequence spread syms by the ±1 chip code; yields len(syms)*sf chips."""

def write_blue_header(path: str | os.PathLike, total: int, sample_type: str = 'cf32', endian: str = 'le', fs: float = 1e6, fc: float = 0.0, data_start: float = 0.0, detached: int = 1) -> None:
    """Write blue header."""
