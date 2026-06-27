# mpsk/mpsk.pyi — type stubs for the mpsk C extension.
import numpy as np
from numpy.typing import NDArray
def mpsk_map(sym: NDArray[np.uint8], m: int = 4) -> NDArray[np.complex64]:
    """Map Gray-coded M-PSK labels to unit-amplitude constellation points.

    Element-wise inverse of mpsk_demap(): each input byte is one symbol's
    log2(M) Gray-coded bits (0..M-1), each output is its cf32 point.
    Memoryless (absolute phase). out must hold sym_len points.

    Parameters
    ----------
    sym : NDArray[np.uint8]
        Gray label bytes (0..M-1), one per symbol.
    m : int
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

def mpsk_demap(x: NDArray[np.complex64], m: int = 4) -> NDArray[np.uint8]:
    """Hard-decide M-PSK symbols to their Gray-coded label bytes.

    Element-wise inverse of mpsk_map(): each cf32 symbol is sliced to the
    nearest constellation point and its Gray label (0..M-1) is written out.
    A slip to an adjacent point flips exactly one bit (Gray). out must hold
    x_len bytes.

    Parameters
    ----------
    x : NDArray[np.complex64]
        Received symbols (any amplitude; phase only).
    m : int
        M in {2,4,8}.

    Returns
    -------
    NDArray[np.uint8]
        Output.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.mpsk import mpsk_demap
    >>> x = np.array([1+0j, 1j, -1+0j, -1j], dtype=np.complex64)   # 8PSK points
    >>> mpsk_demap(x, 8).tolist()   # Gray labels of indices 0, 2, 4, 6
    [0, 3, 6, 5]

    """

def mpsk_diff_map(sym: NDArray[np.uint8], m: int = 4) -> NDArray[np.complex64]:
    """Differential M-PSK map: the label selects a phase INCREMENT.

    Information rides on phase *differences*: the running constellation
    index accumulates `gray_decode(label)` each symbol (starting from an
    implicit zero-phase reference), so an unknown constant carrier phase
    cancels at the receiver (mpsk_diff_demap) — resolving the M-fold
    ambiguity, at ~2x the symbol-error rate of coherent map(). Sequential
    over the array.

    Parameters
    ----------
    sym : NDArray[np.uint8]
        Gray label bytes (0..M-1), one per symbol.
    m : int
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
    >>> rot = (pts * np.exp(1j * np.pi / 2)).astype(np.complex64)  # 90 deg slip
    >>> np.array_equal(mpsk_diff_demap(rot, 4)[1:], sym[1:])   # rotation-invariant
    True

    """

def mpsk_diff_demap(x: NDArray[np.complex64], m: int = 4) -> NDArray[np.uint8]:
    """Differential M-PSK demap: decide from the phase DIFFERENCE.

    Inverse of mpsk_diff_map(): the Gray label of each symbol is decided
    from the phase difference between consecutive sliced indices (the first
    references an implicit zero-phase start). Invariant to an unknown
    constant carrier phase.

    Parameters
    ----------
    x : NDArray[np.complex64]
        Received symbols (any amplitude; phase only).
    m : int
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

def mpsk_bits_per_symbol(m: int = 4) -> int:
    """Bits per M-PSK symbol = log2(M).

    Parameters
    ----------
    m : int
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
