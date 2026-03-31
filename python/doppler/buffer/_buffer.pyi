"""Type stubs for the dp_buffer C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class F32Buffer:
    """Lock-free SPSC ring buffer for float32 samples.

    Parameters
    ----------
    n:
        Buffer capacity in samples.
    """

    def __init__(self, n: int) -> None: ...
    def capacity(self) -> int:
        """Buffer capacity in samples."""
        ...

    def dropped(self) -> int:
        """Number of samples dropped due to buffer-full conditions."""
        ...

    def write(self, arr: NDArray[np.float32]) -> bool:
        """Non-blocking write.

        Parameters
        ----------
        arr:
            Contiguous 1-D float32 array.

        Returns
        -------
        bool
            ``True`` on success; ``False`` if the buffer was full
            (samples are dropped and the dropped counter increments).
        """
        ...

    def wait(self, n: int) -> NDArray[np.float32]:
        """Block until *n* samples are available and return a view.

        Releases the GIL while waiting so a Python producer thread can
        run concurrently.  Call :meth:`consume` when done with the view.

        Returns
        -------
        NDArray[np.float32]
            Zero-copy view of *n* samples in the ring buffer.
        """
        ...

    def consume(self, n: int | None = None) -> None:
        """Release *n* samples (default: count from the last :meth:`wait`)."""
        ...

    def destroy(self) -> None:
        """Release the underlying C resource (also called on GC)."""
        ...

    def __enter__(self) -> "F32Buffer": ...
    def __exit__(self, *args: object) -> None: ...

class F64Buffer:
    """Lock-free SPSC ring buffer for float64 samples.

    Parameters
    ----------
    n:
        Buffer capacity in samples.
    """

    def __init__(self, n: int) -> None: ...
    def capacity(self) -> int: ...
    def dropped(self) -> int: ...
    def write(self, arr: NDArray[np.float64]) -> bool: ...
    def wait(self, n: int) -> NDArray[np.float64]: ...
    def consume(self, n: int | None = None) -> None: ...
    def destroy(self) -> None: ...
    def __enter__(self) -> "F64Buffer": ...
    def __exit__(self, *args: object) -> None: ...

class I16Buffer:
    """Lock-free SPSC ring buffer for int16 samples.

    Parameters
    ----------
    n:
        Buffer capacity in samples.
    """

    def __init__(self, n: int) -> None: ...
    def capacity(self) -> int: ...
    def dropped(self) -> int: ...
    def write(self, arr: NDArray[np.int16]) -> bool: ...
    def wait(self, n: int) -> NDArray[np.int16]: ...
    def consume(self, n: int | None = None) -> None: ...
    def destroy(self) -> None: ...
    def __enter__(self) -> "I16Buffer": ...
    def __exit__(self, *args: object) -> None: ...
