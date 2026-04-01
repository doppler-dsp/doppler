"""Type stubs for the dp_hbdecim C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class HbDecimCf32:
    """Halfband 2:1 decimator for complex64 IQ samples.

    Wraps ``dp_hbdecim_cf32_t``.  Exploits halfband filter symmetry to
    reduce the FIR multiply count to N/2 per output sample; the
    pure-delay branch adds one additional multiply.

    Construct via :class:`doppler.resample.HalfbandDecimator` rather
    than directly — it selects the correct FIR branch from the
    polyphase bank automatically.

    Parameters
    ----------
    num_taps:
        Length of the FIR branch (row from a ``kaiser_prototype(phases=2)``
        bank).  May be even or odd.
    h:
        FIR branch coefficients, shape ``(num_taps,)``, dtype ``float32``.
        Pass one row of the polyphase bank returned by
        ``kaiser_prototype(phases=2)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.polyphase import kaiser_prototype
    >>> from doppler.resample import HalfbandDecimator
    >>> _, bank = kaiser_prototype(phases=2)
    >>> r = HalfbandDecimator(bank)           # preferred entry point
    >>> y = r.execute(np.ones(128, dtype=np.complex64))
    >>> len(y)
    64
    """

    def __init__(
        self,
        num_taps: int,
        h: NDArray[np.float32],
    ) -> None: ...

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    def rate(self) -> float:
        """Decimation rate (always 0.5)."""
        ...

    def num_taps(self) -> int:
        """FIR branch length passed at construction."""
        ...

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    def execute(
        self,
        x: NDArray[np.complex64],
    ) -> NDArray[np.complex64]:
        """Decimate a block of complex64 samples by 2.

        State is preserved across calls — suitable for streaming.
        Odd-length blocks are handled transparently: the dangling even
        sample is buffered and consumed on the next call.

        Parameters
        ----------
        x:
            1-D complex64 input array.

        Returns
        -------
        NDArray[np.complex64]
            Output samples.  Length is ``len(x) // 2`` for even-length
            input (adjusted by any pending sample from a previous call).
        """
        ...

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def reset(self) -> None:
        """Zero the sample history and clear any pending sample."""
        ...

    def __enter__(self) -> "HbDecimCf32": ...
    def __exit__(self, *args: object) -> None: ...
