# source/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .source import NCO, LO, AWGN  # noqa: E402


def awgn(n, amplitude=1.0, seed=0):
    """Generate *n* complex CF32 AWGN samples.

    Functional wrapper around :class:`AWGN`.  Creates a generator,
    produces the samples, and discards the state — use :class:`AWGN`
    directly when you need phase-continuous streams or reproducible
    multi-call replay.

    Parameters
    ----------
    n : int
        Number of complex output samples.
    amplitude : float, optional
        Per-component (Re, Im) standard deviation.  Default 1.0.
    seed : int, optional
        64-bit RNG seed.  Default 0.

    Returns
    -------
    NDArray[np.complex64]
        Shape ``(n,)``, dtype ``complex64``.

    Examples
    --------
    >>> from doppler.source import awgn
    >>> import numpy as np
    >>> noise = awgn(1024)
    >>> noise.shape, noise.dtype
    ((1024,), dtype('complex64'))
    >>> abs(np.std(np.real(noise)) - 1.0) < 0.1
    True
    >>> awgn(256, amplitude=0.5, seed=7).shape
    (256,)
    """
    return AWGN(seed, amplitude).generate(n)


__all__ = ["NCO", "LO", "AWGN"]
