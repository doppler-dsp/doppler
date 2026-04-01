"""Type stubs for the dp_resamp C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class ResampCf32:
    """Kaiser polyphase resampler for complex64 IQ samples.

    Wraps ``dp_resamp_cf32_t`` from the C library. Supports
    continuously-variable interpolation and decimation at any
    rational or irrational rate.

    Construct via :class:`doppler.resample.Resampler` rather than
    directly — it handles polyphase bank layout automatically.

    Parameters
    ----------
    bank : NDArray[np.float32], shape (L, N)
        Polyphase coefficient bank, row-major: ``bank[p, k]`` is
        the *k*-th tap of polyphase branch *p*.
        *L* must be a power of two (number of phases).
        *N* is the number of taps per branch.
        Pass the second return value of
        :func:`doppler.polyphase.kaiser_prototype` directly.
    rate : float
        Output-to-input sample-rate ratio ``fs_out / fs_in``.
        *rate* > 1 → interpolation; *rate* < 1 → decimation.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.resample import Resampler
    >>> from doppler.polyphase import kaiser_prototype
    >>> _, bank = kaiser_prototype()
    >>> r = Resampler(bank, rate=1.5)
    >>> x = np.ones(64, dtype=np.complex64)
    >>> y = r.execute(x)
    >>> y.dtype
    dtype('complex64')
    """

    def __init__(self, bank: NDArray[np.float32], rate: float) -> None: ...

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    def rate(self) -> float:
        """Return the output-to-input sample-rate ratio (fs_out / fs_in)."""
        ...

    def num_phases(self) -> int:
        """Return the number of polyphase branches (L)."""
        ...

    def num_taps(self) -> int:
        """Return the number of taps per polyphase branch (N)."""
        ...

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Resample a block of complex64 samples.

        State is preserved across calls — suitable for streaming.

        Parameters
        ----------
        x : NDArray[np.complex64]
            1-D input array of complex64 samples.

        Returns
        -------
        NDArray[np.complex64]
            Output samples.  Length ≈ ``len(x) * rate``;
            exact count depends on the phase accumulator state.
        """
        ...

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def reset(self) -> None:
        """Zero the sample history and reset the phase accumulator."""
        ...

    def __enter__(self) -> "ResampCf32": ...
    def __exit__(self, *args: object) -> None: ...
