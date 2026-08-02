# cvt/cvt.pyi — type stubs for the cvt C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class F32ToI16:
    """Create a f32_to_i16 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Multiply factor applied before rounding and saturation (default: 32768.0f).  Use 32768.0 to convert a normalised `[-1, +1]` signal to full Q15 range.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16
    >>> obj = F32ToI16(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Clear the sticky clip flag, starting a fresh saturation history.

        Zeroes clipped so a subsequent clipped query reflects only samples seen
        after this call; the immutable scale is preserved. Call it at a buffer
        or segment boundary so a saturation on one block does not leak into the
        next.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16
        >>> c = F32ToI16()
        >>> c.step(9.0)          # out of range -> saturates, latches clipped
        32767
        >>> c.reset()            # forget the clip history
        >>> c.clipped
        False

        """

    def step(self, x: float) -> int:
        """Scale one float sample by scale, round, and saturate to int16.

        Computes round(x * scale), clamps to the int16 range `[-32768, 32767]`,
        and latches the sticky clipped flag if the scaled value fell outside
        that range before clamping. At the default scale of 32768 a normalised
        `[-1, +1]` input maps to the full Q15 code range.

        Parameters
        ----------
        x : float
            Input sample, normally a normalised float in `[-1, +1]`.

        Returns
        -------
        int
            Saturated int16 code in `[-32768, 32767]`.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16
        >>> c = F32ToI16(scale=32768.0)   # normalised float -> full-scale Q15
        >>> c.step(0.5)                    # 0.5 * 32768
        16384
        >>> c.step(2.0)                    # beyond +1.0 -> saturates to int16 max
        32767
        >>> c.clipped                      # sticky flag latched by the clip
        True

        """

    def steps(self, x: NDArray[np.float32], out: NDArray[np.int16] | None = None) -> NDArray[np.int16]:
        """Process a block of float samples to int16.

        Applies step() to every element. The clipped flag is updated
        cumulatively across the block — a single saturating sample raises it for
        the entire call. Accepts an optional pre-allocated output array;
        allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.int16]
            Output.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16
        >>> import numpy as np
        >>> x = np.array([0.0, 0.5, -1.0, 0.999], dtype=np.float32)
        >>> F32ToI16().steps(x).tolist()   # default scale=32768 -> full-scale int16
        [0, 16384, -32768, 32735]

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the F32ToI16 has already been destroyed.

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

        Raises ``RuntimeError`` if the F32ToI16 has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()` before
        the blob is handed to the C core, and the core may reject it as well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the F32ToI16 has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "F32ToI16":
        """Enter a context manager, returning this object.

        Lets a F32ToI16 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        F32ToI16
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the F32ToI16.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class I16ToF32:
    """Create a i16_to_f32 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Denominator scale; 1/scale is applied to each sample (default: 32768.0f).  Use 32768.0 to recover normalised `[-1, +1]` floats from a Q15 int16 stream.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16ToF32
    >>> obj = I16ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """No-op reset, provided only for lifecycle symmetry.

        This converter carries no running state beyond the immutable iscale, so
        there is nothing to clear; the method exists so every converter in the
        module presents the same create / step / reset / destroy lifecycle.

        Examples
        --------
        >>> from doppler.cvt import I16ToF32
        >>> c = I16ToF32()
        >>> c.reset()             # stateless converter -> reset changes nothing
        >>> round(c.step(-32768), 4)
        -1.0

        """

    def step(self, x: int) -> float:
        """Convert one signed int16 sample to a normalised float via 1/scale.

        Returns (float)x * iscale, a single multiply on the hot path. No
        saturation or clipping is possible — every int16 code maps cleanly to
        float32. At the default scale of 32768 the full Q15 range recovers
        `[-1.0, ~+1.0)`, the exact inverse of F32ToI16.

        Parameters
        ----------
        x : int
            Signed int16 code, normally a Q15 sample in `[-32768, 32767]`.

        Returns
        -------
        float
            Normalised float, `x / scale`.

        Examples
        --------
        >>> from doppler.cvt import I16ToF32
        >>> c = I16ToF32(scale=32768.0)   # Q15 int16 -> normalised float
        >>> round(c.step(16384), 4)        # 16384 / 32768
        0.5
        >>> round(c.step(-32768), 4)       # full-negative code -> -1.0
        -1.0

        """

    def steps(self, x: NDArray[np.int16], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a block of int16 samples to float32.

        Applies step() to every element. Accepts an optional pre-allocated
        output array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.int16]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.

        Examples
        --------
        >>> from doppler.cvt import I16ToF32
        >>> import numpy as np
        >>> I16ToF32().steps(np.array([0, 16384, -32768], dtype=np.int16)).tolist()
        [0.0, 0.5, -1.0]

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "I16ToF32":
        """Enter a context manager, returning this object.

        Lets a I16ToF32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        I16ToF32
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the I16ToF32.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class I32ToF32:
    """Create a i32_to_f32 instance.

    Parameters
    ----------
    scale : float, default 2147483648.0
        Denominator scale; 1/scale is applied to each sample (default: 2147483648.0f).  Use 2^31 to recover normalised floats from a full-range int32 stream.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I32ToF32
    >>> obj = I32ToF32(scale=2147483648.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """No-op reset, provided only for lifecycle symmetry.

        No mutable state exists beyond the immutable iscale, so there is nothing
        to clear; the method exists so every converter in the module presents
        the same create / step / reset / destroy lifecycle.

        Examples
        --------
        >>> from doppler.cvt import I32ToF32
        >>> c = I32ToF32()
        >>> c.reset()             # stateless converter -> reset changes nothing
        >>> round(c.step(-2**31), 4)
        -1.0

        """

    def step(self, x: int) -> float:
        """Convert one signed int32 sample to a normalised float via 1/scale.

        Returns (float)x * iscale, a single multiply on the hot path. At the
        default scale of 2^31 the full int32 range recovers `[-1.0, ~+1.0)`.
        Note that float32 carries only 23 mantissa bits, so int32 magnitudes
        beyond 2^24 are rounded to the nearest representable float.

        Parameters
        ----------
        x : int
            Signed int32 code, normally a full-range fixed-point sample.

        Returns
        -------
        float
            Normalised float, `x / scale`.

        Examples
        --------
        >>> from doppler.cvt import I32ToF32
        >>> c = I32ToF32(scale=2147483648.0)  # 2**31: full-range int32 -> [-1, 1)
        >>> round(c.step(2**30), 4)            # quarter-scale code -> 0.5
        0.5
        >>> round(c.step(-2**31), 4)           # full-negative code -> -1.0
        -1.0

        """

    def steps(self, x: NDArray[np.int32], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a block of int32 samples to float32.

        Applies step() to every element. Accepts an optional pre-allocated
        output array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.int32]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.

        Examples
        --------
        >>> from doppler.cvt import I32ToF32
        >>> import numpy as np
        >>> I32ToF32().steps(np.array([0, 2**30, -2**31], dtype=np.int32)).tolist()
        [0.0, 0.5, -1.0]

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "I32ToF32":
        """Enter a context manager, returning this object.

        Lets a I32ToF32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        I32ToF32
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the I32ToF32.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class I8ToF32:
    """Create a i8_to_f32 instance.

    Parameters
    ----------
    scale : float, default 128.0
        Denominator scale; 1/scale is applied to each sample (default: 128.0f).  Use 128.0 to recover normalised floats from a signed 8-bit stream.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I8ToF32
    >>> obj = I8ToF32(scale=128.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """No-op reset, provided only for lifecycle symmetry.

        No mutable state exists beyond the immutable iscale, so there is nothing
        to clear; the method exists so every converter in the module presents
        the same create / step / reset / destroy lifecycle.

        Examples
        --------
        >>> from doppler.cvt import I8ToF32
        >>> c = I8ToF32()
        >>> c.reset()             # stateless converter -> reset changes nothing
        >>> round(c.step(-128), 4)
        -1.0

        """

    def step(self, x: int) -> float:
        """Convert one signed int8 sample to a normalised float via 1/scale.

        Returns (float)x * iscale, a single multiply on the hot path. At the
        default scale of 128 the full int8 range recovers `[-1.0, ~+1.0)` — the
        front end of an 8-bit IQ path (e.g. a signed-8 RTL-SDR stream) into
        normalised floats.

        Parameters
        ----------
        x : int
            Signed int8 code in `[-128, 127]`.

        Returns
        -------
        float
            Normalised float, `x / scale`.

        Examples
        --------
        >>> from doppler.cvt import I8ToF32
        >>> c = I8ToF32(scale=128.0)   # signed 8-bit -> normalised float
        >>> round(c.step(64), 4)        # 64 / 128
        0.5
        >>> round(c.step(-128), 4)      # full-negative code -> -1.0
        -1.0

        """

    def steps(self, x: NDArray[np.int8], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a block of int8 samples to float32.

        Applies step() to every element. Accepts an optional pre-allocated
        output array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.int8]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.

        Examples
        --------
        >>> from doppler.cvt import I8ToF32
        >>> import numpy as np
        >>> I8ToF32().steps(np.array([0, 64, -128], dtype=np.int8)).tolist()
        [0.0, 0.5, -1.0]

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "I8ToF32":
        """Enter a context manager, returning this object.

        Lets a I8ToF32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        I8ToF32
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the I8ToF32.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class F32ToI16U32:
    """Create a f32_to_i16u32 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Multiply factor applied before quantisation and saturation (default: 32768.0f).  Use 32768.0 to convert normalised `[-1, +1]` samples to Q15 packed into a uint32.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16U32
    >>> obj = F32ToI16U32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Clear the sticky clip flag, starting a fresh saturation history.

        Zeroes clipped so a subsequent clipped query reflects only samples seen
        after this call; the immutable scale is preserved. Call it at a buffer
        or segment boundary so a saturation on one block does not leak into the
        next.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16U32
        >>> c = F32ToI16U32()
        >>> c.step(5.0)          # out of range -> saturates, latches clipped
        32767
        >>> c.reset()            # forget the clip history
        >>> c.clipped
        False

        """

    def step(self, x: float) -> int:
        """Scale one float sample to a saturated Q15 code packed in a uint32.

        Computes round(x * scale), saturates to `[-32768, 32767]`, then
        zero-extends the 16-bit two's-complement pattern into the lower 16 bits
        of a uint32 (upper 16 bits are always zero — headroom for the CIC
        integrator cascade). Latches the sticky clipped flag on saturation.

        Parameters
        ----------
        x : float
            Input sample, normally a normalised float in `[-1, +1]`.

        Returns
        -------
        int
            Q15 code in the low 16 bits of a uint32; e.g. -32768 -> 0x8000.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16U32
        >>> c = F32ToI16U32(scale=32768.0)
        >>> c.step(0.5)              # 0.5 -> Q15 16384, upper 16 bits zero
        16384
        >>> hex(c.step(-1.0))        # -32768 as an unsigned low-16 pattern
        '0x8000'

        """

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint32] | None = None) -> NDArray[np.uint32]:
        """Process a block of float samples to Q15-in-uint32.

        Applies step() to every element. The clipped flag is updated
        cumulatively across the block. Accepts an optional pre-allocated output
        array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.uint32]
            Output.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16U32
        >>> import numpy as np
        >>> F32ToI16U32().steps(np.array([0.0, 0.5], dtype=np.float32)).tolist()
        [0, 16384]

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the F32ToI16U32 has already been destroyed.

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

        Raises ``RuntimeError`` if the F32ToI16U32 has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()` before
        the blob is handed to the C core, and the core may reject it as well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the F32ToI16U32 has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "F32ToI16U32":
        """Enter a context manager, returning this object.

        Lets a F32ToI16U32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        F32ToI16U32
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the F32ToI16U32.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class F32ToI16U64:
    """Create a f32_to_i16u64 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Multiply factor applied before quantisation and saturation (default: 32768.0f).  Use 32768.0 to convert normalised `[-1, +1]` samples to Q15 packed into the low 16 bits of a uint64.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16U64
    >>> obj = F32ToI16U64(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Clear the sticky clip flag, starting a fresh saturation history.

        Zeroes clipped so a subsequent clipped query reflects only samples seen
        after this call; the immutable scale is preserved. Call it at a buffer
        or segment boundary so a saturation on one block does not leak into the
        next.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16U64
        >>> c = F32ToI16U64()
        >>> c.step(5.0)          # out of range -> saturates, latches clipped
        32767
        >>> c.reset()            # forget the clip history
        >>> c.clipped
        False

        """

    def step(self, x: float) -> int:
        """Scale one float sample to a saturated Q15 code packed in a uint64.

        Computes round(x * scale), saturates to `[-32768, 32767]`, then
        zero-extends the 16-bit two's-complement pattern into the lower 16 bits
        of a uint64 (upper 48 bits are always zero — headroom for the NCO phase
        accumulator). Latches the sticky clipped flag on saturation.

        Parameters
        ----------
        x : float
            Input sample, normally a normalised float in `[-1, +1]`.

        Returns
        -------
        int
            Q15 code in the low 16 bits of a uint64; e.g. -32768 -> 0x8000.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16U64
        >>> c = F32ToI16U64(scale=32768.0)
        >>> c.step(0.5)              # 0.5 -> Q15 16384, upper 48 bits zero
        16384
        >>> hex(c.step(-1.0))        # -32768 as an unsigned low-16 pattern
        '0x8000'

        """

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint64] | None = None) -> NDArray[np.uint64]:
        """Process a block of float samples to Q15-in-uint64.

        Applies step() to every element. The clipped flag is updated
        cumulatively across the block. Accepts an optional pre-allocated output
        array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.uint64]
            Output.

        Examples
        --------
        >>> from doppler.cvt import F32ToI16U64
        >>> import numpy as np
        >>> F32ToI16U64().steps(np.array([0.0, 0.5], dtype=np.float32)).tolist()
        [0, 16384]

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the F32ToI16U64 has already been destroyed.

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

        Raises ``RuntimeError`` if the F32ToI16U64 has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()` before
        the blob is handed to the C core, and the core may reject it as well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the F32ToI16U64 has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "F32ToI16U64":
        """Enter a context manager, returning this object.

        Lets a F32ToI16U64 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        F32ToI16U64
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the F32ToI16U64.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class I16U32ToF32:
    """Create a i16u32_to_f32 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Denominator scale; 1/scale is applied after sign-extension (default: 32768.0f).  Use 32768.0 to match F32ToI16U32 at its default scale.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16U32ToF32
    >>> obj = I16U32ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """No-op reset, provided only for lifecycle symmetry.

        No mutable state exists beyond the immutable iscale, so there is nothing
        to clear; the method exists so every converter in the module presents
        the same create / step / reset / destroy lifecycle.

        Examples
        --------
        >>> from doppler.cvt import I16U32ToF32
        >>> c = I16U32ToF32()
        >>> c.reset()             # stateless converter -> reset changes nothing
        >>> round(c.step(16384), 4)
        0.5

        """

    def step(self, x: int) -> float:
        """Unpack a Q15 code from a uint32's low 16 bits to a normalised float.

        Masks off the lower 16 bits, reinterprets them as a signed int16 (two's
        complement), then multiplies by iscale — a single multiply after the
        extraction. The upper 16 bits (which may carry CIC bit-growth headroom)
        are ignored. Exact inverse of F32ToI16U32 at the same scale.

        Parameters
        ----------
        x : int
            uint32 carrying a Q15 code in its low 16 bits.

        Returns
        -------
        float
            Normalised float recovered from the low-16 Q15 code.

        Examples
        --------
        >>> from doppler.cvt import I16U32ToF32
        >>> c = I16U32ToF32(scale=32768.0)
        >>> round(c.step(16384), 4)         # low-16 Q15 16384 -> 0.5
        0.5
        >>> round(c.step(0x8000), 4)        # 0x8000 reinterpreted as -32768 -> -1.0
        -1.0

        """

    def steps(self, x: NDArray[np.uint32], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a block of Q15-in-uint32 samples to float32.

        Applies step() to every element. Accepts an optional pre-allocated
        output array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.uint32]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.

        Examples
        --------
        >>> from doppler.cvt import I16U32ToF32
        >>> import numpy as np
        >>> I16U32ToF32().steps(np.array([0, 16384], dtype=np.uint32)).tolist()
        [0.0, 0.5]

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "I16U32ToF32":
        """Enter a context manager, returning this object.

        Lets a I16U32ToF32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        I16U32ToF32
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the I16U32ToF32.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class I16U64ToF32:
    """Create a i16u64_to_f32 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Denominator scale; 1/scale is applied after sign-extension (default: 32768.0f).  Use 32768.0 to match F32ToI16U64 at its default scale.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16U64ToF32
    >>> obj = I16U64ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """No-op reset, provided only for lifecycle symmetry.

        No mutable state exists beyond the immutable iscale, so there is nothing
        to clear; the method exists so every converter in the module presents
        the same create / step / reset / destroy lifecycle.

        Examples
        --------
        >>> from doppler.cvt import I16U64ToF32
        >>> c = I16U64ToF32()
        >>> c.reset()             # stateless converter -> reset changes nothing
        >>> round(c.step(16384), 4)
        0.5

        """

    def step(self, x: int) -> float:
        """Unpack a Q15 code from a uint64's low 16 bits to a normalised float.

        Masks off the lower 16 bits, reinterprets them as a signed int16 (two's
        complement), then multiplies by iscale — a single multiply after the
        extraction. The upper 48 bits (which may carry NCO phase-accumulator
        headroom) are ignored. Exact inverse of F32ToI16U64 at the same scale.

        Parameters
        ----------
        x : int
            uint64 carrying a Q15 code in its low 16 bits.

        Returns
        -------
        float
            Normalised float recovered from the low-16 Q15 code.

        Examples
        --------
        >>> from doppler.cvt import I16U64ToF32
        >>> c = I16U64ToF32(scale=32768.0)
        >>> round(c.step(16384), 4)         # low-16 Q15 16384 -> 0.5
        0.5
        >>> round(c.step(0x8000), 4)        # 0x8000 reinterpreted as -32768 -> -1.0
        -1.0

        """

    def steps(self, x: NDArray[np.uint64], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a block of Q15-in-uint64 samples to float32.

        Applies step() to every element. Accepts an optional pre-allocated
        output array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.uint64]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.

        Examples
        --------
        >>> from doppler.cvt import I16U64ToF32
        >>> import numpy as np
        >>> I16U64ToF32().steps(np.array([0, 16384], dtype=np.uint64)).tolist()
        [0.0, 0.5]

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "I16U64ToF32":
        """Enter a context manager, returning this object.

        Lets a I16U64ToF32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        I16U64ToF32
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the I16U64ToF32.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class F32ToUQ15:
    """Create a f32_to_uq15 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Multiply factor applied before quantisation and saturation (default: 32768.0f).  Use 32768.0 to convert normalised `[-1, +1]` floats to the full UQ15 range `[0, 65535]`. Must be > 0; returns NULL otherwise.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToUQ15
    >>> obj = F32ToUQ15(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Clear the sticky clip flag, starting a fresh saturation history.

        Zeroes clipped so a subsequent clipped query reflects only samples seen
        after this call; the immutable scale is preserved. Call it at a buffer
        or segment boundary so a saturation on one block does not leak into the
        next.

        Examples
        --------
        >>> from doppler.cvt import F32ToUQ15
        >>> c = F32ToUQ15()
        >>> c.step(2.0)          # out of range -> saturates to 0xFFFF, latches clip
        65535
        >>> c.reset()            # forget the clip history
        >>> c.clipped
        False

        """

    def step(self, x: float) -> int:
        """Scale one float sample to an offset-binary UQ15 uint16 code.

        Computes round(x * scale), clamps to `[-32768, 32767]`, then adds the
        32768 offset-binary bias so the signed float domain maps onto the full
        unsigned uint16 range. Latches the sticky clipped flag if the scaled
        value saturated before clamping. Suits DAC and file formats that store
        only unsigned integers.

        Parameters
        ----------
        x : float
            Input sample, normally a normalised float in `[-1, +1]`.

        Returns
        -------
        int
            Offset-binary uint16 in `[0, 65535]`: -1.0 -> 0, 0.0 -> 32768, +1.0
            -> 65535.

        Examples
        --------
        >>> from doppler.cvt import F32ToUQ15
        >>> c = F32ToUQ15(scale=32768.0)
        >>> c.step(0.0)          # midscale maps to the offset-binary bias
        32768
        >>> c.step(-1.0)         # full-negative maps to code 0
        0

        """

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint16] | None = None) -> NDArray[np.uint16]:
        """Process a block of float samples to UQ15 uint16.

        Applies step() to every element. The clipped flag is updated
        cumulatively across the block. Accepts an optional pre-allocated output
        array; allocates a fresh one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.uint16]
            Output.

        Examples
        --------
        >>> from doppler.cvt import F32ToUQ15
        >>> import numpy as np
        >>> F32ToUQ15().steps(np.array([-1.0, 0.0, 0.999], dtype=np.float32)).tolist()
        [0, 32768, 65503]

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the F32ToUQ15 has already been destroyed.

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

        Raises ``RuntimeError`` if the F32ToUQ15 has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()` before
        the blob is handed to the C core, and the core may reject it as well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the F32ToUQ15 has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "F32ToUQ15":
        """Enter a context manager, returning this object.

        Lets a F32ToUQ15 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        F32ToUQ15
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the F32ToUQ15.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class UQ15ToF32:
    """Create a uq15_to_f32 instance.

    Parameters
    ----------
    scale : float, default 32768.0
        Denominator applied after offset-binary bias removal (default: 32768.0f).  Use 32768.0 to recover normalised `[-1, +1]` floats from UQ15 data written by F32ToUQ15. Must be > 0; returns NULL otherwise.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import UQ15ToF32
    >>> obj = UQ15ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """No-op reset, provided only for lifecycle symmetry.

        No mutable state exists beyond the immutable iscale, so there is nothing
        to clear; the method exists so every converter in the module presents
        the same create / step / reset / destroy lifecycle.

        Examples
        --------
        >>> from doppler.cvt import UQ15ToF32
        >>> c = UQ15ToF32()
        >>> c.reset()             # stateless converter -> reset changes nothing
        >>> round(c.step(32768), 4)
        0.0

        """

    def step(self, x: int) -> float:
        """Decode one offset-binary UQ15 uint16 code to a normalised float.

        Computes ((int32_t)x - 32768) * iscale — removes the 32768 offset-binary
        bias and applies 1/scale. The int32_t cast prevents signed overflow when
        x is 0 (which yields -32768 after bias removal). Exact inverse of
        F32ToUQ15 at the same scale.

        Parameters
        ----------
        x : int
            UQ15 offset-binary uint16 code: 0 -> -1.0, 32768 -> 0.0, 65535 ->
            +32767/32768.

        Returns
        -------
        float
            Normalised float in `[-1.0, ~+1.0)`.

        Examples
        --------
        >>> from doppler.cvt import UQ15ToF32
        >>> c = UQ15ToF32(scale=32768.0)
        >>> round(c.step(32768), 4)   # midscale code -> 0.0
        0.0
        >>> round(c.step(0), 4)       # zero code -> -1.0
        -1.0

        """

    def steps(self, x: NDArray[np.uint16], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a block of UQ15 samples to float32.

        Applies step() to every element. State is not mutated (no clipped flag).
        Accepts an optional pre-allocated output array; allocates a fresh one
        when output is NULL.

        Parameters
        ----------
        x : NDArray[np.uint16]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.

        Examples
        --------
        >>> from doppler.cvt import UQ15ToF32
        >>> import numpy as np
        >>> UQ15ToF32().steps(np.array([0, 32768], dtype=np.uint16)).tolist()
        [-1.0, 0.0]

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "UQ15ToF32":
        """Enter a context manager, returning this object.

        Lets a UQ15ToF32 be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        UQ15ToF32
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the UQ15ToF32.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

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
class ADC:
    """Create an ADC instance.

    Parameters
    ----------
    bits : int, default 16
        ADC resolution in bits (1..64).
    dbfs : float, default -10.0
        Full-scale reference level in dBFS (typically negative, e.g. -10.0).  A signal with amplitude 10^(dbfs/20) fills the converter's integer range exactly.
    dithering : int, default 0
        0 = no dither; non-zero = TPDF dither before rounding.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import ADC
    >>> obj = ADC(bits=16, dbfs=-10.0, dithering=0)

    """
    def __init__(self, bits: int = ..., dbfs: float = ..., dithering: int = ...) -> None: ...

    def reset(self) -> None:
        """Clear the clip flag and re-seed the dither PRNG for a reproducible run.

        Zeroes the sticky clipped flag and re-seeds the xorshift32 dither PRNG
        to its fixed initial value, so a dithered capture restarted after
        reset() is bit-for-bit reproducible. The immutable configuration (bits,
        scale, clip bounds) is preserved.

        Examples
        --------
        >>> from doppler.cvt import ADC
        >>> adc = ADC(bits=8, dbfs=0.0, dithering=0)
        >>> adc.step(9.0)            # beyond full scale -> saturates, latches clip
        127
        >>> adc.reset()             # clear clip history and re-seed the dither PRNG
        >>> adc.clipped
        False

        """

    def step(self, x: float) -> int:
        """Quantise one float sample to a signed N-bit ADC code.

        Multiplies x by the pre-computed double-precision scale, optionally adds
        TPDF dither (when the object was built with dithering enabled), rounds
        with llround, and clamps to the signed integer range `[clip_min,
        clip_max]`. Latches the sticky clipped flag if the sample saturated. A
        sample at amplitude 10^(dbfs/20) reaches full scale.

        Parameters
        ----------
        x : float
            Input sample, normally a normalised float in `[-1, +1]`.

        Returns
        -------
        int
            Signed ADC code in `[-(2^(bits-1)), 2^(bits-1)-1]`.

        Examples
        --------
        >>> from doppler.cvt import ADC
        >>> adc = ADC(bits=8, dbfs=0.0, dithering=0)  # 8-bit, full scale at 0 dBFS
        >>> adc.step(0.5)            # 0.5 * 128 codes
        64
        >>> adc.step(2.0)            # beyond full scale -> clamps to +127
        127
        >>> adc.clipped              # sticky flag latched by the clamp
        True

        """

    def steps(self, x: NDArray[np.float32], out: NDArray[np.int64] | None = None) -> NDArray[np.int64]:
        """Process a block of float samples to int64.

        When dithering is disabled the float-to-double multiply can use SIMD
        widening (jm_simd.h); the int64_t conversion and clamp remain scalar.
        When dithering is enabled the loop is scalar to preserve sequential PRNG
        state. Accepts an optional pre-allocated output array; allocates a fresh
        one when output is NULL.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.int64]
            Output.

        Examples
        --------
        >>> from doppler.cvt import ADC
        >>> import numpy as np
        >>> # ideal 12-bit ADC: full scale spans +-2**11 codes
        >>> ADC(12, 0.0, 0).steps(np.array([0.0, 0.5, 0.999, -1.0],
        ...                                dtype=np.float32)).tolist()
        [0, 1024, 2046, -2048]

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the ADC has already been destroyed.

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

        Raises ``RuntimeError`` if the ADC has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()` before
        the blob is handed to the C core, and the core may reject it as well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the ADC has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def clipped(self) -> bool:
        """Clipped."""

    @property
    def scale(self) -> float:
        """Scale."""

    @property
    def bits(self) -> int:
        """Bits."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "ADC":
        """Enter a context manager, returning this object.

        Lets a ADC be used in a `with` statement so its C resources are released
        deterministically on exit rather than at collection time.

        Returns
        -------
        ADC
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the ADC.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """
