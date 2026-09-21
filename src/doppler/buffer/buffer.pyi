# buffer/buffer.pyi — type stubs for the buffer C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class F32Buffer:
    """Lock-free SPSC ring buffer for complex64 (CF32) samples.

    Parameters
    ----------
    capacity : int
        Requested buffer size in complex samples. Must be a power of two. The
        VM mirror is built at page granularity, so ``capacity * 8`` must span a
        whole page; a sub-page request is rounded **up** to the smallest
        power-of-two that does (minimum 512 on 4 KiB pages, 2048 on 16 KiB
        pages such as macOS arm64). Read :attr:`capacity` back for the size
        actually allocated.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``capacity must be a
        power of two (and the mapping must succeed)``.

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

    def write(self, x: NDArray[np.complex64]) -> bool:
        """Write samples into the buffer without blocking.

        Copies the complex64 array into the ring buffer in a single ``memcpy``.
        If there is not enough free space for all ``len(x)`` samples the call
        is **refused entirely** — nothing is copied and ``x`` is untouched, so
        you still hold every sample and may retry once the consumer has made
        room. Nothing is dropped unless you discard it; the refusal is counted
        in :attr:`dropped`, which is not a loss count. The array must be 1-D
        and C-contiguous.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Samples to write. Must be 1-D and C-contiguous.

        Returns
        -------
        bool
            ``True`` if all samples were written; ``False`` if the ring had no
            room and the call was refused (``x`` untouched).

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

    def write_some(self, x: NDArray[np.complex64]) -> int:
        """Write as much of ``x`` as fits and say how much that was.

        The partial-write twin of :meth:`write`. Where :meth:`write` refuses a
        block that does not fit whole, this takes the leading samples that do
        and returns their count -- ``0`` when the ring is full. It never
        refuses, so it never touches :attr:`dropped`. It is the only way to
        feed a chunk larger than the ring: loop, advancing by the return value,
        draining in between.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Samples to write. Must be 1-D and C-contiguous.

        Returns
        -------
        int
            Samples accepted, ``0 <= k <= len(x)``. The caller still owns
            ``x[k:]``.

        Examples
        --------
        A chunk three times the size of the ring, fed by looping:

        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> cap = buf.capacity
        >>> chunk = np.ones(3 * cap, dtype=np.complex64)
        >>> fed = 0
        >>> while fed < len(chunk):
        ...     fed += buf.write_some(chunk[fed:])
        ...     _ = buf.peek(buf.available); buf.consume()
        >>> fed == 3 * cap, buf.dropped
        (True, 0)

        """

    def wait(self, n: int) -> NDArray[np.complex64]:
        """Block until ``n`` samples are available, then lend a zero-copy view.

        Spins (releasing the GIL so a producer thread can run concurrently)
        until at least ``n`` samples have been written by the producer. Returns
        a 1-D complex64 NumPy array that is a *direct view* into the
        double-mapped ring buffer — no data is copied. Because of the
        double-mapping, the view is always contiguous even when the requested
        range wraps around the physical end of the ring.

        The caller **must** call :meth:`consume` before the next call to
        ``wait``. Using the returned array after ``consume`` is undefined
        behaviour; the producer may overwrite it at any time.

        Parameters
        ----------
        n : int
            Number of complex samples to wait for. Must be positive and not
            larger than :attr:`capacity`.

        Returns
        -------
        NDArray[np.complex64]
            Zero-copy view of the next ``n`` samples in the ring.

        Raises
        ------
        EOFError
            The producer called :meth:`close` and fewer than ``n`` samples
            remain. The tail is drained and no more is coming, so the wait ends
            rather than blocking forever.
        KeyboardInterrupt
            Somebody asked this process to stop, through a
            :class:`doppler.interrupt.Interrupt` guard -- from any module: the
            flag is process-wide. Without a guard the spin checks for no
            signals at all.

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

    def peek(self, n: int) -> NDArray[np.complex64] | None:
        """:meth:`wait` that never blocks: a view, or None for not yet.

        The single-threaded consumer's read. :meth:`wait` spins until a
        producer on *another* thread delivers, so a caller that is its own
        producer would deadlock in it; ``peek`` answers at once instead. When
        ``n`` samples are buffered it returns the same zero-copy,
        always-contiguous view :meth:`wait` would (1-D complex64); otherwise it
        returns ``None``.

        ``None`` means **not yet** and nothing else. The two conditions no
        amount of waiting can cure are raised, exactly as :meth:`wait` raises
        them, so a poll loop cannot mistake either for a slow producer.

        Peeking does not consume. Follow it with :meth:`consume`; a
        ``consume(k)`` with ``k < n`` advances by a hop smaller than the frame,
        which is how overlapped frames are read.

        Parameters
        ----------
        n : int
            Number of samples wanted. Must be positive and not larger than
            :attr:`capacity`.

        Returns
        -------
        NDArray[np.complex64] | None
            Zero-copy view of the next ``n`` samples, or ``None`` when fewer
            than ``n`` have been written so far.

        Raises
        ------
        EOFError
            The ring is closed and fewer than ``n`` samples remain: the rest is
            never coming.
        ValueError
            ``n`` exceeds :attr:`capacity` (or is not positive), so no producer
            could ever satisfy it.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> buf.peek(4) is None
        True
        >>> buf.write_some(np.ones(8, dtype=np.complex64))
        8
        >>> buf.peek(4).shape
        (4,)
        >>> buf.consume(2)
        >>> buf.available
        6
        >>> buf.close()
        >>> buf.peek(8)
        Traceback (most recent call last):
            ...
        EOFError: end of stream: the producer closed the ring

        """

    def consume(self, n: int = ...) -> None:
        """Release ``n`` samples back to the producer.

        Advances the consumer tail pointer by ``n``, making that space
        available for the producer to overwrite, and ends the loan: the view a
        :meth:`wait` or :meth:`peek` lent must not be used afterwards. If ``n``
        is omitted it is the count of that outstanding view, so the number is
        written once. ``n`` smaller than the view is how overlapped frames are
        read: release a hop, keep the rest.

        Parameters
        ----------
        n : int
            Number of samples to release. Defaults to the count of the
            outstanding :meth:`wait` / :meth:`peek` view.

        Raises
        ------
        RuntimeError
            ``n`` was omitted and nothing is outstanding -- no view was lent
            since the last release, so there is no count to default to.

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

    def close(self) -> None:
        """Say that no more data is coming.

        The producer's half of end of stream. Until this exists a consumer
        cannot tell a slow producer from a finished one -- both look like an
        empty ring -- so :meth:`wait` had nothing to do but spin. Call it once,
        after the last write.

        Release ordering: every sample written before this is visible to a
        consumer that observes the flag. Closing does not discard what was
        already written; :meth:`wait` keeps returning batches until the ring is
        drained, and only then raises ``EOFError``.

        See ``docs/design/io-termination.md`` for the one termination contract
        shared with the network and disk transports.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.buffer import F32Buffer
        >>> buf = F32Buffer(1024)
        >>> buf.close()
        >>> buf.closed
        True
        >>> buf.wait(4)
        Traceback (most recent call last):
            ...
        EOFError: end of stream: the producer closed the ring

        """

    def reset(self) -> None:
        """Empty the ring and reopen it.

        Discards everything buffered, and clears :attr:`closed` so the same
        ring can carry a second stream -- without it, reuse after :meth:`close`
        means destroying and re-mapping. :attr:`dropped` is a lifetime count
        and is kept.

        Not safe against a concurrent producer or consumer: it moves both ends
        of the ring. Call it only when both sides are idle.

        Examples
        --------
        >>> from doppler.buffer import F32Buffer
        >>> import numpy as np
        >>> buf = F32Buffer(1024)
        >>> buf.write_some(np.ones(8, dtype=np.complex64))
        8
        >>> buf.close()
        >>> buf.reset()
        >>> buf.available, buf.closed
        (0, False)

        """

    @property
    def capacity(self) -> int:
        """Buffer capacity in complex samples."""

    @property
    def available(self) -> int:
        """Samples written but not yet consumed."""

    @property
    def space(self) -> int:
        """Free room in samples: the largest :meth:`write` sure to fit."""

    @property
    def dropped(self) -> int:
        """Cumulative samples in REFUSED writes -- not samples lost."""

    @property
    def closed(self) -> bool:
        """``True`` once the producer has called :meth:`close`."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "F32Buffer":
        """Enter a context manager, returning this object.

        Lets a F32Buffer be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        F32Buffer
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the F32Buffer.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class F64Buffer:
    """Lock-free SPSC ring buffer for complex128 (CF64) samples.

    Parameters
    ----------
    capacity : int
        Requested buffer size in complex samples. Must be a power of two.
        ``capacity * 16`` must span a whole page; a sub-page request is rounded
        **up** to the smallest power-of-two that does (minimum 256 on 4 KiB
        pages, 1024 on 16 KiB pages). Read :attr:`capacity` back for the size
        actually allocated.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``capacity must be a
        power of two (and the mapping must succeed)``.

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

    def write(self, x: NDArray[np.complex128]) -> bool:
        """Write complex128 samples into the buffer without blocking.

        Copies the entire array in a single ``memcpy``. Rejects the write
        atomically if there is insufficient free space; the call is refused
        whole -- nothing copied, ``x`` untouched -- and :attr:`dropped` grows
        by ``len(x)``. The array must be 1-D and C-contiguous.

        Parameters
        ----------
        x : NDArray[np.complex128]
            Samples to write. Must be 1-D and C-contiguous.

        Returns
        -------
        bool
            ``True`` if all samples were written; ``False`` if the ring was
            full and the call was refused (``x`` untouched).

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

    def write_some(self, x: NDArray[np.complex128]) -> int:
        """Write as much of ``x`` as fits and say how much that was.

        The partial-write twin of :meth:`write`. Where :meth:`write` refuses a
        block that does not fit whole, this takes the leading samples that do
        and returns their count -- ``0`` when the ring is full. It never
        refuses, so it never touches :attr:`dropped`. It is the only way to
        feed a chunk larger than the ring: loop, advancing by the return value,
        draining in between.

        Parameters
        ----------
        x : NDArray[np.complex128]
            Samples to write. Must be 1-D and C-contiguous.

        Returns
        -------
        int
            Samples accepted, ``0 <= k <= len(x)``. The caller still owns
            ``x[k:]``.

        Examples
        --------
        A chunk three times the size of the ring, fed by looping:

        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(1024)
        >>> cap = buf.capacity
        >>> chunk = np.ones(3 * cap, dtype=np.complex128)
        >>> fed = 0
        >>> while fed < len(chunk):
        ...     fed += buf.write_some(chunk[fed:])
        ...     _ = buf.peek(buf.available); buf.consume()
        >>> fed == 3 * cap, buf.dropped
        (True, 0)

        """

    def wait(self, n: int) -> NDArray[np.complex128]:
        """Block until ``n`` samples are available; return zero-copy view.

        Spins with the GIL released until the producer has written at least
        ``n`` samples. Returns a zero-copy 1-D complex128 view directly into
        the ring buffer. Caller must call :meth:`consume` before the next
        ``wait``.

        Parameters
        ----------
        n : int
            Number of complex samples to wait for.

        Returns
        -------
        NDArray[np.complex128]
            Zero-copy view into the ring buffer.

        Raises
        ------
        EOFError
            The producer called :meth:`close` and fewer than ``n`` samples
            remain. The tail is drained and no more is coming, so the wait ends
            rather than blocking forever.
        KeyboardInterrupt
            Somebody asked this process to stop, through a
            :class:`doppler.interrupt.Interrupt` guard -- from any module: the
            flag is process-wide. Without a guard the spin checks for no
            signals at all.

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

    def peek(self, n: int) -> NDArray[np.complex128] | None:
        """:meth:`wait` that never blocks: a view, or None for not yet.

        The single-threaded consumer's read. :meth:`wait` spins until a
        producer on *another* thread delivers, so a caller that is its own
        producer would deadlock in it; ``peek`` answers at once instead. When
        ``n`` samples are buffered it returns the same zero-copy,
        always-contiguous view :meth:`wait` would (1-D complex128); otherwise
        it returns ``None``.

        ``None`` means **not yet** and nothing else. The two conditions no
        amount of waiting can cure are raised, exactly as :meth:`wait` raises
        them, so a poll loop cannot mistake either for a slow producer.

        Peeking does not consume. Follow it with :meth:`consume`; a
        ``consume(k)`` with ``k < n`` advances by a hop smaller than the frame,
        which is how overlapped frames are read.

        Parameters
        ----------
        n : int
            Number of samples wanted. Must be positive and not larger than
            :attr:`capacity`.

        Returns
        -------
        NDArray[np.complex128] | None
            Zero-copy view of the next ``n`` samples, or ``None`` when fewer
            than ``n`` have been written so far.

        Raises
        ------
        EOFError
            The ring is closed and fewer than ``n`` samples remain: the rest is
            never coming.
        ValueError
            ``n`` exceeds :attr:`capacity` (or is not positive), so no producer
            could ever satisfy it.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(1024)
        >>> buf.peek(4) is None
        True
        >>> buf.write_some(np.ones(8, dtype=np.complex128))
        8
        >>> buf.peek(4).shape
        (4,)
        >>> buf.consume(2)
        >>> buf.available
        6
        >>> buf.close()
        >>> buf.peek(8)
        Traceback (most recent call last):
            ...
        EOFError: end of stream: the producer closed the ring

        """

    def consume(self, n: int = ...) -> None:
        """Release ``n`` samples back to the producer.

        Advances the consumer tail pointer by ``n``, making that space
        available for the producer to overwrite, and ends the loan: the view a
        :meth:`wait` or :meth:`peek` lent must not be used afterwards. If ``n``
        is omitted it is the count of that outstanding view, so the number is
        written once. ``n`` smaller than the view is how overlapped frames are
        read: release a hop, keep the rest.

        Parameters
        ----------
        n : int
            Number of samples to release. Defaults to the count of the
            outstanding :meth:`wait` / :meth:`peek` view.

        Raises
        ------
        RuntimeError
            ``n`` was omitted and nothing is outstanding -- no view was lent
            since the last release, so there is no count to default to.

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

    def close(self) -> None:
        """Say that no more data is coming.

        The producer's half of end of stream. Until this exists a consumer
        cannot tell a slow producer from a finished one -- both look like an
        empty ring -- so :meth:`wait` had nothing to do but spin. Call it once,
        after the last write.

        Release ordering: every sample written before this is visible to a
        consumer that observes the flag. Closing does not discard what was
        already written; :meth:`wait` keeps returning batches until the ring is
        drained, and only then raises ``EOFError``.

        See ``docs/design/io-termination.md`` for the one termination contract
        shared with the network and disk transports.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.buffer import F64Buffer
        >>> buf = F64Buffer(1024)
        >>> buf.close()
        >>> buf.closed
        True
        >>> buf.wait(4)
        Traceback (most recent call last):
            ...
        EOFError: end of stream: the producer closed the ring

        """

    def reset(self) -> None:
        """Empty the ring and reopen it.

        Discards everything buffered, and clears :attr:`closed` so the same
        ring can carry a second stream -- without it, reuse after :meth:`close`
        means destroying and re-mapping. :attr:`dropped` is a lifetime count
        and is kept.

        Not safe against a concurrent producer or consumer: it moves both ends
        of the ring. Call it only when both sides are idle.

        Examples
        --------
        >>> from doppler.buffer import F64Buffer
        >>> import numpy as np
        >>> buf = F64Buffer(1024)
        >>> buf.write_some(np.ones(8, dtype=np.complex128))
        8
        >>> buf.close()
        >>> buf.reset()
        >>> buf.available, buf.closed
        (0, False)

        """

    @property
    def capacity(self) -> int:
        """Buffer capacity in complex samples."""

    @property
    def available(self) -> int:
        """Samples written but not yet consumed."""

    @property
    def space(self) -> int:
        """Free room in samples: the largest :meth:`write` sure to fit."""

    @property
    def dropped(self) -> int:
        """Cumulative samples in REFUSED writes -- not samples lost."""

    @property
    def closed(self) -> bool:
        """``True`` once the producer has called :meth:`close`."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "F64Buffer":
        """Enter a context manager, returning this object.

        Lets a F64Buffer be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        F64Buffer
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the F64Buffer.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class I16Buffer:
    """Lock-free SPSC ring buffer for interleaved int16 IQ pairs.

    Parameters
    ----------
    capacity : int
        Requested buffer size in IQ sample pairs. Must be a power of two.
        ``capacity * 4`` must span a whole page; a sub-page request is rounded
        **up** to the smallest power-of-two that does (minimum 1024 on 4 KiB
        pages, 4096 on 16 KiB pages). Read :attr:`capacity` back for the size
        actually allocated.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``capacity must be a
        power of two (and the mapping must succeed)``.

    Examples
    --------
    >>> from doppler.buffer import I16Buffer
    >>> import numpy as np
    >>> buf = I16Buffer(1024)
    >>> buf.capacity >= 1024
    True
    >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
    >>> adc = np.array([10, 20, 30, 40], dtype=np.int16)   # I, Q, I, Q
    >>> buf.write(adc.view(IQ16))
    True
    >>> buf.wait(2)["q"].tolist()
    [20, 40]

    """
    def __init__(self, capacity: int) -> None: ...

    def write(self, x: NDArray[Any]) -> bool:
        """Write IQ samples into the buffer without blocking.

        Copies the record array into the ring in a single ``memcpy``. With
        fewer than ``len(x)`` free slots the call is **refused entirely** --
        nothing copied, ``x`` untouched -- and :attr:`dropped` grows by
        ``len(x)``, which is not a loss count: you still hold every sample.

        A bare int16 array is refused with ``TypeError``, flat or ``(n, 2)``:
        it is not an array of samples. ``flat.view(IQ16)`` makes it one, with
        no copy.

        Parameters
        ----------
        x : NDArray[Any]
            IQ samples to write: 1-D, C-contiguous, dtype ``[("i", "<i2"),
            ("q", "<i2")]``.

        Returns
        -------
        bool
            ``True`` if all samples were written; ``False`` if the ring had no
            room and the call was refused (``x`` untouched).

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> buf = I16Buffer(1024)
        >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
        >>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16).view(IQ16))
        True
        >>> buf2 = I16Buffer(1024)
        >>> buf2.write(np.zeros(1024, dtype=IQ16))
        True
        >>> buf2.write(np.zeros(1, dtype=IQ16))
        False

        """

    def write_some(self, x: NDArray[Any]) -> int:
        """Write as much of ``x`` as fits and say how much that was.

        The partial-write twin of :meth:`write`. Where :meth:`write` refuses a
        block that does not fit whole, this takes the leading samples that do
        and returns their count -- ``0`` when the ring is full. It never
        refuses, so it never touches :attr:`dropped`. It is the only way to
        feed a chunk larger than the ring: loop, advancing by the return value,
        draining in between.

        Parameters
        ----------
        x : NDArray[Any]
            Samples to write: 1-D, C-contiguous, dtype ``[("i", "<i2"), ("q",
            "<i2")]``.

        Returns
        -------
        int
            Samples accepted, ``0 <= k <= len(x)``. The caller still owns
            ``x[k:]``.

        Examples
        --------
        A chunk three times the size of the ring, fed by looping:

        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
        >>> buf = I16Buffer(1024)
        >>> cap = buf.capacity
        >>> chunk = np.ones(3 * cap, dtype=IQ16)
        >>> fed = 0
        >>> while fed < len(chunk):
        ...     fed += buf.write_some(chunk[fed:])
        ...     _ = buf.peek(buf.available); buf.consume()
        >>> fed == 3 * cap, buf.dropped
        (True, 0)

        """

    def wait(self, n: int) -> NDArray[Any]:
        """Block until ``n`` samples are available, then lend a zero-copy view.

        Spins with the GIL released until the producer has written at least
        ``n`` samples. Returns a 1-D record array directly into the
        double-mapped ring: ``view["i"]`` is the I channel, ``view["q"]`` the Q
        channel, each a strided int16 view with no copy. Caller must call
        :meth:`consume` before the next ``wait``.

        Parameters
        ----------
        n : int
            Number of IQ sample pairs to wait for.

        Returns
        -------
        NDArray[Any]
            Zero-copy view of the next ``n`` samples, one record each.

        Raises
        ------
        EOFError
            The producer called :meth:`close` and fewer than ``n`` samples
            remain. The tail is drained and no more is coming, so the wait ends
            rather than blocking forever.
        KeyboardInterrupt
            Somebody asked this process to stop, through a
            :class:`doppler.interrupt.Interrupt` guard -- from any module: the
            flag is process-wide. Without a guard the spin checks for no
            signals at all.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> buf = I16Buffer(1024)
        >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
        >>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16).view(IQ16))
        True
        >>> view = buf.wait(2)
        >>> view.dtype
        dtype([('i', '<i2'), ('q', '<i2')])
        >>> view.shape
        (2,)
        >>> view.tolist()
        [(10, 20), (30, 40)]
        >>> buf.consume()

        """

    def peek(self, n: int) -> NDArray[Any] | None:
        """:meth:`wait` that never blocks: a view, or None for not yet.

        The single-threaded consumer's read. :meth:`wait` spins until a
        producer on *another* thread delivers, so a caller that is its own
        producer would deadlock in it; ``peek`` answers at once instead. When
        ``n`` samples are buffered it returns the same zero-copy,
        always-contiguous view :meth:`wait` would (1-D, one ``(i, q)`` record
        per sample); otherwise it returns ``None``.

        ``None`` means **not yet** and nothing else. The two conditions no
        amount of waiting can cure are raised, exactly as :meth:`wait` raises
        them, so a poll loop cannot mistake either for a slow producer.

        Peeking does not consume. Follow it with :meth:`consume`; a
        ``consume(k)`` with ``k < n`` advances by a hop smaller than the frame,
        which is how overlapped frames are read.

        Parameters
        ----------
        n : int
            Number of samples wanted. Must be positive and not larger than
            :attr:`capacity`.

        Returns
        -------
        NDArray[Any] | None
            Zero-copy view of the next ``n`` samples, or ``None`` when fewer
            than ``n`` have been written so far.

        Raises
        ------
        EOFError
            The ring is closed and fewer than ``n`` samples remain: the rest is
            never coming.
        ValueError
            ``n`` exceeds :attr:`capacity` (or is not positive), so no producer
            could ever satisfy it.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
        >>> buf = I16Buffer(1024)
        >>> buf.peek(4) is None
        True
        >>> buf.write_some(np.ones(8, dtype=IQ16))
        8
        >>> buf.peek(4).shape
        (4,)
        >>> buf.consume(2)
        >>> buf.available
        6
        >>> buf.close()
        >>> buf.peek(8)
        Traceback (most recent call last):
            ...
        EOFError: end of stream: the producer closed the ring

        """

    def consume(self, n: int = ...) -> None:
        """Release ``n`` samples back to the producer.

        Advances the consumer tail pointer by ``n``, making that space
        available for the producer to overwrite, and ends the loan: the view a
        :meth:`wait` or :meth:`peek` lent must not be used afterwards. If ``n``
        is omitted it is the count of that outstanding view, so the number is
        written once. ``n`` smaller than the view is how overlapped frames are
        read: release a hop, keep the rest.

        Parameters
        ----------
        n : int
            Number of samples to release. Defaults to the count of the
            outstanding :meth:`wait` / :meth:`peek` view.

        Raises
        ------
        RuntimeError
            ``n`` was omitted and nothing is outstanding -- no view was lent
            since the last release, so there is no count to default to.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
        >>> buf = I16Buffer(1024)
        >>> buf.write(np.array([1, 2, 3, 4], dtype=np.int16).view(IQ16))
        True
        >>> _ = buf.wait(2)
        >>> buf.consume()

        """

    def close(self) -> None:
        """Say that no more data is coming.

        The producer's half of end of stream. Until this exists a consumer
        cannot tell a slow producer from a finished one -- both look like an
        empty ring -- so :meth:`wait` had nothing to do but spin. Call it once,
        after the last write.

        Release ordering: every sample written before this is visible to a
        consumer that observes the flag. Closing does not discard what was
        already written; :meth:`wait` keeps returning batches until the ring is
        drained, and only then raises ``EOFError``.

        See ``docs/design/io-termination.md`` for the one termination contract
        shared with the network and disk transports.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.buffer import I16Buffer
        >>> buf = I16Buffer(1024)
        >>> buf.close()
        >>> buf.closed
        True
        >>> buf.wait(4)
        Traceback (most recent call last):
            ...
        EOFError: end of stream: the producer closed the ring

        """

    def reset(self) -> None:
        """Empty the ring and reopen it.

        Discards everything buffered, and clears :attr:`closed` so the same
        ring can carry a second stream -- without it, reuse after :meth:`close`
        means destroying and re-mapping. :attr:`dropped` is a lifetime count
        and is kept.

        Not safe against a concurrent producer or consumer: it moves both ends
        of the ring. Call it only when both sides are idle.

        Examples
        --------
        >>> from doppler.buffer import I16Buffer
        >>> import numpy as np
        >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
        >>> buf = I16Buffer(1024)
        >>> buf.write_some(np.ones(8, dtype=IQ16))
        8
        >>> buf.close()
        >>> buf.reset()
        >>> buf.available, buf.closed
        (0, False)

        """

    @property
    def capacity(self) -> int:
        """Buffer capacity in IQ sample pairs."""

    @property
    def available(self) -> int:
        """Samples written but not yet consumed."""

    @property
    def space(self) -> int:
        """Free room in samples: the largest :meth:`write` sure to fit."""

    @property
    def dropped(self) -> int:
        """Cumulative IQ sample pairs in REFUSED writes -- not pairs lost."""

    @property
    def closed(self) -> bool:
        """``True`` once the producer has called :meth:`close`."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "I16Buffer":
        """Enter a context manager, returning this object.

        Lets a I16Buffer be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        I16Buffer
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the I16Buffer.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """
