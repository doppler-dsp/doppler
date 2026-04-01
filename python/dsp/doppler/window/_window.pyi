import numpy as np
from numpy.typing import NDArray

def kaiser_window(n: int, beta: float) -> NDArray[np.float32]:
    """
    Return a Kaiser window of length *n* with shape parameter *beta*.

    Parameters
    ----------
    n : int
        Window length.  Must be >= 1.
    beta : float
        Shape parameter (direct I0 argument, NumPy/SciPy convention).
        0 gives a rectangular window; larger values increase side-lobe
        suppression at the cost of a wider main lobe.

    Returns
    -------
    w : ndarray, dtype=float32, shape=(n,)
        Window coefficients.  Centre sample equals 1.0.
    """
    ...

def kaiser_enbw(w: NDArray[np.float32]) -> float:
    """
    Compute the equivalent noise bandwidth (ENBW) of a window.

    Parameters
    ----------
    w : array-like, dtype=float32
        Window coefficients.

    Returns
    -------
    enbw : float
        ENBW in FFT bins.  Multiply by ``fs / len(w)`` to convert to Hz.
    """
    ...
