# filter/filter.pyi — type stubs for the filter C extension.
import numpy as np
from numpy.typing import NDArray

class FIR:
    """FIR component.

    Parameters
    ----------
    taps : NDArray[np.complex64], default ...
        taps constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.filter import FIR
    >>> obj = FIR(...)

    """
    def __init__(self, taps: NDArray[np.complex64] = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Execute."""

    @property
    def num_taps(self) -> int:
        """Num taps."""

    @property
    def is_real(self) -> bool:
        """Is real."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "FIR": ...

    def __exit__(self, *args: object) -> None: ...

class HBDecimQ15:
    """HBDecimQ15 component.

    Parameters
    ----------
    h : NDArray[np.float32], default ...
        h constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.filter import HBDecimQ15
    >>> obj = HBDecimQ15(...)

    """
    def __init__(self, h: NDArray[np.float32] = ...) -> None: ...

    def execute(self, x: NDArray[np.int16]) -> NDArray[np.int16]:
        """Execute."""

    def reset(self) -> None:
        """Reset."""

    @property
    def num_taps(self) -> int:
        """Num taps."""

    @property
    def rate(self) -> float:
        """Rate."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "HBDecimQ15": ...

    def __exit__(self, *args: object) -> None: ...
