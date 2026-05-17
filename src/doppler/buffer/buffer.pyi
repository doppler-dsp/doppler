# buffer/buffer.pyi — type stubs for the buffer C extension.
import numpy as np
from numpy.typing import NDArray

class F32Buffer:
    """Lock-free SPSC ring buffer for complex64 (CF32) samples.

    Uses virtual-memory double-mapping so the consumer always sees a
    contiguous window across the wrap boundary.  One thread writes;
    one thread reads.

    Parameters
    ----------
    capacity : int
        Buffer size in complex samples.  Must be a power of two and
        large enough that ``capacity * 8`` is page-aligned.

    Examples
    --------
    >>> from doppler.buffer import F32Buffer
    >>> import numpy as np
    >>> buf = F32Buffer(1024)
    >>> buf.capacity
    1024
    >>> buf.write(np.ones(512, dtype=np.complex64))
    True

    """

    def __init__(self, capacity: int) -> None: ...
    def write(self, arr: NDArray[np.complex64]) -> bool:
        """Non-blocking write.  Returns True on success, False if full."""
        ...
    def wait(self, n: int) -> NDArray[np.complex64]:
        """Block until n samples are available; return zero-copy view.

        Must call :meth:`consume` when done with the view.
        """
        ...
    def consume(self, n: int = ...) -> None:
        """Release n samples (defaults to the count from the last wait)."""
        ...
    def destroy(self) -> None:
        """Unmap and release the buffer."""
        ...
    @property
    def capacity(self) -> int:
        """Buffer capacity in complex samples."""
        ...
    @property
    def dropped(self) -> int:
        """Samples dropped due to buffer overrun."""
        ...

class F64Buffer:
    """Lock-free SPSC ring buffer for complex128 (CF64) samples.

    Parameters
    ----------
    capacity : int
        Buffer size in complex samples.  Must be a power of two.

    Examples
    --------
    >>> from doppler.buffer import F64Buffer
    >>> import numpy as np
    >>> buf = F64Buffer(512)
    >>> buf.capacity
    512

    """

    def __init__(self, capacity: int) -> None: ...
    def write(self, arr: NDArray[np.complex128]) -> bool:
        """Non-blocking write.  Returns True on success, False if full."""
        ...
    def wait(self, n: int) -> NDArray[np.complex128]:
        """Block until n samples are available; return zero-copy view."""
        ...
    def consume(self, n: int = ...) -> None:
        """Release n samples."""
        ...
    def destroy(self) -> None:
        """Unmap and release the buffer."""
        ...
    @property
    def capacity(self) -> int:
        """Buffer capacity in complex samples."""
        ...
    @property
    def dropped(self) -> int:
        """Samples dropped due to buffer overrun."""
        ...

class I16Buffer:
    """Lock-free SPSC ring buffer for interleaved int16 IQ pairs.

    The :meth:`wait` view has shape ``(n, 2)``: column 0 is I,
    column 1 is Q.

    Parameters
    ----------
    capacity : int
        Buffer size in IQ sample pairs.  Must be a power of two.

    Examples
    --------
    >>> from doppler.buffer import I16Buffer
    >>> import numpy as np
    >>> buf = I16Buffer(2048)
    >>> buf.capacity
    2048

    """

    def __init__(self, capacity: int) -> None: ...
    def write(self, arr: NDArray[np.int16]) -> bool:
        """Non-blocking write of flat int16 array (length must be even)."""
        ...
    def wait(self, n: int) -> NDArray[np.int16]:
        """Block until n IQ pairs are available; return shape (n, 2) view."""
        ...
    def consume(self, n: int = ...) -> None:
        """Release n IQ pairs."""
        ...
    def destroy(self) -> None:
        """Unmap and release the buffer."""
        ...
    @property
    def capacity(self) -> int:
        """Buffer capacity in IQ sample pairs."""
        ...
    @property
    def dropped(self) -> int:
        """Samples dropped due to buffer overrun."""
        ...
