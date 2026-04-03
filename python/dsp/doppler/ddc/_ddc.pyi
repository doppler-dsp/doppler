"""Type stubs for the _ddc C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class Ddc:
    """Digital Down-Converter: NCO mix + DPMFS decimation.

    Wraps ``dp_ddc_t`` from the C library.  Uses built-in M=3 N=19
    Kaiser-DPMFS filter coefficients (passband ≤ 0.4·fs_out,
    stopband ≥ 0.6·fs_out, 60 dB rejection) — no design step needed.

    All internal buffers are pre-allocated at construction time; no
    heap allocation occurs during :meth:`execute`.

    Parameters
    ----------
    norm_freq : float
        NCO normalised frequency in cycles/sample.  Negative values
        shift a positive-offset signal to DC.
    num_in : int
        Fixed input block size.  Pass the same length to every
        :meth:`execute` call.
    rate : float
        ``fs_out / fs_in``.  1.0 bypasses the resampler.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.ddc import Ddc
    >>> ddc = Ddc(-0.1, 1024, 0.25)
    >>> x = np.zeros(1024, dtype=np.complex64)
    >>> y = ddc.execute(x)
    >>> y.dtype
    dtype('complex64')
    """

    def __init__(self, norm_freq: float, num_in: int, rate: float) -> None: ...

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    @property
    def max_out(self) -> int:
        """Maximum output samples per :meth:`execute` call (fixed)."""
        ...

    @property
    def nout(self) -> int:
        """Actual output count from the last :meth:`execute` call."""
        ...

    # ------------------------------------------------------------------
    # Control
    # ------------------------------------------------------------------

    def set_freq(self, norm_freq: float) -> None:
        """Retune the NCO without resetting phase or resampler history."""
        ...

    def get_freq(self) -> float:
        """Return the current NCO normalised frequency."""
        ...

    def reset(self) -> None:
        """Zero NCO phase and resampler history."""
        ...

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Mix and resample a block of complex64 IQ samples.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples, length ``num_in``.

        Returns
        -------
        NDArray[np.complex64]
            Output samples, length ``nout`` (≤ ``max_out``).
        """
        ...

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def __enter__(self) -> "Ddc": ...
    def __exit__(self, *args: object) -> None: ...
