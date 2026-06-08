# ddc/ddc.pyi — type stubs for the ddc C extension.
import numpy as np
from numpy.typing import NDArray

class DDC:
    """Create a complex-input Digital Down-Converter. Allocates internal state for the LO and RateConverter cascade. The RateConverter selects the cheapest multi-stage decimation chain (CIC + optional halfband + polyphase resampler) for the given rate.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.
    rate : float, default 0.25
        rate constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ddc import DDC
    >>> obj = DDC(norm_freq=0.0, rate=0.25)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
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

class DDCR:
    """DDCR component.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.
    rate : float, default 0.25
        rate constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ddc import DDCR
    >>> obj = DDCR(norm_freq=0.0, rate=0.25)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def execute(self, x: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Halfband R2C decimate, LO mix, then rate-convert.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def reset(self) -> None:
        """Zero halfband, LO phase, and filter history.
        """

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Rate."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DDCR": ...

    def __exit__(self, *args: object) -> None: ...
