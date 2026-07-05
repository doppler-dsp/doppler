# delay/delay.pyi — type stubs for the delay C extension.
import numpy as np
from numpy.typing import NDArray

class DelayCf64:
    """Create a dual-buffer circular delay line of length num_taps. The internal capacity is rounded up to the next power of two so that modular indexing reduces to a single bitwise AND.  Any window of num_taps consecutive samples is always contiguous in the backing store; no wrap-around copy is ever needed.

    Parameters
    ----------
    num_taps : int, default 1
        Number of delay taps (window length, >= 1). Internally rounded up to the next power of two.

    Examples
    --------
    Create with defaults:

    >>> from doppler.delay import DelayCf64
    >>> obj = DelayCf64(num_taps=1)

    """
    def __init__(self, num_taps: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset the delay line to its post-create state. Zeroes the entire dual buffer and resets the write pointer to 0, discarding all previously pushed samples.  The num_taps and capacity are preserved; only the sample history is cleared.

        Examples
        --------
        >>> from doppler.delay import DelayCf64
        >>> d = DelayCf64(num_taps=3)
        >>> d.push(1+2j)
        >>> d.push(3+4j)
        >>> d.ptr().tolist()
        [(3+4j), (1+2j), 0j]
        >>> d.reset()
        >>> d.ptr().tolist()
        [0j, 0j, 0j]

        """

    def push(self, x: complex) -> None:
        """Advance the write pointer and insert a new sample. The head pointer decrements (mod capacity) before the write so that `buf[head]` always holds the most recent sample.  The same value is simultaneously written at `buf[head + capacity]` to keep the mirror half in sync; this ensures any num_taps-length window starting at head is contiguous without an extra copy.

        Parameters
        ----------
        x : complex
            New complex sample to insert.

        Examples
        --------
        >>> from doppler.delay import DelayCf64
        >>> d = DelayCf64(num_taps=3)
        >>> d.push(1+2j)
        >>> d.push(3+4j)
        >>> d.ptr().tolist()
        [(3+4j), (1+2j), 0j]

        """

    def ptr(
        self, n: int = ..., out: NDArray[np.complex128] | None = ...
    ) -> NDArray[np.complex128]:
        """Return a zero-copy view of the n most recent samples. Copies at most min(n, num_taps) samples starting from `buf[head]` into out.  Because the dual-buffer layout guarantees contiguity, this is a single memcpy of up to num_taps elements; no wrap-around logic is needed.  Without out=, the Python binding returns a NumPy array backed directly by the pre-allocated output buffer (base object is the DelayCf64 itself); with out= (must have at least max(ptr_max_out(), n) elements), writes directly into the caller's array and returns a view of it.

        Parameters
        ----------
        n : int, default 1
            Number of most recent samples to return.
        out : NDArray[np.complex128], optional
            Caller-provided output buffer.

        Returns
        -------
        NDArray[np.complex128]
            Number of samples written.

        Examples
        --------
        >>> from doppler.delay import DelayCf64
        >>> d = DelayCf64(num_taps=3)
        >>> d.push(1+0j)
        >>> d.push(2+0j)
        >>> y = d.ptr()
        >>> y.tolist()
        [(2+0j), (1+0j), 0j]
        >>> y.dtype
        dtype('complex128')
        >>> y.shape
        (3,)

        """

    def ptr_max_out(self) -> int:
        """Max output length ptr() can produce for the current state. Use to size the ``out=`` buffer."""

    def push_ptr(
        self, x: complex, out: NDArray[np.complex128] | None = ...
    ) -> NDArray[np.complex128]:
        """Atomically push a sample and snapshot the current window. Equivalent to calling push(x) then ptr(num_taps), but avoids the overhead of a second function call.  Always writes exactly num_taps samples.  Without out=, the Python binding returns a NumPy array backed by the pre-allocated push_ptr output buffer; with out= (must have exactly num_taps elements), writes directly into the caller's array and returns it.

        Parameters
        ----------
        x : complex
            New complex sample to insert.
        out : NDArray[np.complex128], optional
            Caller-provided output buffer; must have exactly num_taps elements.

        Returns
        -------
        NDArray[np.complex128]
            num_taps (always equal to the window length).

        Examples
        --------
        >>> from doppler.delay import DelayCf64
        >>> d = DelayCf64(num_taps=3)
        >>> d.push_ptr(1+0j).tolist()
        [(1+0j), 0j, 0j]
        >>> d.push_ptr(2+0j).tolist()
        [(2+0j), (1+0j), 0j]

        """

    def push_ptr_max_out(self) -> int:
        """Max output length push_ptr() can produce for the current state (always exactly num_taps). Use to size the ``out=`` buffer."""

    def write(self, x: complex) -> None:
        """Alias for delay_push(); insert a sample without reading back. Provided for API symmetry with write-then-read patterns where the caller wants to decouple sample ingestion from window inspection. Internally delegates to delay_push() with no additional overhead.

        Parameters
        ----------
        x : complex
            New complex sample to insert.

        Examples
        --------
        >>> from doppler.delay import DelayCf64
        >>> d = DelayCf64(num_taps=2)
        >>> d.write(5+6j)
        >>> d.ptr().tolist()
        [(5+6j), 0j]

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def num_taps(self) -> int:
        """Num taps."""

    @property
    def capacity(self) -> int:
        """Capacity."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DelayCf64": ...

    def __exit__(self, *args: object) -> None: ...
