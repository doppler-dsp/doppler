"""Type stubs for the dp_accumulator C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray


class AccF32:
    """General-purpose float32 scalar accumulator wrapping dp_acc_f32_t.

    Examples
    --------
    >>> from doppler.accumulator import AccF32
    >>> import numpy as np
    >>> acc = AccF32()
    >>> acc.push(1.0)
    >>> acc.push(2.0)
    >>> acc.dump()
    3.0
    >>> x = np.array([1, 2, 3, 4], dtype=np.float32)
    >>> h = np.array([1, 0, 0, 0], dtype=np.float32)
    >>> acc.madd(x, h)
    >>> acc.dump()
    1.0
    """

    def __init__(self) -> None: ...

    def reset(self) -> None:
        """Zero the accumulator without reading it."""
        ...

    def get(self) -> float:
        """Read the current accumulated value without clearing it."""
        ...

    def dump(self) -> float:
        """Read the accumulated value and reset to zero."""
        ...

    def push(self, x: float) -> None:
        """Add a single scalar value."""
        ...

    def add(self, x: NDArray[np.float32]) -> None:
        """Accumulate all elements of a 1-D float32 array."""
        ...

    def madd(
        self,
        x: NDArray[np.float32],
        h: NDArray[np.float32],
    ) -> None:
        """Multiply-accumulate: sum(x[k] * h[k]) added to the running total.

        Parameters
        ----------
        x:
            Signal samples (float32).
        h:
            Coefficients (float32); must be at least as long as *x*.
        """
        ...

    def destroy(self) -> None:
        """Release the underlying C resource (also called on GC)."""
        ...

    def __enter__(self) -> "AccF32": ...
    def __exit__(self, *args: object) -> None: ...


class AccCf64:
    """General-purpose complex128 accumulator wrapping dp_acc_cf64_t.

    Coefficients are float32; signal data is complex128 (double I/Q).

    Examples
    --------
    >>> from doppler.accumulator import AccCf64
    >>> import numpy as np
    >>> acc = AccCf64()
    >>> x = np.array([3+4j, 1+2j, 0+0j], dtype=np.complex128)
    >>> h = np.array([1, 0, 0], dtype=np.float32)
    >>> acc.madd(x, h)
    >>> acc.dump()
    (3+4j)
    """

    def __init__(self) -> None: ...

    def reset(self) -> None:
        """Zero both I and Q accumulators."""
        ...

    def get(self) -> complex:
        """Read the current accumulated value without clearing it."""
        ...

    def dump(self) -> complex:
        """Read the accumulated value and reset both channels to zero."""
        ...

    def push(self, x: complex) -> None:
        """Add a single complex128 sample."""
        ...

    def add(self, x: NDArray[np.complex128]) -> None:
        """Accumulate all elements of a 1-D complex128 array."""
        ...

    def madd(
        self,
        x: NDArray[np.complex128],
        h: NDArray[np.float32],
    ) -> None:
        """Multiply-accumulate: sum(x[k] * h[k]) added to the running total.

        Parameters
        ----------
        x:
            Complex128 signal samples (delay-line window).
        h:
            Float32 filter coefficients (polyphase bank row).
        """
        ...

    def destroy(self) -> None:
        """Release the underlying C resource (also called on GC)."""
        ...

    def __enter__(self) -> "AccCf64": ...
    def __exit__(self, *args: object) -> None: ...
