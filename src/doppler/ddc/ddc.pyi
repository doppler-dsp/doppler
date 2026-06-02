# ddc/ddc.pyi — type stubs for the ddc C extension.
import numpy as np
from numpy.typing import NDArray

class DDC:
    """DDC component.

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
    >>> obj = DDC(0.0, 0.25)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Execute."""

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
    >>> obj = DDCR(0.0, 0.25)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Execute."""

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
