# accumulator/accumulator.pyi — type stubs for the accumulator C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class AccF32:
    """Single-precision floating-point scalar accumulator. Maintains one
    running sum (``acc``) that persists across calls to ``step``, ``steps``,
    ``madd``, ``add2d``, and ``madd2d``. Use ``get`` to read without
    side-effects or ``dump`` to read and atomically zero in a single call.

    Parameters
    ----------
    acc : float, default 0.0
        acc state variable.

    Examples
    --------
    >>> from doppler.accumulator import AccF32
    >>> obj = AccF32(0.0)
    >>> obj.get_acc()
    0.0
    >>> obj.set_acc(5.0)
    >>> obj.get_acc()
    5.0
    >>> obj.reset()
    >>> obj.get_acc()
    0.0

    """
    def __init__(self, acc: float = 0.0) -> None: ...

    def reset(self) -> None:
        """Zero the accumulator, restoring the same state as a fresh
        ``AccF32(0.0)`` — regardless of the value supplied to
        ``acc_f32_create``. Subsequent ``get`` / ``dump`` calls return ``0.0``
        until new samples are processed.

        Examples
        --------
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> obj.step(7.0)
        >>> obj.reset()
        >>> obj.get_acc()
        0.0

        """

    def step(self, x: float) -> None:
        """Add one sample to the running sum (``acc += x``). This is the
        hot-path entry point for sample-by-sample processing. For block inputs
        prefer ``acc_f32_steps`` to amortise call overhead and allow
        auto-vectorisation.

        Parameters
        ----------
        x : float
            Input sample (float).

        Examples
        --------
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> obj.step(3.0)
        >>> obj.get()
        3.0

        """

    def steps(self, x: NDArray[np.float32]) -> None:
        """Add all samples in ``input`` to the running sum. Equivalent to
        calling ``acc_f32_step`` for each element, but SIMD-vectorised on
        platforms that provide it (AVX-512 / AVX2 / SSE2). The loop uses
        JM_RESTRICT so the compiler can assume no aliasing between ``state``
        and ``input``.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> obj.steps(np.array([1.0, 2.0, 3.0], dtype=np.float32))
        >>> obj.get()
        6.0

        """

    def get(self) -> float:
        """Return the current accumulated sum without resetting state.
        Identical to reading the ``acc`` property directly; retained as an
        explicit method so call sites that need the value can be uniform with
        ``dump`` without a conditional.

        Returns
        -------
        float
            Current value of ``acc`` (float).

        Examples
        --------
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> obj.step(2.0)
        >>> obj.step(3.0)
        >>> obj.get()
        5.0

        """

    def dump(self) -> float:
        """Return the accumulated sum and atomically reset it to zero. This is
        the canonical "drain" primitive: read the period total, then start a
        fresh accumulation interval without a separate ``reset`` call. The
        zero-reset is unconditional and always writes 0.0f.

        Returns
        -------
        float
            Value of ``acc`` just before the reset (float).

        Examples
        --------
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> obj.step(3.0)
        >>> obj.step(4.0)
        >>> obj.dump()
        7.0
        >>> obj.get()
        0.0

        """

    def madd(self, x: NDArray[np.float32], h: NDArray[np.float32]) -> None:
        """Dot-product accumulate: ``acc += sum(x[i] * h[i])`` for ``i`` in ``0
        .. min(x_len, h_len) - 1``. The shorter of the two arrays limits the
        iteration count; no out-of-bounds access occurs. Typical use: apply a
        short FIR weight vector to one block of signal samples and fold the
        result into a running total.

        Parameters
        ----------
        x : NDArray[np.float32]
            Signal samples (float32 array).
        h : NDArray[np.float32]
            Coefficient / weight array (float32 array).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> x = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
        >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
        >>> obj.madd(x, h)
        >>> obj.get()
        5.0

        """

    def add2d(self, x: NDArray[np.float32]) -> None:
        """Sum all elements of a (logically) 2-D float array into the
        accumulator. The array is treated as a flat C-order buffer of ``x_len``
        floats regardless of the original shape; the caller is responsible for
        passing the total element count.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input array (float32, any shape — passed as flat buffer).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> grid = np.array([[1.0, 2.0], [3.0, 4.0]], dtype=np.float32)
        >>> obj.add2d(grid)
        >>> obj.get()
        10.0

        """

    def madd2d(self, x: NDArray[np.float32], h: NDArray[np.float32]) -> None:
        """Dot-product accumulate over a flat 2-D buffer: ``acc += sum(x[i] *
        h[i])`` for ``i`` in ``0 .. min(x_len, h_len) - 1``. Combines ``add2d``
        and ``madd`` semantics — a 2-D signal array is weighted element-wise by
        a coefficient buffer and the scalar total is folded into the running
        sum.

        Parameters
        ----------
        x : NDArray[np.float32]
            Signal samples (float32, flat buffer of the 2-D array).
        h : NDArray[np.float32]
            Coefficient / weight array (float32).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> x = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
        >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
        >>> obj.madd2d(x, h)
        >>> obj.get()
        5.0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the AccF32 has already been destroyed.

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

        Raises ``RuntimeError`` if the AccF32 has already been destroyed.

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
        ``RuntimeError`` if the AccF32 has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """
    def get_acc(self) -> float:
        """Return the current accumulator value without modifying state. Use
        this when you need to read the running sum mid-accumulation without
        disturbing it. For a read-and-reset in one call use ``acc_f32_dump``.

        Returns
        -------
        float
            Current value of ``acc`` (float).

        Examples
        --------
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> obj.step(4.0)
        >>> obj.get_acc()          # non-destructive read
        4.0
        >>> obj.get_acc()          # still there — get_acc never drains
        4.0

        """

    def set_acc(self, value: float) -> None:
        """Overwrite the accumulator with a new value. Useful for seeding the
        accumulator to a known baseline before processing a new segment without
        a full ``reset``; subsequent ``step`` / ``steps`` samples accumulate on
        top of the seeded value.

        Parameters
        ----------
        value : float
            New accumulator value.

        Examples
        --------
        >>> from doppler.accumulator import AccF32
        >>> obj = AccF32(0.0)
        >>> obj.set_acc(10.0)      # seed a running baseline
        >>> obj.step(2.5)          # later samples fold in on top
        >>> obj.get_acc()
        12.5

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "AccF32":
        """Enter a context manager, returning this object.

        Lets a AccF32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        AccF32
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the AccF32.

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
class AccCf64:
    """Double-precision complex scalar accumulator. Maintains one running
    complex sum (``acc``) across calls to ``step``, ``steps``, ``madd``,
    ``add2d``, and ``madd2d``. The signal path is double-precision complex
    (128-bit per sample); coefficient arrays for ``madd``/``madd2d`` are
    single-precision float to match typical FIR weight storage. Use ``get`` to
    read without side-effects or ``dump`` to read and zero atomically.

    Parameters
    ----------
    acc : complex, default 0j
        acc state variable.

    Examples
    --------
    >>> from doppler.accumulator import AccCf64
    >>> obj = AccCf64(0j)
    >>> obj.get_acc()
    0j
    >>> obj.set_acc(3+4j)
    >>> obj.get_acc()
    (3+4j)
    >>> obj.reset()
    >>> obj.get_acc()
    0j

    """
    def __init__(self, acc: complex = 0j) -> None: ...

    def reset(self) -> None:
        """Zero the accumulator, restoring the same state as a fresh
        ``AccCf64(0j)`` — regardless of the value supplied to
        ``acc_cf64_create``. Both the real and imaginary parts are set to 0.0.
        Subsequent ``get`` / ``dump`` calls return ``0j`` until new samples are
        processed.

        Examples
        --------
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> obj.step(3+2j)
        >>> obj.reset()
        >>> obj.get_acc()
        0j

        """

    def step(self, x: complex) -> None:
        """Add one complex sample to the running sum (``acc += x``). This is
        the hot-path entry for sample-by-sample processing. For block inputs
        prefer ``acc_cf64_steps`` to amortise call overhead.

        Parameters
        ----------
        x : complex
            Input sample (complex).

        Examples
        --------
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> obj.step(3+2j)
        >>> obj.get()
        (3+2j)

        """

    def steps(self, x: NDArray[np.complex128]) -> None:
        """Add all samples in ``input`` to the running sum. Equivalent to
        calling ``acc_cf64_step`` for each element; iterates element-by-element
        over double-precision complex samples.

        Parameters
        ----------
        x : NDArray[np.complex128]
            Input.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> obj.steps(np.array([1+0j, 2+1j, 3+2j], dtype=np.complex128))
        >>> obj.get()
        (6+3j)

        """

    def get(self) -> complex:
        """Return the current accumulated sum without resetting state.
        Identical to reading the ``acc`` property directly; retained as an
        explicit method so call sites that need the value can be uniform with
        ``dump`` without a conditional.

        Returns
        -------
        complex
            Current value of ``acc`` (complex).

        Examples
        --------
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> obj.step(2+0j)
        >>> obj.step(0+3j)
        >>> obj.get()
        (2+3j)

        """

    def dump(self) -> complex:
        """Return the accumulated sum and atomically reset it to zero. This is
        the canonical "drain" primitive: read the period total, then start a
        fresh accumulation interval without a separate ``reset`` call. Both
        real and imaginary parts are zeroed unconditionally.

        Returns
        -------
        complex
            Value of ``acc`` just before the reset (complex).

        Examples
        --------
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> obj.step(3+2j)
        >>> obj.step(1+1j)
        >>> obj.dump()
        (4+3j)
        >>> obj.get()
        0j

        """

    def madd(self, x: NDArray[np.complex128], h: NDArray[np.float32]) -> None:
        """Dot-product accumulate with complex signal and float weights: ``acc
        += sum(x[i] * h[i])`` for ``i`` in ``0 .. min(x_len, h_len) - 1``. The
        signal array ``x`` is double-precision complex; the coefficient array
        ``h`` is single-precision float (widened to double before
        multiplication). The shorter of the two arrays limits iteration.

        Parameters
        ----------
        x : NDArray[np.complex128]
            Complex signal samples (complex128 array).
        h : NDArray[np.float32]
            Real coefficient / weight array (float32 array).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)
        >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
        >>> obj.madd(x, h)
        >>> obj.get()
        (5+0j)

        """

    def add2d(self, x: NDArray[np.complex128]) -> None:
        """Sum all elements of a (logically) 2-D complex array into the
        accumulator. The array is treated as a flat C-order buffer of ``x_len``
        complex128 samples regardless of the original shape; the caller is
        responsible for passing the total element count.

        Parameters
        ----------
        x : NDArray[np.complex128]
            Input array (complex128, any shape — passed as flat buffer).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> grid = np.array([[1+0j, 2+0j], [3+0j, 4+0j]], dtype=np.complex128)
        >>> obj.add2d(grid)
        >>> obj.get()
        (10+0j)

        """

    def madd2d(
        self,
        x: NDArray[np.complex128],
        h: NDArray[np.float32],
    ) -> None:
        """Dot-product accumulate over a flat 2-D complex buffer: ``acc +=
        sum(x[i] * h[i])`` for ``i`` in ``0 .. min(x_len, h_len) - 1``.
        Combines ``add2d`` and ``madd`` semantics for 2-D data — a complex
        signal grid is weighted element-wise by a real coefficient buffer and
        folded into the running sum.

        Parameters
        ----------
        x : NDArray[np.complex128]
            Complex signal samples (complex128, flat buffer).
        h : NDArray[np.float32]
            Real coefficient / weight array (float32).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)
        >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
        >>> obj.madd2d(x, h)
        >>> obj.get()
        (5+0j)

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the AccCf64 has already been destroyed.

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

        Raises ``RuntimeError`` if the AccCf64 has already been destroyed.

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
        ``RuntimeError`` if the AccCf64 has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """
    def get_acc(self) -> complex:
        """Return the current accumulator value without modifying state. Use
        this when you need to read the running sum mid-accumulation without
        disturbing it. For a read-and-reset in one call use ``acc_cf64_dump``.

        Returns
        -------
        complex
            Current value of ``acc`` (complex).

        Examples
        --------
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> obj.step(1+2j)
        >>> obj.get_acc()          # non-destructive read
        (1+2j)
        >>> obj.get_acc()          # still there — get_acc never drains
        (1+2j)

        """

    def set_acc(self, value: complex) -> None:
        """Overwrite the accumulator with a new complex value. Useful for
        seeding the accumulator to a known baseline before processing a new
        segment without a full ``reset``; subsequent ``step`` / ``steps``
        samples accumulate on top of the seeded value.

        Parameters
        ----------
        value : complex
            New accumulator value (complex).

        Examples
        --------
        >>> from doppler.accumulator import AccCf64
        >>> obj = AccCf64(0j)
        >>> obj.set_acc(5+6j)      # seed a complex baseline
        >>> obj.step(1+1j)         # later samples fold in on top
        >>> obj.get_acc()
        (6+7j)

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "AccCf64":
        """Enter a context manager, returning this object.

        Lets a AccCf64 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        AccCf64
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the AccCf64.

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
class AccTrace:
    """Create a length-n trace accumulator.

    Parameters
    ----------
    n : int, default 1024
        Trace length in bins. Must be > 0; returns NULL otherwise.
    mode : Literal["mean", "exp", "maxhold", "minhold"], default "mean"
        Reduction mode index (0=mean, 1=exp, 2=maxhold, 3=minhold).
    alpha : float, default 0.1
        EMA smoothing factor used only by exp mode (0 < alpha <= 1).

    Examples
    --------
    >>> from doppler.accumulator import AccTrace
    >>> acc = AccTrace(n=8, mode="mean")
    >>> acc.n, acc.count
    (8, 0)

    """
    def __init__(
        self,
        n: int = ...,
        mode: Literal["mean", "exp", "maxhold", "minhold"] = "mean",
        alpha: float = ...,
    ) -> None: ...

    def accumulate(self, p: NDArray[np.float32]) -> None:
        """Fold one length-n frame into the running trace.

        Parameters
        ----------
        p : NDArray[np.float32]
            Input frame (float32).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccTrace
        >>> acc = AccTrace(n=4, mode="mean")
        >>> acc.accumulate(np.array([1, 3, 5, 7], dtype=np.float32))
        >>> acc.accumulate(np.array([3, 5, 7, 9], dtype=np.float32))
        >>> acc.value().tolist()
        [2.0, 4.0, 6.0, 8.0]

        """

    def reset(self) -> None:
        """Discard the running trace; the next accumulate re-seeds it.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccTrace
        >>> acc = AccTrace(n=4, mode="mean")
        >>> acc.accumulate(np.ones(4, dtype=np.float32))
        >>> acc.reset()
        >>> acc.count
        0

        """

    def value(
        self,
        count: int = 1,
        out: NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """Copy the current averaged trace (None before any accumulate).

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[np.float32] | None
            Destination, at least n float32 elements.

        Returns
        -------
        NDArray[np.float32]
            min(n, max_out) samples, or 0 if empty.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.accumulator import AccTrace
        >>> acc = AccTrace(n=3, mode="maxhold")
        >>> acc.value() is None            # empty until the first frame
        True
        >>> acc.accumulate(np.array([1, 5, 2], dtype=np.float32))
        >>> acc.accumulate(np.array([4, 3, 6], dtype=np.float32))
        >>> acc.value().tolist()           # per-bin running maximum
        [4.0, 5.0, 6.0]

        """

    def value_max_out(self) -> int:
        """Output capacity hint for value(); equals the trace length n.

        Returns
        -------
        int
            Output.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the AccTrace has already been destroyed.

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

        Raises ``RuntimeError`` if the AccTrace has already been destroyed.

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
        ``RuntimeError`` if the AccTrace has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def n(self) -> int:
        """Trace length (bins)."""

    @property
    def alpha(self) -> float:
        """EMA smoothing factor (exp mode)."""
    @alpha.setter
    def alpha(self, value: float) -> None: ...

    @property
    def count(self) -> int:
        """Frames folded in so far."""

    @property
    def mode(self) -> int:
        """Reduction mode."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "AccTrace":
        """Enter a context manager, returning this object.

        Lets a AccTrace be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        AccTrace
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the AccTrace.

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
