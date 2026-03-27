"""Type stubs for the dp_delay C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class DelayCf64:
    """Dual-buffer circular delay line for complex128 IQ samples.

    Internal capacity is rounded up to the next power of two ≥ *num_taps*
    so that the head pointer advances with a bitmask.  Every push writes
    the new sample to both halves of a 2×capacity buffer, keeping a
    contiguous *num_taps*-sample window available at all times — no
    modulo required in the reader.

    Parameters
    ----------
    num_taps:
        Length of the read window (≥ 1).

    Examples
    --------
    >>> from doppler.delay import DelayCf64
    >>> dl = DelayCf64(4)
    >>> dl.push(1+2j)
    >>> dl.push(3+4j)
    >>> dl.ptr()
    array([3.+4.j, 1.+2.j, 0.+0.j, 0.+0.j])

    MAC against a polyphase coefficient row:

    >>> import numpy as np
    >>> dl = DelayCf64(3)
    >>> for s in [1+0j, 2+0j, 3+0j]:
    ...     dl.push(s)
    >>> h = np.array([1, 0, 0], dtype=np.float32)
    >>> complex(np.dot(dl.ptr(), h))
    (3+0j)
    """

    def __init__(self, num_taps: int) -> None: ...

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    def num_taps(self) -> int:
        """Number of taps the delay line was created for."""
        ...

    def capacity(self) -> int:
        """Internal capacity (next power of two ≥ num_taps)."""
        ...

    # ------------------------------------------------------------------
    # Hot path
    # ------------------------------------------------------------------

    def push(self, x: complex) -> None:
        """Push one complex128 sample.

        Advances the head pointer and writes *x* to both buffer halves.
        After the call, :meth:`ptr` returns a window with *x* at index 0.
        """
        ...

    def ptr(self) -> NDArray[np.complex128]:
        """Return a copy of the contiguous *num_taps*-sample read window.

        ``window[0]``  — most-recently pushed sample
        ``window[-1]`` — oldest sample in the window

        The returned array is a fresh copy; subsequent :meth:`push` calls
        do not affect it.
        """
        ...

    def push_ptr(self, x: complex) -> NDArray[np.complex128]:
        """Push *x* and return the updated read window in one call.

        Equivalent to::

            dl.push(x)
            return dl.ptr()
        """
        ...

    def write(self, x: NDArray[np.complex128]) -> None:
        """Push all samples from a 1-D complex128 array.

        Equivalent to calling :meth:`push` for each element in order
        (``x[0]`` first, ``x[-1]`` last).  After the call,
        :meth:`ptr` returns a window with ``x[-1]`` at index 0.

        Parameters
        ----------
        x:
            1-D C-contiguous complex128 array.
        """
        ...

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def reset(self) -> None:
        """Zero the sample history without freeing the delay line."""
        ...

    def destroy(self) -> None:
        """Release the underlying C resource (also called on GC)."""
        ...

    def __enter__(self) -> "DelayCf64": ...
    def __exit__(self, *args: object) -> None: ...
