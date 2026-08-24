# delay/delay.pyi — type stubs for the delay C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class DelayCf64:
    """Create a dual-buffer circular delay line of length num_taps. The
    internal capacity is rounded up to the next power of two so that modular
    indexing reduces to a single bitwise AND. Any window of num_taps
    consecutive samples is always contiguous in the backing store; no
    wrap-around copy is ever needed.

    Parameters
    ----------
    num_taps : int, default 1
        Number of delay taps (window length, >= 1). Internally rounded up to
        the next power of two.

    Examples
    --------
    >>> from doppler.delay import DelayCf64
    >>> d = DelayCf64(num_taps=3)
    >>> d.num_taps
    3
    >>> d.capacity   # next power-of-two >= 3
    4

    """
    def __init__(self, num_taps: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset the delay line to its post-create state. Zeroes the entire
        dual buffer and resets the write pointer to 0, discarding all
        previously pushed samples. The num_taps and capacity are preserved;
        only the sample history is cleared.

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
        """Advance the write pointer and insert a new sample. The head pointer
        decrements (mod capacity) before the write so that `buf[head]` always
        holds the most recent sample. The same value is simultaneously written
        at `buf[head + capacity]` to keep the mirror half in sync; this ensures
        any num_taps-length window starting at head is contiguous without an
        extra copy.

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
        self,
        count: int = ...,
        out: NDArray[np.complex128] | None = None,
    ) -> NDArray[np.complex128]:
        """Return a zero-copy view of the n most recent samples. Copies at most
        min(n, num_taps) samples starting from `buf[head]` into out. Because
        the dual-buffer layout guarantees contiguity, this is a single memcpy
        of up to num_taps elements; no wrap-around logic is needed. The Python
        binding returns a NumPy array backed directly by the pre-allocated
        output buffer (base object is the DelayCf64 itself).

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[np.complex128] | None
            Output buffer; must hold at least max_out elements.

        Returns
        -------
        NDArray[np.complex128]
            min(n, num_taps, max_out) samples.

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

    def ptr_max_out(self, n: int) -> int:
        """Maximum samples delay_ptr() writes for a request of n. Returns
        min(n, num_taps) — the tight per-call bound (gh-607).

        Parameters
        ----------
        n : int
            Number of samples the matching delay_ptr() call requests.

        Returns
        -------
        int
            min(n, num_taps).
        """

    def push_ptr(
        self,
        x: complex,
        out: NDArray[np.complex128] | None = None,
    ) -> NDArray[np.complex128]:
        """Atomically push a sample and snapshot the current window. Equivalent
        to calling delay_push() then delay_ptr(num_taps), but avoids the
        overhead of a second function call. Always writes exactly num_taps
        samples to out. The Python binding returns a NumPy array backed by the
        pre-allocated push_ptr output buffer.

        Parameters
        ----------
        x : complex
            New complex sample to insert.
        out : NDArray[np.complex128] | None
            Output buffer; must hold at least max_out elements.

        Returns
        -------
        NDArray[np.complex128]
            min(num_taps, max_out) samples.

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
        """Return the maximum output capacity for delay_push_ptr(). Returns
        num_taps; the Python binding uses this to pre-allocate the output
        buffer before calling delay_push_ptr().

        Returns
        -------
        int
            num_taps (number of samples delay_push_ptr() will write).
        """

    def write(self, x: complex) -> None:
        """Alias for delay_push(); insert a sample without reading back.
        Provided for API symmetry with write-then-read patterns where the
        caller wants to decouple sample ingestion from window inspection.
        Internally delegates to delay_push() with no additional overhead.

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
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the DelayCf64 has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the DelayCf64 has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the DelayCf64 has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def num_taps(self) -> int:
        """Num taps."""

    @property
    def capacity(self) -> int:
        """Capacity."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "DelayCf64":
        """Enter a context manager, returning this object.

        Lets a DelayCf64 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        DelayCf64
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the DelayCf64.

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
