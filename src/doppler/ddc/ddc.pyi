# ddc/ddc.pyi — type stubs for the ddc C extension.
import numpy as np
from numpy.typing import NDArray

class DDC:
    """Create a complex-input DDC.

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
            Input samples, complex64, length x_len.

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written.
        """

    def reset(self) -> None:
        """Zero LO phase and filter history.
        """

    @property
    def norm_freq(self) -> float:
        """Return the current LO normalised frequency."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Return the configured output/input rate ratio."""

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
