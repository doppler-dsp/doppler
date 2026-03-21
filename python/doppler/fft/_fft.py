"""
doppler.fft — Fast Fourier Transform interface.

Wraps the C ``dp_fft_*`` functions exposed by the ``_doppler``
Python extension module.  A single *global plan* is maintained inside the
C layer; call :func:`setup` (or the one-shot :func:`fft`) before executing
any transform.

Supported backends
------------------
* **PocketFFT** — bundled header-only library, always available.
* **FFTW3** — used when the library is compiled with FFTW support.
  Enables planner effort levels and wisdom save/load.

Examples
--------
>>> import numpy as np
>>> import doppler.fft as nfft
>>>
>>> x = np.random.randn(1024).astype(np.complex128)
>>>
>>> # One-shot (setup + execute):
>>> X = nfft.fft(x)
>>>
>>> # Or explicit setup then execute:
>>> nfft.setup((1024,))
>>> X = nfft.execute1d(x)
"""

from ..dp_fft import (
    fft_global_setup,
    fft1d_execute,
    fft1d_execute_inplace,
    fft2d_execute,
    fft2d_execute_inplace,
)


# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------


def setup(shape, sign=-1, nthreads=1, planner="estimate", wisdom=None):
    """Set up (or reuse) the global FFT plan for the given shape.

    Plans are cached by shape inside the C layer; subsequent calls with
    the same shape are effectively free.

    Parameters
    ----------
    shape : tuple of int
        Dimensions of the transform.  Use ``(N,)`` for a 1-D FFT of length
        *N*, or ``(rows, cols)`` for a 2-D FFT.
    sign : int, optional
        Transform direction.  ``-1`` (default) is the forward FFT
        (``e^{-2πi·k·n/N}``); ``+1`` is the inverse FFT.
    nthreads : int, optional
        Number of threads to use.  Only effective when the library is built
        against FFTW3; PocketFFT is single-threaded.  Default ``1``.
    planner : str, optional
        FFTW planner effort level.  One of ``"estimate"``, ``"measure"``,
        ``"patient"``, or ``"exhaustive"``.  Ignored by PocketFFT.
        Default ``"estimate"``.
    wisdom : str or None, optional
        Path to a file used to load/save FFTW wisdom.  Pass ``None`` (default)
        to skip wisdom I/O.


    Warning
    -------
    **High-effort planners overwrite data!**
    Heavier planners (``"measure"``, ``"patient"``, etc.) work by timing
    transforms on your actual buffers. This process **erases** the input
    array. For one-shot transforms on live data, always use the default
    ``"estimate"`` planner.


    Examples
    --------
    >>> import doppler.fft as nfft
    >>> nfft.setup((1024,))               # 1-D plan, forward FFT
    >>> nfft.setup((64, 64), sign=-1)     # 2-D forward plan
    >>> nfft.setup((1024,), planner="measure", wisdom="/tmp/fftw.wisdom")
    """
    shape = tuple(int(s) for s in shape)
    fft_global_setup(shape, int(sign), int(nthreads), str(planner), wisdom or "")


# ---------------------------------------------------------------------------
# Execute — 1-D
# ---------------------------------------------------------------------------


def execute1d(x):
    """Execute an out-of-place 1-D FFT.

    Uses the plan established by the most recent :func:`setup` call with a
    1-D shape.  The input array is not modified.

    Parameters
    ----------
    x : np.ndarray
        1-D array of ``complex128`` (or any dtype that can be safely cast to
        ``complex128`` by the C extension).

    Returns
    -------
    np.ndarray
        1-D ``complex128`` output array of the same length as *x*.

    Examples
    --------
    >>> import numpy as np
    >>> import doppler.fft as nfft
    >>> nfft.setup((8,))
    >>> X = nfft.execute1d(np.array([1, 0, 0, 0, 0, 0, 0, 0], dtype=complex))
    """
    return fft1d_execute(x)


def execute1d_inplace(x):
    """Execute a 1-D FFT in-place, overwriting the input array.

    Parameters
    ----------
    x : np.ndarray
        1-D ``complex128`` array.  **Modified in place.**

    Returns
    -------
    np.ndarray
        The same array *x*, now containing the FFT output.

    Examples
    --------
    >>> import numpy as np
    >>> import doppler.fft as nfft
    >>> nfft.setup((8,))
    >>> x = np.array([1, 0, 0, 0, 0, 0, 0, 0], dtype=complex)
    >>> nfft.execute1d_inplace(x)   # x is modified
    array([1.+0.j, ...])
    """
    fft1d_execute_inplace(x)
    return x


# ---------------------------------------------------------------------------
# Execute — 2-D
# ---------------------------------------------------------------------------


def execute2d(x):
    """Execute an out-of-place 2-D FFT.

    Uses the plan established by the most recent :func:`setup` call with a
    2-D shape.  The input array is not modified.

    Parameters
    ----------
    x : np.ndarray
        2-D array of ``complex128`` in row-major (C) order.

    Returns
    -------
    np.ndarray
        2-D ``complex128`` output array of the same shape as *x*.

    Examples
    --------
    >>> import numpy as np
    >>> import doppler.fft as nfft
    >>> nfft.setup((64, 64))
    >>> X = nfft.execute2d(np.zeros((64, 64), dtype=complex))
    """
    return fft2d_execute(x)


def execute2d_inplace(x):
    """Execute a 2-D FFT in-place, overwriting the input array.

    Parameters
    ----------
    x : np.ndarray
        2-D ``complex128`` array in row-major order.  **Modified in place.**

    Returns
    -------
    np.ndarray
        The same array *x*, now containing the 2-D FFT output.

    Examples
    --------
    >>> import numpy as np
    >>> import doppler.fft as nfft
    >>> nfft.setup((64, 64))
    >>> x = np.zeros((64, 64), dtype=complex)
    >>> nfft.execute2d_inplace(x)
    array([[0.+0.j, ...]])
    """
    fft2d_execute_inplace(x)
    return x


# ---------------------------------------------------------------------------
# Dispatcher — shape-agnostic execute
# ---------------------------------------------------------------------------


def execute(x):
    """Execute a 1-D or 2-D FFT out-of-place (shape-dispatched).

    Calls :func:`execute1d` or :func:`execute2d` based on ``x.ndim``.
    A matching plan must have been set up with :func:`setup` beforehand.

    Parameters
    ----------
    x : np.ndarray
        1-D or 2-D ``complex128`` array.

    Returns
    -------
    np.ndarray
        FFT output array of the same shape as *x*.

    Raises
    ------
    ValueError
        If ``x.ndim`` is not 1 or 2.
    """
    if x.ndim == 1:
        return fft1d_execute(x)
    elif x.ndim == 2:
        return fft2d_execute(x)
    raise ValueError("Only 1D and 2D FFTs supported")


def execute_inplace(x):
    """Execute a 1-D or 2-D FFT in-place (shape-dispatched).

    Calls :func:`execute1d_inplace` or :func:`execute2d_inplace` based on
    ``x.ndim``.  **Modifies x in place.**

    Parameters
    ----------
    x : np.ndarray
        1-D or 2-D ``complex128`` array.  **Modified in place.**

    Returns
    -------
    np.ndarray
        The same array *x* after the transform.

    Raises
    ------
    ValueError
        If ``x.ndim`` is not 1 or 2.
    """
    if x.ndim == 1:
        fft1d_execute_inplace(x)
    elif x.ndim == 2:
        fft2d_execute_inplace(x)
    else:
        raise ValueError("Only 1D and 2D FFTs supported")
    return x


# ---------------------------------------------------------------------------
# One-shot convenience
# ---------------------------------------------------------------------------


def fft(x, sign=-1, nthreads=1, planner="estimate", wisdom=None):
    """Set up and execute a 1-D or 2-D forward FFT in a single call.

    Equivalent to calling :func:`setup` followed by :func:`execute`.
    Useful for one-off transforms; for repeated transforms on the same shape,
    prefer calling :func:`setup` once and :func:`execute` in a loop.

    Parameters
    ----------
    x : np.ndarray
        1-D or 2-D ``complex128`` input array.
    sign : int, optional
        ``-1`` (default) for forward FFT, ``+1`` for inverse.
    nthreads : int, optional
        Thread count (FFTW only).  Default ``1``.
    planner : str, optional
        FFTW planner effort.  Default ``"estimate"``.
    wisdom : str or None, optional
        FFTW wisdom file path, or ``None``.

    Returns
    -------
    np.ndarray
        FFT output array of the same shape as *x*.

    Raises
    ------
    ValueError
        If ``x.ndim`` is not 1 or 2.

    Examples
    --------
    >>> import numpy as np
    >>> import doppler.fft as nfft
    >>>
    >>> # 1-D FFT of a 1024-point signal:
    >>> x = np.random.randn(1024).astype(complex)
    >>> X = nfft.fft(x)
    >>>
    >>> # 2-D FFT of a 64×64 image:
    >>> img = np.random.randn(64, 64).astype(complex)
    >>> IMG = nfft.fft(img)
    """
    setup(x.shape, sign=sign, nthreads=nthreads, planner=planner, wisdom=wisdom)
    if x.ndim == 1:
        return fft1d_execute(x)
    elif x.ndim == 2:
        return fft2d_execute(x)
    raise ValueError("Only 1D and 2D FFTs supported")
