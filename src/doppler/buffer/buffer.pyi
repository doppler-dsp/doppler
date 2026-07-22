# buffer/buffer.pyi — type stubs for the buffer C extension.
import numpy as np
from numpy.typing import NDArray

class F32Buffer:
    """Lock-free SPSC ring buffer for complex64 (CF32) samples.

    Uses virtual-memory double-mapping so the consumer always sees a
    contiguous window across the wrap boundary.  The same physical pages
    are mapped twice at adjacent virtual addresses, so a read that crosses
    the end of the ring returns data from the beginning without any
    memcpy or branch.  Intended for single-producer / single-consumer
    use; do not share one instance between multiple producer threads or
    multiple consumer threads.

    Head and tail indices are separated by a full cache line (64 bytes)
    to prevent false-sharing between the producer and consumer cores.
    On x86-64 the spin-wait loop in :meth:`wait` uses ``PAUSE`` to
    reduce power consumption and avoid branch-predictor pollution.

    Parameters
    ----------
    capacity : int
        Requested buffer size in complex samples.  Must be a power of two.
        The VM mirror is built at page granularity, so ``capacity * 8`` must
        span a whole page; a sub-page request is rounded **up** to the
        smallest power-of-two that does (minimum 512 on 4 KiB pages, 2048 on
        16 KiB pages such as macOS arm64).  Read :attr:`capacity` back for the
        size actually allocated.

    Examples
    --------
    >>> from doppler.buffer import F32Buffer
    >>> import numpy as np
    >>> buf = F32Buffer(1024)
    >>> buf.capacity >= 1024
    True
    >>> buf.write(np.ones(512, dtype=np.complex64))
    True

    """

    def __init__(self, capacity: int) -> None: ...

    def write(self, arr: NDArray[np.complex64]) -> bool:
        """Write samples into the buffer without blocking.

        Copies the complex64 array into the ring buffer in a single
        ``memcpy``.  If there is not enough free space for all
        ``len(arr)`` samples the write is rejected entirely — no partial
        write occurs.  When rejected, the dropped counter is incremented
        by ``len(arr)``.  The array must be 1-D and C-contiguous.

        Parameters
        ----------
        arr : ndarray of complex64
            Samples to write.  Must be 1-D and C-contiguous.

        Returns
        -------
        bool
            ``True`` if all samples were written; ``False`` if the
            buffer did not have enough free space (samples were dropped).

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> buf.write(np.array([1+2j, 3+4j], dtype=np.complex64))
        True
        >>> buf2 = F32Buffer(1024)
        >>> buf2.write(np.zeros(1024, dtype=np.complex64))
        True
        >>> buf2.write(np.zeros(1, dtype=np.complex64))
        False

        """
        ...

    def wait(self, n: int) -> NDArray[np.complex64]:
        """Block until ``n`` samples are available, then return a zero-copy view.

        Spins (releasing the GIL so a producer thread can run concurrently)
        until at least ``n`` samples have been written by the producer.
        Returns a 1-D complex64 NumPy array that is a *direct view* into
        the double-mapped ring buffer — no data is copied.  Because of the
        double-mapping, the view is always contiguous even when the
        requested range wraps around the physical end of the ring.

        The caller **must** call :meth:`consume` before the next call to
        ``wait``.  Using the returned array after ``consume`` is undefined
        behaviour; the producer may overwrite it at any time.

        Parameters
        ----------
        n : int
            Number of complex samples to wait for.  Must be positive and
            not larger than :attr:`capacity`.

        Returns
        -------
        ndarray of complex64, shape (n,)
            Zero-copy view of the next ``n`` samples in the ring.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> buf.write(np.array([1+2j, 3+4j, 5+6j], dtype=np.complex64))
        True
        >>> view = buf.wait(3)
        >>> view.dtype
        dtype('complex64')
        >>> view.shape
        (3,)
        >>> view.tolist()
        [(1+2j), (3+4j), (5+6j)]
        >>> buf.consume(3)

        """
        ...

    def consume(self, n: int = ...) -> None:
        """Release ``n`` samples back to the producer.

        Advances the consumer tail pointer by ``n``, making that space
        available for the producer to overwrite.  Must be called after
        processing the view returned by :meth:`wait`.  If ``n`` is
        omitted, the count from the most recent :meth:`wait` call is
        used automatically.

        Parameters
        ----------
        n : int, optional
            Number of samples to release.  Defaults to the count passed
            to the last :meth:`wait` call.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> buf.write(np.ones(4, dtype=np.complex64))
        True
        >>> _ = buf.wait(4)
        >>> buf.consume()

        """
        ...

    def destroy(self) -> None:
        """Unmap the double-mapped region and free the buffer struct.

        Releases both virtual-address views via ``munmap`` (POSIX) or
        ``UnmapViewOfFile`` (Windows) and frees the struct allocated by
        the constructor.  After calling ``destroy`` the object must not
        be used again.  Calling ``destroy`` more than once is safe; the
        second call is a no-op.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> buf = F32Buffer(1024)
        >>> buf.destroy()

        """
        ...

    @property
    def capacity(self) -> int:
        """Buffer capacity in complex samples.

        Read-only.  Set at construction time and never changes.  This is the
        *actual* allocated size: a sub-page request is rounded up to the
        page-spanning minimum (512 on 4 KiB pages, 2048 on 16 KiB pages), so
        it may exceed the value passed to the constructor.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> F32Buffer(1024).capacity >= 1024
        True

        """
        ...

    @property
    def available(self) -> int:
        """Samples written but not yet consumed.

        The largest ``n`` for which :meth:`wait` is guaranteed to return
        without spinning.  Read this rather than tracking the count
        yourself: :meth:`wait` has no timeout and no short return, so
        asking for more than has been written spins until the producer
        catches up -- forever, if there is no producer.

        Read from the consumer side this is a *lower* bound.  A producer
        on another thread can only increase it, so a block sized from it
        is always safe; it may simply be smaller than what has landed by
        the time :meth:`wait` runs.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> buf.available
        0
        >>> _ = buf.write(np.zeros(100, dtype=np.complex64))
        >>> buf.available
        100
        >>> _ = buf.wait(64); buf.consume(64)
        >>> buf.available
        36

        """
        ...

    @property
    def dropped(self) -> int:
        """Cumulative count of samples dropped due to buffer overrun.

        Incremented atomically (relaxed order) by :meth:`write` whenever
        a write is rejected because the buffer is full.  The increment is
        by the number of samples in the rejected batch, not by 1.
        Resets to zero only when the object is recreated.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> buf.dropped
        0
        >>> buf.write(np.zeros(1024, dtype=np.complex64))
        True
        >>> buf.write(np.zeros(3, dtype=np.complex64))
        False
        >>> buf.dropped
        3

        """
        ...

class F64Buffer:
    """Lock-free SPSC ring buffer for complex128 (CF64) samples.

    Identical in design to :class:`F32Buffer` but stores ``double``
    complex (128-bit / 16 bytes per sample) instead of ``float``
    complex.  The virtual-memory double-mapping and cache-line separated
    head/tail layout are the same.  The GIL is released inside
    :meth:`wait` so a producer thread can run concurrently.

    Parameters
    ----------
    capacity : int
        Requested buffer size in complex samples.  Must be a power of two.
        ``capacity * 16`` must span a whole page; a sub-page request is
        rounded **up** to the smallest power-of-two that does (minimum 256 on
        4 KiB pages, 1024 on 16 KiB pages).  Read :attr:`capacity` back for the
        size actually allocated.

    Examples
    --------
    >>> from doppler.buffer import F64Buffer
    >>> import numpy as np
    >>> buf = F64Buffer(512)
    >>> buf.capacity >= 512
    True
    >>> buf.write(np.ones(256, dtype=np.complex128))
    True

    """

    def __init__(self, capacity: int) -> None: ...

    def write(self, arr: NDArray[np.complex128]) -> bool:
        """Write complex128 samples into the buffer without blocking.

        Copies the entire array in a single ``memcpy``.  Rejects the
        write atomically if there is insufficient free space; the
        dropped counter is incremented by ``len(arr)`` in that case.
        The array must be 1-D and C-contiguous.

        Parameters
        ----------
        arr : ndarray of complex128
            Samples to write.  Must be 1-D and C-contiguous.

        Returns
        -------
        bool
            ``True`` if all samples were written; ``False`` if the
            buffer was full (all samples dropped).

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(512)
        >>> buf.write(np.array([1+2j, 3+4j], dtype=np.complex128))
        True
        >>> buf2 = F64Buffer(512)
        >>> buf2.write(np.zeros(512, dtype=np.complex128))
        True
        >>> buf2.write(np.zeros(1, dtype=np.complex128))
        False

        """
        ...

    def wait(self, n: int) -> NDArray[np.complex128]:
        """Block until ``n`` samples are available; return zero-copy view.

        Spins with the GIL released until the producer has written at
        least ``n`` samples.  Returns a zero-copy 1-D complex128 view
        directly into the ring buffer.  Caller must call
        :meth:`consume` before the next ``wait``.

        Parameters
        ----------
        n : int
            Number of complex samples to wait for.

        Returns
        -------
        ndarray of complex128, shape (n,)
            Zero-copy view into the ring buffer.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(512)
        >>> buf.write(np.array([1+2j, 3+4j], dtype=np.complex128))
        True
        >>> view = buf.wait(2)
        >>> view.dtype
        dtype('complex128')
        >>> view.shape
        (2,)
        >>> view.tolist()
        [(1+2j), (3+4j)]
        >>> buf.consume()

        """
        ...

    def consume(self, n: int = ...) -> None:
        """Release ``n`` samples back to the producer.

        Advances the consumer tail pointer.  If ``n`` is omitted, the
        count from the most recent :meth:`wait` call is used.

        Parameters
        ----------
        n : int, optional
            Number of samples to release.  Defaults to the last
            :meth:`wait` count.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(512)
        >>> buf.write(np.ones(4, dtype=np.complex128))
        True
        >>> _ = buf.wait(4)
        >>> buf.consume()

        """
        ...

    def destroy(self) -> None:
        """Unmap the buffer and free the underlying struct.

        Releases both virtual-address views and frees the C struct.
        Safe to call more than once; subsequent calls are no-ops.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> buf = F64Buffer(512)
        >>> buf.destroy()

        """
        ...

    @property
    def capacity(self) -> int:
        """Buffer capacity in complex samples.

        Read-only.  The *actual* allocated size: a sub-page request rounds up
        to the page-spanning minimum (256 on 4 KiB pages, 1024 on 16 KiB
        pages), so it may exceed the requested value.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> F64Buffer(512).capacity >= 512
        True

        """
        ...

    @property
    def available(self) -> int:
        """Samples written but not yet consumed.

        The largest ``n`` for which :meth:`wait` is guaranteed to return
        without spinning.  Read this rather than tracking the count
        yourself: :meth:`wait` has no timeout and no short return, so
        asking for more than has been written spins until the producer
        catches up -- forever, if there is no producer.

        Read from the consumer side this is a *lower* bound.  A producer
        on another thread can only increase it, so a block sized from it
        is always safe; it may simply be smaller than what has landed by
        the time :meth:`wait` runs.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(1024)
        >>> buf.available
        0
        >>> _ = buf.write(np.zeros(100, dtype=np.complex128))
        >>> buf.available
        100
        >>> _ = buf.wait(64); buf.consume(64)
        >>> buf.available
        36

        """
        ...

    @property
    def dropped(self) -> int:
        """Cumulative count of samples dropped due to buffer overrun.

        Incremented atomically by the number of samples in each
        rejected :meth:`write` batch.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(512)
        >>> buf.dropped
        0
        >>> buf.write(np.zeros(512, dtype=np.complex128))
        True
        >>> buf.write(np.zeros(1, dtype=np.complex128))
        False
        >>> buf.dropped
        1

        """
        ...

class I16Buffer:
    """Lock-free SPSC ring buffer for interleaved int16 IQ pairs.

    Stores raw 16-bit integer I/Q samples as they arrive from SDR
    hardware (e.g. RTL-SDR, HackRF) before conversion to floating
    point.  Uses the same virtual-memory double-mapping as
    :class:`F32Buffer` to give zero-copy, branchless access across the
    wrap boundary.

    :meth:`write` accepts a flat int16 array of length ``2*n``
    (interleaved I, Q, I, Q, …).  :meth:`wait` returns a zero-copy
    view with shape ``(n, 2)`` where column 0 is I and column 1 is Q,
    suitable for direct ``np.dot`` or ``view(np.int16)`` processing.

    Parameters
    ----------
    capacity : int
        Requested buffer size in IQ sample pairs.  Must be a power of two.
        ``capacity * 4`` must span a whole page; a sub-page request is rounded
        **up** to the smallest power-of-two that does (minimum 1024 on 4 KiB
        pages, 4096 on 16 KiB pages).  Read :attr:`capacity` back for the size
        actually allocated.

    Examples
    --------
    >>> from doppler.buffer import I16Buffer
    >>> import numpy as np
    >>> buf = I16Buffer(1024)
    >>> buf.capacity >= 1024
    True
    >>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16))
    True

    """

    def __init__(self, capacity: int) -> None: ...

    def write(self, arr: NDArray[np.int16]) -> bool:
        """Write interleaved int16 IQ samples without blocking.

        Accepts a flat int16 array of even length ``2*n`` or a
        C-contiguous 2-D array of shape ``(n, 2)`` — either layout is
        accepted because the total byte count determines ``n``.  The
        write is rejected atomically if the buffer has fewer than ``n``
        free slots; in that case ``dropped`` is incremented by ``n``.

        Parameters
        ----------
        arr : ndarray of int16
            IQ samples to write.  Total element count must be even.
            Must be C-contiguous; dtype must be int16.

        Returns
        -------
        bool
            ``True`` if all sample pairs were written; ``False`` if
            the buffer was full (all pairs dropped).

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> buf = I16Buffer(1024)
        >>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16))
        True
        >>> buf2 = I16Buffer(1024)
        >>> buf2.write(np.zeros(2048, dtype=np.int16))
        True
        >>> buf2.write(np.zeros(2, dtype=np.int16))
        False

        """
        ...

    def wait(self, n: int) -> NDArray[np.int16]:
        """Block until ``n`` IQ pairs are available; return shape (n, 2) view.

        Spins with the GIL released until the producer has written at
        least ``n`` IQ pairs.  Returns a zero-copy view with shape
        ``(n, 2)`` and dtype int16 directly into the double-mapped ring:
        ``view[:, 0]`` is the I channel; ``view[:, 1]`` is the Q
        channel.  Caller must call :meth:`consume` before the next
        ``wait``.

        Parameters
        ----------
        n : int
            Number of IQ sample pairs to wait for.

        Returns
        -------
        ndarray of int16, shape (n, 2)
            Zero-copy view; column 0 = I, column 1 = Q.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> buf = I16Buffer(1024)
        >>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16))
        True
        >>> view = buf.wait(2)
        >>> view.dtype
        dtype('int16')
        >>> view.shape
        (2, 2)
        >>> view.tolist()
        [[10, 20], [30, 40]]
        >>> buf.consume()

        """
        ...

    def consume(self, n: int = ...) -> None:
        """Release ``n`` IQ sample pairs back to the producer.

        Advances the consumer tail pointer by ``n`` pairs.  If ``n``
        is omitted, the count from the most recent :meth:`wait` call
        is used.

        Parameters
        ----------
        n : int, optional
            Number of IQ sample pairs to release.  Defaults to the
            last :meth:`wait` count.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> buf = I16Buffer(1024)
        >>> buf.write(np.array([1, 2, 3, 4], dtype=np.int16))
        True
        >>> _ = buf.wait(2)
        >>> buf.consume()

        """
        ...

    def destroy(self) -> None:
        """Unmap the buffer and free the underlying struct.

        Releases both virtual-address views and frees the C struct.
        Safe to call more than once; subsequent calls are no-ops.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> buf = I16Buffer(1024)
        >>> buf.destroy()

        """
        ...

    @property
    def capacity(self) -> int:
        """Buffer capacity in IQ sample pairs.

        Read-only.  The *actual* allocated size: a sub-page request rounds up
        to the page-spanning minimum (1024 on 4 KiB pages, 4096 on 16 KiB
        pages), so it may exceed the requested value.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> I16Buffer(1024).capacity >= 1024
        True

        """
        ...

    @property
    def available(self) -> int:
        """Samples written but not yet consumed.

        The largest ``n`` for which :meth:`wait` is guaranteed to return
        without spinning.  Read this rather than tracking the count
        yourself: :meth:`wait` has no timeout and no short return, so
        asking for more than has been written spins until the producer
        catches up -- forever, if there is no producer.

        Read from the consumer side this is a *lower* bound.  A producer
        on another thread can only increase it, so a block sized from it
        is always safe; it may simply be smaller than what has landed by
        the time :meth:`wait` runs.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> buf = I16Buffer(1024)
        >>> buf.available
        0
        >>> _ = buf.write(np.zeros((100, 2), dtype=np.int16))
        >>> buf.available
        100
        >>> _ = buf.wait(64); buf.consume(64)
        >>> buf.available
        36

        """
        ...

    @property
    def dropped(self) -> int:
        """Cumulative IQ sample pairs dropped due to buffer overrun.

        Incremented atomically by the number of pairs in each rejected
        :meth:`write` batch (i.e. ``len(arr) // 2``).

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> buf = I16Buffer(1024)
        >>> buf.dropped
        0
        >>> buf.write(np.zeros(2048, dtype=np.int16))
        True
        >>> buf.write(np.zeros(2, dtype=np.int16))
        False
        >>> buf.dropped
        1

        """
        ...
