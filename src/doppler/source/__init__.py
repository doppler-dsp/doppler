"""Signal sources: sample generators comprising a 32-bit phase-accumulator NCO (NCO), a quadrature local oscillator (LO), and an AWGN generator (AWGN).

- `LO` emits CF32 phasors from a 2^16-entry sine table: SFDR at least
  90 dBc at any frequency (about 96 dBc is typical), phase-continuous from
  one call to the next and across a retune.
- `NCO` is the raw uint32 phase accumulator underneath, with the overflow
  carry that drives a sample clock and a fixed-point `[0, nmax)` scaling.
- `AWGN` is complex Gaussian noise, `amplitude` per component; one seed is
  one noise sequence on every platform, and `reset` replays it.

Sources generate a signal; `doppler.impairment` transforms one.

Examples
--------
>>> from doppler.source import NCO
>>> NCO(norm_freq=0.25).steps_u32(4).tolist()
[0, 1073741824, 2147483648, 3221225472]

The overflow carry marks each wrap, every 4 samples at Fs/4:

>>> NCO(0.25).steps_u32_ovf(8)[1].tolist()
[0, 0, 0, 1, 0, 0, 0, 1]

Two blocks from an LO are one stream, and a reset AWGN replays its noise:

>>> import numpy as np
>>> from doppler.source import LO, AWGN
>>> a, b = LO(0.25), LO(0.25)
>>> bool(np.array_equal(np.concatenate([a.steps(4), a.steps(4)]), b.steps(8)))
True
>>> g = AWGN(seed=42, amplitude=1.0)
>>> first = g.generate(8)
>>> g.reset()
>>> bool(np.array_equal(g.generate(8), first))
True

"""

# source/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .source import AWGN, LO, NCO  # noqa: E402


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
