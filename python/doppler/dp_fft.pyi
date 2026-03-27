"""Type stubs for the dp_fft C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

def fft_global_setup(
    shape: tuple[int, ...],
    sign: int = -1,
    nthreads: int = 1,
    planner: str = "estimate",
    wisdom: str = "",
) -> None:
    """Create (or reuse) the global FFT plan.

    Parameters
    ----------
    shape:
        Transform dimensions — ``(N,)`` for 1-D, ``(rows, cols)`` for 2-D.
    sign:
        ``-1`` forward (default), ``+1`` inverse.
    nthreads:
        Thread count (FFTW only; ignored by pocketfft).
    planner:
        FFTW planner effort: ``"estimate"``, ``"measure"``,
        ``"patient"``, or ``"exhaustive"``.  Default ``"estimate"``.

        .. warning::
            Planners other than ``"estimate"`` **overwrite** the input
            buffer during planning.
    wisdom:
        Path to an FFTW wisdom file, or ``""`` to skip wisdom I/O.
    """
    ...

def fft1d_execute(
    x: NDArray[np.complex128],
) -> NDArray[np.complex128]:
    """Out-of-place 1-D FFT.  *x* is not modified."""
    ...

def fft1d_execute_inplace(
    x: NDArray[np.complex128],
) -> None:
    """In-place 1-D FFT.  *x* is overwritten with the transform output."""
    ...

def fft2d_execute(
    x: NDArray[np.complex128],
) -> NDArray[np.complex128]:
    """Out-of-place 2-D FFT.  *x* is not modified."""
    ...

def fft2d_execute_inplace(
    x: NDArray[np.complex128],
) -> None:
    """In-place 2-D FFT.  *x* is overwritten with the transform output."""
    ...
