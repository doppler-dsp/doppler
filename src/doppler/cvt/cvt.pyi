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
    >>> obj = F32ToI16(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32], out: NDArray[np.int16] | None = None) -> NDArray[np.int16]:
        """Process a samples array."""

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
    >>> obj = I16ToF32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

    def steps(self, x: NDArray[np.int16], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "I16ToF32": ...

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
    >>> obj = F32ToI16U32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint32] | None = None) -> NDArray[np.uint32]:
        """Process a samples array."""

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
    >>> obj = F32ToI16U64(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint64] | None = None) -> NDArray[np.uint64]:
        """Process a samples array."""

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
    >>> obj = I16U32ToF32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

    def steps(self, x: NDArray[np.uint32], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a samples array."""

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
    >>> obj = I16U64ToF32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

    def steps(self, x: NDArray[np.uint64], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "I16U64ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class F32ToUQ15:
    """F32ToUQ15 component.

    Converts normalised float samples to UQ15 offset-binary uint16.  The Q15
    quantised value is shifted by +32768 so that -1.0 → 0, 0.0 → 32768,
    and +0.99997 → 65535.

    This is the inverse of :class:`UQ15ToF32`.

    Parameters
    ----------
    scale : float, default 32768.0
        Multiply factor applied before saturation and quantisation.

    Examples
    --------
    Offset-binary encode and check the clipped flag:

    >>> from doppler.cvt import F32ToUQ15
    >>> enc = F32ToUQ15()
    >>> enc.step(0.0)
    32768
    >>> enc.step(-1.0)
    0
    >>> enc.clipped
    False
    >>> enc.step(1.2)
    65535
    >>> enc.clipped
    True

    """
    def __init__(self, scale: float = ...) -> None: ...

    clipped: bool
    """Sticky saturation flag.

    Set to ``True`` by the first sample whose pre-saturation scaled value
    falls outside ``[-32768, 32767]``; cleared only by :meth:`reset`.
    Read-only.
    """

    def reset(self) -> None:
        """Reset state to post-create defaults (clears ``clipped``)."""

    def step(self, x: float) -> int:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint16] | None = None) -> NDArray[np.uint16]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToUQ15": ...

    def __exit__(self, *args: object) -> None: ...

class UQ15ToF32:
    """UQ15ToF32 component.

    Decodes UQ15 offset-binary uint16 samples back to normalised floats:
    0 → -1.0, 32768 → 0.0, 65535 → +32767/32768 ≈ +0.99997.

    This is the inverse of :class:`F32ToUQ15`.

    Parameters
    ----------
    scale : float, default 32768.0
        Denominator applied after bias removal.  The default maps the full
        UQ15 range [0, 65535] to approximately [-1, +1].

    Examples
    --------
    Roundtrip through F32ToUQ15 and UQ15ToF32:

    >>> import numpy as np
    >>> from doppler.cvt import F32ToUQ15, UQ15ToF32
    >>> x = np.array([0.5, 0.0, -0.5], dtype=np.float32)
    >>> q = F32ToUQ15().steps(x)
    >>> x_hat = UQ15ToF32().steps(q)
    >>> float(np.max(np.abs(x - x_hat))) < 2e-5
    True

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

    def steps(self, x: NDArray[np.uint16], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "UQ15ToF32": ...

    def __exit__(self, *args: object) -> None: ...
