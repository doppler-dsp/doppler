# ddc/ddc.pyi — type stubs for the ddc C extension.
import numpy as np
from numpy.typing import NDArray

class DDC:
    """Create a complex-input Digital Down-Converter. Allocates internal state for the LO and RateConverter cascade. The RateConverter selects the cheapest multi-stage decimation chain (CIC + optional halfband + polyphase resampler) for the given rate.

    Parameters
    ----------
    norm_freq : float, default 0.0
        LO frequency in cycles/sample at the input rate. Set to -f_carrier to shift a carrier at f_carrier to DC.  Any real value is accepted.
    rate : float, default 0.25
        Output rate / input rate.  Must be > 0.  Values ≥ 1 are up-sampling; typical use is decimation (0 < rate < 1).

    Examples
    --------
    Create with defaults:

    >>> from doppler.ddc import DDC
    >>> obj = DDC(norm_freq=0.0, rate=0.25)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Mix input block with LO, then rate-convert.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block; accepted as float32 (auto-cast).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written (C-only).

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
        >>> t = np.arange(4096)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> y = ddc.execute(x)
        >>> y.shape
        (1024,)
        >>> y.dtype
        dtype('complex64')
        >>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
        1.0

        """

    def execute_max_out(self) -> int:
        """Max output length execute() can produce for the current state."""

    def reset(self) -> None:
        """Zero LO phase and filter history.

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=0.0, rate=0.25)
        >>> x = np.ones(64, dtype=np.complex64)
        >>> y1 = ddc.execute(x)
        >>> ddc.reset()
        >>> y2 = ddc.execute(x)
        >>> bool(np.array_equal(y1, y2))
        True

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def norm_freq(self) -> float:
        """Return the current LO normalised frequency (cycles/sample)."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Return the configured output/input rate ratio (read-only). The rate is fixed at create time; change it by destroying and recreating the DDC with the new value."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DDC": ...

    def __exit__(self, *args: object) -> None: ...
