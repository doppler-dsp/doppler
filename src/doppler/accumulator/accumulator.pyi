# accumulator/accumulator.pyi — type stubs for the accumulator C extension.
import numpy as np
from numpy.typing import NDArray

class AccF32:
    """Running sum accumulator for float32 samples.

    Parameters
    ----------
    acc : float, optional
        Initial accumulator value (default 0.0).

    Examples
    --------
    >>> from doppler.accumulator import AccF32
    >>> obj = AccF32(0.0)
    >>> obj.get_acc()
    0.0
    >>> obj.set_acc(1.0)
    >>> obj.reset()
    >>> obj.get_acc()
    0.0
    """

    def __init__(self, acc: float = ...) -> None: ...
    def reset(self) -> None:
        """Reset accumulator to zero."""

    def step(self, x: np.float32) -> None:
        """Add one float32 sample to the accumulator."""

    def steps(self, x: NDArray[np.float32]) -> None:
        """Add an array of float32 samples to the accumulator."""

    def get(self) -> float:
        """Return current accumulator value (non-destructive)."""

    def dump(self) -> float:
        """Return current value and reset accumulator to zero (atomic)."""

    def madd(
        self,
        x: NDArray[np.float32],
        h: NDArray[np.float32],
    ) -> None:
        """Multiply-accumulate: acc += sum(x[i] * h[i])."""

    def add2d(self, x: NDArray[np.float32]) -> None:
        """Add all elements of x to the accumulator."""

    def madd2d(
        self,
        x: NDArray[np.float32],
        h: NDArray[np.float32],
    ) -> None:
        """2-D multiply-accumulate: acc += sum(x[i] * h[i])."""

    def get_acc(self) -> float:
        """Return the raw accumulator state."""

    def set_acc(self, val: float) -> None:
        """Set the raw accumulator state."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AccF32": ...
    def __exit__(self, *args: object) -> None: ...

class AccCf64:
    """Running sum accumulator for double complex samples.

    Parameters
    ----------
    acc : complex, optional
        Initial accumulator value (default 0+0j).

    Examples
    --------
    >>> from doppler.accumulator import AccCf64
    >>> obj = AccCf64(0j)
    >>> obj.get().real
    0.0
    """

    def __init__(self, acc: complex = ...) -> None: ...
    def reset(self) -> None:
        """Reset accumulator to zero."""

    def step(self, x: complex) -> None:
        """Add one complex128 sample to the accumulator."""

    def steps(self, x: NDArray[np.complex128]) -> None:
        """Add an array of complex128 samples to the accumulator."""

    def get(self) -> complex:
        """Return current accumulator value (non-destructive)."""

    def dump(self) -> complex:
        """Return current value and reset accumulator to zero (atomic)."""

    def madd(
        self,
        x: NDArray[np.complex128],
        h: NDArray[np.float32],
    ) -> None:
        """Multiply-accumulate: acc += sum(x[i] * h[i]) with float weights."""

    def add2d(self, x: NDArray[np.complex128]) -> None:
        """Add all elements of x to the accumulator."""

    def madd2d(
        self,
        x: NDArray[np.complex128],
        h: NDArray[np.float32],
    ) -> None:
        """2-D multiply-accumulate: acc += sum(x[i] * h[i])."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AccCf64": ...
    def __exit__(self, *args: object) -> None: ...
