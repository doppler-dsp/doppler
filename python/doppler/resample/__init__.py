"""doppler.resample — continuously-variable polyphase resampler.

Three implementations are provided:

* :class:`Resampler` — Kaiser polyphase table resampler backed by the
  C library (``dp_resamp_cf32``).  Default choice: fast, cache-resident
  for small banks.

* :class:`ResamplerDpmfs` — Dual Phase Modified Farrow Structure
  resampler (``dp_resamp_dpmfs``).  608-byte coefficient footprint;
  ideal for multi-channel pipelines.

* :class:`HalfbandDecimator` — dedicated 2:1 halfband decimator
  (``dp_hbdecim_cf32``).  Exploits halfband symmetry to halve the
  multiply count vs a general 2-phase polyphase decimator.

All classes accept complex64 NumPy arrays and return complex64 NumPy
arrays.  Internal state is preserved across calls — suitable for
streaming pipelines.

Typical usage
-------------
>>> import numpy as np
>>> from doppler.resample import Resampler
>>> from doppler.polyphase import kaiser_prototype
>>> _, bank = kaiser_prototype()
>>> r = Resampler(bank, rate=2.0)
>>> x = np.ones(64, dtype=np.complex64)
>>> y = r.execute(x)
>>> y.dtype
dtype('complex64')
>>> len(y) > 0
True

The reference Python implementation lives in
:mod:`doppler.resample.reference`.
"""

from __future__ import annotations

import numpy as np

from ._resamp import ResampCf32
from ._resamp_dpmfs import ResampDpmfs as _ResampDpmfs
from ._hbdecim import HbDecimCf32 as _HbDecimCf32

__all__ = ["Resampler", "ResamplerDpmfs", "HalfbandDecimator"]


class Resampler:
    """Kaiser polyphase table resampler for cf32 IQ samples.

    Wraps ``dp_resamp_cf32_t`` from the C library.  Supports
    continuously-variable interpolation and decimation at any
    rational or irrational rate.

    Parameters
    ----------
    bank : np.ndarray, shape (L, N), dtype=float32
        Polyphase coefficient bank.  Row-major: ``bank[p, k]`` is
        the *k*-th tap of polyphase branch *p*.
        *L* must be a power of two.  Pass the second element of the
        tuple returned by :func:`doppler.polyphase.kaiser_prototype`.
    rate : float
        Output-to-input sample-rate ratio ``fs_out / fs_in``.
        *rate* > 1 → interpolation; *rate* < 1 → decimation.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.polyphase import kaiser_prototype
    >>> from doppler.resample import Resampler
    >>> _, bank = kaiser_prototype(passband=0.4, stopband=0.6)
    >>> r = Resampler(bank, rate=1.5)
    >>> x = np.zeros(256, dtype=np.complex64)
    >>> x[0] = 1.0          # impulse
    >>> y = r.execute(x)
    >>> y.dtype
    dtype('complex64')
    >>> len(y) >= 384 - 20  # approx ceil(256 * 1.5)
    True
    """

    def __init__(self, bank: np.ndarray, rate: float) -> None:
        bank = np.asarray(bank, dtype=np.float32)
        if bank.ndim != 2:
            raise ValueError("bank must be 2-D, shape (L, N)")
        self._r = ResampCf32(bank, rate)

    def execute(self, x: np.ndarray) -> np.ndarray:
        """Resample a block of cf32 samples.

        State is preserved across calls — suitable for streaming.

        Parameters
        ----------
        x : np.ndarray, dtype=complex64
            Input samples.

        Returns
        -------
        np.ndarray, dtype=complex64
            Output samples.  Length ≈ ``len(x) * rate``; exact count
            depends on the internal phase accumulator state.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.polyphase import kaiser_prototype
        >>> from doppler.resample import Resampler
        >>> _, bank = kaiser_prototype()
        >>> r = Resampler(bank, rate=0.5)
        >>> y = r.execute(np.ones(128, dtype=np.complex64))
        >>> len(y) <= 70      # roughly half the input
        True
        """
        x = np.asarray(x, dtype=np.complex64)
        return self._r.execute(x)

    def reset(self) -> None:
        """Zero the sample history and reset the phase accumulator."""
        self._r.reset()

    @property
    def rate(self) -> float:
        """Output-to-input sample-rate ratio."""
        return self._r.rate()

    @property
    def num_phases(self) -> int:
        """Number of polyphase branches (*L*)."""
        return self._r.num_phases()

    @property
    def num_taps(self) -> int:
        """Taps per polyphase branch (*N*)."""
        return self._r.num_taps()

    def __enter__(self) -> "Resampler":
        return self

    def __exit__(self, *_) -> None:
        self._r.__exit__()


class ResamplerDpmfs:
    """DPMFS polyphase resampler for cf32 IQ samples.

    Wraps ``dp_resamp_dpmfs_t`` from the C library.  Uses a compact
    polynomial coefficient bank (608 bytes for M=3, N=19) that fits
    in L1 cache, eliminating table-lookup cache misses in multi-channel
    pipelines.

    Parameters
    ----------
    coeffs : DPMFSCoeffs
        Polynomial coefficients from
        :func:`doppler.polyphase.fit_dpmfs`.  Must expose a ``.c``
        attribute of shape ``(2, M+1, N)``, dtype float32.
        ``coeffs.c[j, m, k]`` is the *k*-th tap coefficient for
        polynomial order *m* and phase half *j* (0 = μ ∈ [0, 0.5),
        1 = μ ∈ [0.5, 1.0)).
    rate : float
        Output-to-input sample-rate ratio ``fs_out / fs_in``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.polyphase import kaiser_prototype, fit_dpmfs
    >>> from doppler.resample import ResamplerDpmfs
    >>> _, bank = kaiser_prototype()
    >>> coeffs = fit_dpmfs(bank, M=3)
    >>> r = ResamplerDpmfs(coeffs, rate=1.5)
    >>> x = np.zeros(256, dtype=np.complex64)
    >>> x[0] = 1.0
    >>> y = r.execute(x)
    >>> y.dtype
    dtype('complex64')
    >>> len(y) >= 384 - 20
    True
    """

    def __init__(self, coeffs, rate: float) -> None:
        self._r = _ResampDpmfs(coeffs, rate)

    def execute(self, x: np.ndarray) -> np.ndarray:
        """Resample a block of cf32 samples.

        Parameters
        ----------
        x : np.ndarray, dtype=complex64
            Input samples.

        Returns
        -------
        np.ndarray, dtype=complex64
            Output samples.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.polyphase import kaiser_prototype, fit_dpmfs
        >>> from doppler.resample import ResamplerDpmfs
        >>> _, bank = kaiser_prototype()
        >>> r = ResamplerDpmfs(fit_dpmfs(bank, M=3), rate=0.5)
        >>> y = r.execute(np.ones(128, dtype=np.complex64))
        >>> len(y) <= 70
        True
        """
        x = np.asarray(x, dtype=np.complex64)
        return self._r.execute(x)

    def reset(self) -> None:
        """Zero the sample history and reset the phase accumulator."""
        self._r.reset()

    @property
    def rate(self) -> float:
        """Output-to-input sample-rate ratio."""
        return self._r.rate()

    @property
    def num_taps(self) -> int:
        """Taps per branch (*N*)."""
        return self._r.num_taps()

    @property
    def poly_order(self) -> int:
        """Polynomial order (*M*)."""
        return self._r.poly_order()

    def __enter__(self) -> "ResamplerDpmfs":
        return self

    def __exit__(self, *_) -> None:
        self._r.__exit__()


class HalfbandDecimator:
    """Halfband 2:1 decimator for cf32 IQ samples.

    Wraps ``dp_hbdecim_cf32_t`` from the C library.  Exploits halfband
    filter symmetry to reduce the multiply count to N/2 + 2 per output
    sample (vs N for a general 2-phase polyphase decimator).

    Parameters
    ----------
    bank : np.ndarray, shape (2, N), dtype=float32
        Polyphase bank from :func:`doppler.polyphase.kaiser_prototype`
        called with ``phases=2``.  One row is the symmetric FIR branch;
        the other is a pure-delay branch (unit gain at the centre tap).
        This class detects and selects the correct row automatically.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.polyphase import kaiser_prototype
    >>> from doppler.resample import HalfbandDecimator
    >>> _, bank = kaiser_prototype(phases=2)
    >>> r = HalfbandDecimator(bank)
    >>> x = np.zeros(256, dtype=np.complex64)
    >>> x[0] = 1.0          # impulse
    >>> y = r.execute(x)
    >>> y.dtype
    dtype('complex64')
    >>> len(y) == 128
    True
    """

    def __init__(self, bank: np.ndarray) -> None:
        bank = np.asarray(bank, dtype=np.float32)
        if bank.ndim != 2 or bank.shape[0] != 2:
            raise ValueError(
                "bank must be shape (2, N) — pass the second return "
                "value of kaiser_prototype(phases=2)"
            )
        N = bank.shape[1]
        centre = N // 2
        # One row has a dominant centre tap (pure delay, gain ≈ 1.0);
        # the other is the symmetric FIR branch.  Select FIR branch.
        if abs(float(bank[0, centre])) > abs(float(bank[1, centre])):
            h_fir = np.ascontiguousarray(bank[1])
        else:
            h_fir = np.ascontiguousarray(bank[0])
        self._r = _HbDecimCf32(N, h_fir)

    def execute(self, x: np.ndarray) -> np.ndarray:
        """Decimate a block of cf32 samples by 2.

        State is preserved across calls — suitable for streaming.
        Odd-length blocks are handled transparently: the dangling even
        sample is buffered and consumed on the next call.

        Parameters
        ----------
        x : np.ndarray, dtype=complex64
            Input samples.

        Returns
        -------
        np.ndarray, dtype=complex64
            Output samples.  Length == ``len(x) // 2`` for even-length
            input (or one less when the previous call left a pending
            sample).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.polyphase import kaiser_prototype
        >>> from doppler.resample import HalfbandDecimator
        >>> _, bank = kaiser_prototype(phases=2)
        >>> r = HalfbandDecimator(bank)
        >>> y = r.execute(np.ones(128, dtype=np.complex64))
        >>> len(y) == 64
        True
        """
        x = np.asarray(x, dtype=np.complex64)
        return self._r.execute(x)

    def reset(self) -> None:
        """Zero the sample history and clear any pending sample."""
        self._r.reset()

    @property
    def rate(self) -> float:
        """Decimation rate (always 0.5)."""
        return self._r.rate()

    @property
    def num_taps(self) -> int:
        """FIR branch length (*N*)."""
        return self._r.num_taps()

    def __enter__(self) -> "HalfbandDecimator":
        return self

    def __exit__(self, *_) -> None:
        self._r.__exit__()
