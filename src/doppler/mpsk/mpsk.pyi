# mpsk/mpsk.pyi — type stubs for the mpsk C extension.
import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

def mpsk_map(
    sym: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    m: int = 4,
) -> NDArray[np.complex64]:
    """Map Gray-coded M-PSK labels to unit-amplitude constellation points.

    Element-wise inverse of dp_mpsk_demap(): each input byte is one
    symbol's log2(M) Gray-coded bits (0..M-1), each output is its cf32
    point. Memoryless (absolute phase). out must hold sym_len points.

    Parameters
    ----------
    sym : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
        Gray label bytes (0..M-1), one per symbol.
    m : int, default 4
        M in {2,4,8}.

    Returns
    -------
    NDArray[np.complex64]
        Output.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import mpsk_map, mpsk_demap
    >>> sym = np.array([0, 1, 2, 3], dtype=np.uint8)   # QPSK labels
    >>> pts = mpsk_map(sym, 4)
    >>> np.round(np.abs(pts), 5)
    array([1., 1., 1., 1.], dtype=float32)
    >>> np.array_equal(mpsk_demap(pts, 4), sym)
    True

    """

def mpsk_demap(x: npt.NDArray[np.complex64], m: int = 4) -> NDArray[np.uint8]:
    """Hard-decide M-PSK symbols to their Gray-coded label bytes.

    Element-wise inverse of dp_mpsk_map(): each cf32 symbol is sliced to
    the nearest constellation point and its Gray label (0..M-1) is written
    out. A slip to an adjacent point flips exactly one bit (Gray). out must
    hold x_len bytes.

    Parameters
    ----------
    x : npt.NDArray[np.complex64]
        Received symbols (any amplitude; phase only).
    m : int, default 4
        M in {2,4,8}.

    Returns
    -------
    NDArray[np.uint8]
        Output.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import mpsk_demap
    >>> x = np.array([1+0j, 1j, -1+0j, -1j], dtype=np.complex64)  # 8PSK
    >>> mpsk_demap(x, 8).tolist()   # Gray labels of indices 0, 2, 4, 6
    [0, 3, 6, 5]

    """

def mpsk_diff_map(
    sym: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    m: int = 4,
) -> NDArray[np.complex64]:
    """Differential M-PSK map: the label selects a phase INCREMENT.

    Information rides on phase *differences*: the running constellation
    index accumulates `gray_decode(label)` each symbol (starting from an
    implicit zero-phase reference), so an unknown constant carrier phase
    cancels at the receiver (mpsk_diff_demap) — resolving the M-fold
    ambiguity. Sequential over the array.

    The cost is up to **2x the symbol-error rate** of coherent map(). That
    factor is a high-SNR asymptote, not a constant: measured, BPSK and QPSK
    reach it by ~8 dB Es/N0 while 8PSK pays only 1.44x at 4 dB and 2.03x by
    14 dB. A caller sizing a link at low Es/N0 is charged less than the
    round number suggests (native/validation/mpsk_diff_penalty.c).

    Parameters
    ----------
    sym : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
        Gray label bytes (0..M-1), one per symbol.
    m : int, default 4
        M in {2,4,8}.

    Returns
    -------
    NDArray[np.complex64]
        Output.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import mpsk_diff_map, mpsk_diff_demap
    >>> sym = np.array([1, 0, 3, 2, 1], dtype=np.uint8)
    >>> pts = mpsk_diff_map(sym, 4)
    >>> np.array_equal(mpsk_diff_demap(pts, 4), sym)   # exact round-trip
    True
    >>> rot = (pts * np.exp(1j * np.pi / 2)).astype(np.complex64)  # slip
    >>> np.array_equal(mpsk_diff_demap(rot, 4)[1:], sym[1:])  # invariant
    True

    """

def mpsk_diff_demap(
    x: npt.NDArray[np.complex64],
    m: int = 4,
) -> NDArray[np.uint8]:
    """Differential M-PSK demap: decide from the phase DIFFERENCE.

    Inverse of dp_mpsk_diff_map(): the Gray label of each symbol is decided
    from the phase difference between consecutive sliced indices (the first
    references an implicit zero-phase start). Invariant to an unknown
    constant carrier phase.

    Parameters
    ----------
    x : npt.NDArray[np.complex64]
        Received symbols (any amplitude; phase only).
    m : int, default 4
        M in {2,4,8}.

    Returns
    -------
    NDArray[np.uint8]
        Output.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import mpsk_diff_demap, mpsk_diff_map
    >>> sym = np.array([2, 2, 1, 0], dtype=np.uint8)
    >>> np.array_equal(mpsk_diff_demap(mpsk_diff_map(sym, 8), 8), sym)
    True

    """

def mpsk_soft_demap(
    x: npt.NDArray[np.complex64],
    llr: npt.NDArray[np.float32],
    m: int = 4,
    n0: float = 1.0,
) -> None:
    """Soft-demap M-PSK symbols to per-bit log-likelihood ratios.

    The soft counterpart of dp_mpsk_demap(): instead of one label byte per
    symbol it writes `log2(M)` LLRs, one per bit, which is what a
    soft-input decoder (a Viterbi, for the CCSDS inner code) needs. A hard
    decision throws away roughly 2 dB of the coding gain such a decoder
    exists to deliver.

    The convention, which every consumer has to agree with:

    L_i = log( P(bit i = 0 | y) / P(bit i = 1 | y) )

    so **positive means bit 0** and the hard decision is `L < 0`. That is
    not a separate rule: `dp_mpsk_demap()` is what this reproduces, and the
    sign agreeing with it at every M and every SNR is asserted in
    test_mpsk_core.c rather than assumed. The repository has ONE decision
    rule; this is a second view of it, not a second copy.

    Bits are LSB-first within a symbol, matching how the Gray label packs
    them, and symbols run in order: `llr[i * log2(M) + b]` is bit b of
    symbol i.

    Computed by the max-log rule over the constellation `L_i = (min_{b_i=1}
    |y-a|^2 - min_{b_i=0} |y-a|^2) / n0`. For BPSK and QPSK this is EXACT —
    QPSK's `phi0 = pi/4` grid is axis-separable, so its two bits are
    independent BPSK decisions and each subset holds one point. Only 8PSK
    is an approximation; what that costs in dB is not measured yet and is
    therefore not claimed here (docs/design/mpsk.md §9.7).

    n0 is the noise power `E[|n|^2]` for unit-amplitude symbols, and it
    scales the output exactly: `L(n0) = L(1) / n0`. A **Viterbi is
    invariant to it**, since scaling every branch metric by a positive
    constant cannot move the maximum-likelihood path — so a caller with no
    SNR estimate may pass 1.0 and get correctly ordered, unscaled soft
    values.

    Parameters
    ----------
    x : npt.NDArray[np.complex64]
        Received symbols (amplitude matters here — unlike the hard path,
        which uses phase only).
    llr : npt.NDArray[np.float32]
        Out: x_len * log2(M) LLRs.
    m : int, default 4
        M in {2,4,8}.
    n0 : float, default 1.0
        Noise power `E[|n|^2]`; must be positive.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import mpsk_soft_demap, mpsk_demap
    >>> x = np.array([0.9+0.1j, -0.8-0.2j], dtype=np.complex64)   # BPSK
    >>> llr = np.empty(2, dtype=np.float32)
    >>> mpsk_soft_demap(x, llr, 2, 1.0)
    >>> np.round(llr, 3)                       # 4*Re(y)/n0
    array([ 3.6, -3.2], dtype=float32)
    >>> np.array_equal((llr < 0).astype(np.uint8), mpsk_demap(x, 2))
    True

    """

def mpsk_bits_per_symbol(m: int = 4) -> int:
    """Bits per M-PSK symbol = log2(M).

    Parameters
    ----------
    m : int, default 4
        M in {2,4,8}.

    Returns
    -------
    int
        1, 2, or 3 (0 for an unsupported M).

    Examples
    --------
    >>> from doppler.mpsk import mpsk_bits_per_symbol
    >>> [mpsk_bits_per_symbol(m) for m in (2, 4, 8)]
    [1, 2, 3]

    """

def qam_map(sym: npt.NDArray[np.uint16], m: int = 16) -> NDArray[np.complex64]:
    """Map QAM labels to unit-average-energy constellation points.

    Element-wise inverse of dp_qam_demap(): each input is one symbol's
    `log2(M)` bits packed LSB-first in transmission order, each output its
    cf32 point. out must hold sym_len points. For an unsupported m every
    output is 0, which is not a constellation point of any supported M, so
    check dp_qam_bits_per_symbol() first.

    Parameters
    ----------
    sym : npt.NDArray[np.uint16]
        Labels (0 .. M-1), one per symbol, 16-bit.
    m : int, default 16
        M in {4, 16, 64, 256, 1024}.

    Returns
    -------
    NDArray[np.complex64]
        Output.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import qam_map, qam_demap
    >>> sym = np.array([0, 1, 3, 2], dtype=np.uint16)   # 16-QAM, I axis
    >>> np.round(qam_map(sym, 16).real * np.sqrt(10), 3)
    array([ 3., -3., -1.,  1.])
    >>> np.array_equal(qam_demap(qam_map(sym, 16), 16), sym)
    True

    """

def qam_demap(x: npt.NDArray[np.complex64], m: int = 16) -> NDArray[np.uint16]:
    """Hard-decide QAM symbols to their labels.

    Element-wise inverse of dp_qam_map(). Each axis is decided on its own
    to the nearest level, so a received point outside the constellation
    lands on the nearest edge point. For an unsupported m every label is 0.

    Parameters
    ----------
    x : npt.NDArray[np.complex64]
        Received symbols; amplitude matters.
    m : int, default 16
        M in {4, 16, 64, 256, 1024}.

    Returns
    -------
    NDArray[np.uint16]
        Output.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import qam_demap
    >>> v = np.array([3.2, -0.9, 0.2, -2.7]) / np.sqrt(10)
    >>> x = (v + 1j * v[::-1]).astype(np.complex64)
    >>> qam_demap(x, 16).tolist()
    [4, 11, 14, 1]

    """

def qam_bits_per_symbol(m: int = 16) -> int:
    """Bits per QAM symbol = log2(M).

    Parameters
    ----------
    m : int, default 16
        M in {4, 16, 64, 256, 1024}.

    Returns
    -------
    int
        2, 4, 6, 8 or 10 (0 for an unsupported M).

    Examples
    --------
    >>> from doppler.mpsk import qam_bits_per_symbol
    >>> [qam_bits_per_symbol(m) for m in (4, 16, 64, 256, 1024)]
    [2, 4, 6, 8, 10]

    """
