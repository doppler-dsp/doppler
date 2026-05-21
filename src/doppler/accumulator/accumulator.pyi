# accumulator/accumulator.pyi — type stubs for the accumulator C extension.
import numpy as np
from numpy.typing import NDArray

class AccF32:
    """AccF32 component.

    Parameters
    ----------
    acc : float, default 0.0
        acc state variable.

    Examples
    --------
    Create with defaults:

    >>> from doppler.accumulator import AccF32
    >>> obj = AccF32(0.0)
    >>> obj.get_acc()
    0.0

    Reset restores defaults:

    >>> obj.set_acc(1.0)
    >>> obj.reset()
    >>> obj.get_acc()
    0.0

    """
    def __init__(self, acc: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> None:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32]) -> None:
        """Process a samples array."""

    def get(self) -> float:
        """Get."""

    def dump(self) -> float:
        """Dump."""

    def madd(self, x: NDArray[np.float32], h: NDArray[np.float32]) -> None:
        """Madd."""

    def add2d(self, x: NDArray[np.float32]) -> None:
        """Add2d."""

    def madd2d(self, x: NDArray[np.float32], h: NDArray[np.float32]) -> None:
        """Madd2d."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AccF32": ...

    def __exit__(self, *args: object) -> None: ...

class AccCf64:
    """AccCf64 component.

    Parameters
    ----------
    acc : complex, default 0j
        acc state variable.

    Examples
    --------
    Create with defaults:

    >>> from doppler.accumulator import AccCf64
    >>> obj = AccCf64(0j)
    >>> obj.get_acc()
    0j

    Reset restores defaults:

    >>> obj.set_acc(0.0)
    >>> obj.reset()
    >>> obj.get_acc()
    0j

    """
    def __init__(self, acc: complex = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: complex) -> None:
        """Process one input sample."""

    def steps(self, x: NDArray[np.complex128]) -> None:
        """Process a samples array."""

    def get(self) -> complex:
        """Get."""

    def dump(self) -> complex:
        """Dump."""

    def madd(self, x: NDArray[np.complex128], h: NDArray[np.float32]) -> None:
        """Madd."""

    def add2d(self, x: NDArray[np.complex128]) -> None:
        """Add2d."""

    def madd2d(self, x: NDArray[np.complex128], h: NDArray[np.float32]) -> None:
        """Madd2d."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AccCf64": ...

    def __exit__(self, *args: object) -> None: ...
