# filter/filter.pyi — type stubs for the filter C extension.
import numpy as np
from numpy.typing import NDArray

class FIR:
    """FIR component.

    Parameters
    ----------
    taps : Any, default ...
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

class CIC:
    """CIC component.

    Parameters
    ----------
    R : int, default 1
        R constructor parameter.
    N : int, default 4
        N constructor parameter.
    M : int, default 1
        M constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.filter import CIC
    >>> obj = CIC(1, 4, 1)

    """
    def __init__(self, R: int = ..., N: int = ..., M: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def reconfigure(self, R: int, N: int, M: int) -> None:
        """Reconfigure."""

    def decimate(self, x: complex) -> NDArray[np.complex64]:
        """Decimate."""

    @property
    def R(self) -> int:
        """R."""

    @property
    def N(self) -> int:
        """N."""

    @property
    def M(self) -> int:
        """M."""

    @property
    def input_scale(self) -> float:
        """Input scale."""

    @property
    def output_scale(self) -> float:
        """Output scale."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CIC": ...

    def __exit__(self, *args: object) -> None: ...
