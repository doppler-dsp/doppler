# cvt/cvt.pyi — type stubs for the cvt C extension.
import numpy as np
from numpy.typing import NDArray

class F32ToI16:
    """F32ToI16 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16
    >>> obj = F32ToI16(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

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
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToI16": ...

    def __exit__(self, *args: object) -> None: ...

class I16ToF32:
    """I16ToF32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16ToF32
    >>> obj = I16ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "I16ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class I32ToF32:
    """I32ToF32 component.

    Parameters
    ----------
    scale : float, default 2147483648.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I32ToF32
    >>> obj = I32ToF32(scale=2147483648.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset I32ToF32 to its post-create state.

        No mutable state exists beyond the immutable iscale; reset is a no-op
        provided for lifecycle symmetry with other converters.
        """

    def step(self, x: int) -> float:
        """Process one input sample."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "I32ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class I8ToF32:
    """I8ToF32 component.

    Parameters
    ----------
    scale : float, default 128.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I8ToF32
    >>> obj = I8ToF32(scale=128.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset I8ToF32 to its post-create state.

        No mutable state exists beyond the immutable iscale; reset is a no-op
        provided for lifecycle symmetry with other converters.
        """

    def step(self, x: int) -> float:
        """Process one input sample."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "I8ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class F32ToI16U32:
    """F32ToI16U32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16U32
    >>> obj = F32ToI16U32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

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
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToI16U32": ...

    def __exit__(self, *args: object) -> None: ...

class F32ToI16U64:
    """F32ToI16U64 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16U64
    >>> obj = F32ToI16U64(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

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
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToI16U64": ...

    def __exit__(self, *args: object) -> None: ...

class I16U32ToF32:
    """I16U32ToF32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16U32ToF32
    >>> obj = I16U32ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "I16U32ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class I16U64ToF32:
    """I16U64ToF32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16U64ToF32
    >>> obj = I16U64ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "I16U64ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class F32ToUQ15:
    """F32ToUQ15 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToUQ15
    >>> obj = F32ToUQ15(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

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
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def clipped(self) -> bool:
        """True if any sample has been saturated since the last reset()."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToUQ15": ...

    def __exit__(self, *args: object) -> None: ...

class UQ15ToF32:
    """UQ15ToF32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import UQ15ToF32
    >>> obj = UQ15ToF32(scale=32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "UQ15ToF32": ...

    def __exit__(self, *args: object) -> None: ...

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
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

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
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "ADC": ...

    def __exit__(self, *args: object) -> None: ...
